#include <bfcp/server/thread_pool.h>

#include <boost/bind.hpp>
#include <boost/make_shared.hpp>
#include <muduo/base/Exception.h>
#include <bfcp/server/task_queue.h>

namespace bfcp
{

ThreadPool::ThreadPool( const string &name /*= string("ThreadPool")*/ )
    : mutex_(),
      notEmpty_(mutex_),
      name_(name),
      running_(false)
{
}

ThreadPool::~ThreadPool()
{
  if (running_)
  {
    stop();
  }
}

int ThreadPool::createQueue( uint32_t queueID, size_t maxQueueSize )
{
  auto lb = queueMap_.lower_bound(queueID);
  if (lb == queueMap_.end() || (*lb).first != queueID) // not found
  { 
    auto it = queueMap_.emplace_hint(lb, 
      std::make_pair(
        queueID, 
        boost::make_shared<TaskQueue>(this, maxQueueSize)));
    // FIXME: check (*it).first == handle
    return 0;
  }
  return -1;
}

int ThreadPool::releaseQueue( uint32_t queueID )
{
  auto it = queueMap_.find(queueID);
  if (it != queueMap_.end())
  {
    (*it).second->markRelease();
    queueMap_.erase(it);
  }
  return -1;
}

void ThreadPool::start( int numThreads )
{
  assert(threads_.empty());
  running_ = true;
  threads_.reserve(numThreads);
  for (int i = 0; i < numThreads; ++i)
  {
    char id[32];
    snprintf(id, sizeof id, "%d", i + 1);
    threads_.push_back(new muduo::Thread(
      boost::bind(&ThreadPool::runInThread, this), name_ + id));
    threads_[i].start();
  }
  if (numThreads == 0 && threadInitCallback_)
  {
    threadInitCallback_();
  }
}

void ThreadPool::stop()
{
  {
    muduo::MutexLockGuard lock(mutex_);
    running_ = false;
    notEmpty_.notifyAll();
  }
  for (auto &thread : threads_)
  {
    thread.join();
  }
}

int ThreadPool::run( uint32_t queueID, const Task &task, Priority priority )
{
  if (threads_.empty())
  {
    task();
  }
  else
  {
    auto it = queueMap_.find(queueID);
    if (it == queueMap_.end())
    {
      return -1;
    }
    (*it).second->put(task, priority);
    put((*it).second);
  }
  return 0;
}

int ThreadPool::run( uint32_t queueID, Task &&task, Priority priority )
{
  if (threads_.empty())
  {
    task();
  }
  else
  {
    auto it = queueMap_.find(queueID);
    if (it == queueMap_.end())
    {
      return -1;
    }
    (*it).second->put(std::move(task), priority);
    put((*it).second);
  }
  return 0;
}

void ThreadPool::runInThread()
{
  try
  {
    if (threadInitCallback_)
    {
      threadInitCallback_();
    }
    while (running_)
    {
      auto taskQueue = take();
      if (taskQueue)
      {
        Task task(taskQueue->take());
        if (task)
        {
          task();
        }
        put(taskQueue);
      }
    }
  }
  catch (const muduo::Exception& ex)
  {
    fprintf(stderr, "exception caught in ThreadPool %s\n", name_.c_str());
    fprintf(stderr, "reason: %s\n", ex.what());
    fprintf(stderr, "stack trace: %s\n", ex.stackTrace());
    abort();
  }
  catch (const std::exception& ex)
  {
    fprintf(stderr, "exception caught in ThreadPool %s\n", name_.c_str());
    fprintf(stderr, "reason: %s\n", ex.what());
    abort();
  }
  catch (...)
  {
    fprintf(stderr, "unknown exception caught in ThreadPool %s\n", name_.c_str());
    throw; // rethrow
  }
}

TaskQueuePtr ThreadPool::take()
{
  muduo::MutexLockGuard lock(mutex_);
  while (globalQueue_.empty() && running_)
  {
    notEmpty_.wait();
  }

  TaskQueuePtr taskQueue;
  if (!globalQueue_.empty())
  {
    taskQueue = globalQueue_.front();
    taskQueue->setInGlobal(false);
    globalQueue_.pop_front();
  }
  return taskQueue;
}

void ThreadPool::put( const TaskQueuePtr &taskQueue )
{
  muduo::MutexLockGuard lock(mutex_);
  if (taskQueue->isReleasing() || 
      taskQueue->isInGlobal() || 
      taskQueue->empty())
  {
    return;
  }
  taskQueue->setInGlobal(true);
  globalQueue_.push_back(taskQueue);
}

} // namespace bfcp

