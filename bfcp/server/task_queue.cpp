#include <bfcp/server/task_queue.h>

#include <muduo/base/Logging.h>

namespace bfcp
{

TaskQueue::TaskQueue( ThreadPool *runner, size_t maxQueueSize )
  : runner_(CHECK_NOTNULL(runner)),
    maxQueueSize_(maxQueueSize),
    mutex_(),
    notFull_(maxQueueSize > 0 ? new muduo::Condition(mutex_) : nullptr),
    isInGlobal_(false),
    isReleasing_(false)
{
}

void TaskQueue::put( Task &&action, ThreadPool::Priority priority )
{
  muduo::MutexLockGuard lock(mutex_);
  while (isFull())
  {
    notFull_->wait();
  }
  assert(!isFull());
  if (priority == ThreadPool::kHighPriority)
  { 
    highPriorityTasks_.emplace_back(std::move(action));
  }
  else
  {
    normalPriorityTasks_.emplace_back(std::move(action));
  }
}

void TaskQueue::put( const Task &action, ThreadPool::Priority priority )
{
  muduo::MutexLockGuard lock(mutex_);
  while (isFull())
  {
    notFull_->wait();
  }
  assert(!isFull());
  if (priority == ThreadPool::kHighPriority)
  {
    highPriorityTasks_.emplace_back(action);
  }
  else
  {
    normalPriorityTasks_.emplace_back(action);
  }
}

TaskQueue::Task TaskQueue::take()
{
  Task task;

  muduo::MutexLockGuard lock(mutex_);
  if (!highPriorityTasks_.empty())
  {
    task.swap(highPriorityTasks_.front());
    highPriorityTasks_.pop_front();
    if (maxQueueSize_ > 0)
    {
      notFull_->notify();
    }
  }
  else if (!normalPriorityTasks_.empty())
  {
    task.swap(normalPriorityTasks_.front());
    normalPriorityTasks_.pop_front();
    if (maxQueueSize_ > 0)
    {
      notFull_->notify();
    }
  }

  return task;
}

size_t TaskQueue::size()
{
  muduo::MutexLockGuard lock(mutex_);
  return highPriorityTasks_.size() + normalPriorityTasks_.size();
}

bool TaskQueue::empty()
{
  muduo::MutexLockGuard lock(mutex_);
  return highPriorityTasks_.empty() && normalPriorityTasks_.empty();
}

bool TaskQueue::isFull()
{
  mutex_.assertLocked();
  if (maxQueueSize_ > 0)
  {
    size_t queueSize = 
      highPriorityTasks_.size() + normalPriorityTasks_.size();
    if (maxQueueSize_ <= queueSize)
    {
      return true;
    }
  }
  return false;
}


} // namespace bfcp


