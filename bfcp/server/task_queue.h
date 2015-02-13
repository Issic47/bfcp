#ifndef BFCP_TASK_QUEUE_H
#define BFCP_TASK_QUEUE_H

#include <deque>

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>
#include <muduo/base/Mutex.h>
#include <muduo/base/Condition.h>

#include <bfcp/server/thread_pool.h>

namespace bfcp
{

class TaskQueue : boost::noncopyable
{
public:
  typedef ThreadPool::Task Task;

  explicit TaskQueue(ThreadPool *runner, size_t maxQueueSize);

  void setInGlobal(bool inGlobal) { isInGlobal_ = inGlobal; }
  bool isInGlobal() const { return isInGlobal_; }

  bool isReleasing() const { return isReleasing_; }
  void markRelease() { isReleasing_ = true; }

  void put(Task &&task, ThreadPool::Priority priority);
  void put(const Task &task, ThreadPool::Priority priority);
  Task take();

  size_t size();
  bool empty();
  
private:
  bool isFull();

  ThreadPool *runner_;
  size_t maxQueueSize_;
  muduo::MutexLock mutex_;
  boost::scoped_ptr<muduo::Condition> notFull_;
  std::deque<Task> highPriorityTasks_;
  std::deque<Task> normalPriorityTasks_;
  bool isInGlobal_;
  bool isReleasing_;
};

} // namespace bfcp

#endif // BFCP_TASK_QUEUE_H