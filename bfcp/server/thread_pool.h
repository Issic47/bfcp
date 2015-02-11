#ifndef BFCP_THREAD_POOL_H
#define BFCP_THREAD_POOL_H

#include <map>
#include <deque>

#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

#include <muduo/base/Types.h>
#include <muduo/base/Mutex.h>
#include <muduo/base/Condition.h>
#include <muduo/base/Thread.h>

#include <bfcp/common/bfcp_param.h>

namespace bfcp
{

class TaskQueue;
typedef boost::shared_ptr<TaskQueue> TaskQueuePtr;

class ThreadPool : boost::noncopyable
{
public:
  typedef boost::function<void ()> Task;

  enum Priority
  {
    kNormalPriority = 1,
    kHighPriority = 2,
  };

public:
  explicit ThreadPool(const string &name = string("ThreadPool"));
  ~ThreadPool(); 

  // WARN: the following methods are not thread safe
  
  void setThreadInitCallback(const Task &cb)
  { threadInitCallback_ = cb; }
  void setThreadInitCallback(Task &&cb)
  { threadInitCallback_ = std::move(cb); }
  
  int createQueue(uint32_t queueID, size_t maxQueueSize);
  int releaseQueue(uint32_t queueID);
  
  void start(int numThreads);
  void stop();
  
  int run(uint32_t queueID, const Task &task, Priority priority);
  int run(uint32_t queueID, Task &&task, Priority priority);

private:
  void runInThread();
  TaskQueuePtr take();
  void put(const TaskQueuePtr &taskQueue);

  muduo::MutexLock mutex_;
  muduo::Condition notEmpty_;
  string name_;
  Task threadInitCallback_;
  
  boost::ptr_vector<muduo::Thread> threads_;
  std::map<uint32_t, TaskQueuePtr> queueMap_;
  std::deque<TaskQueuePtr> globalQueue_;
  
  bool running_;
};


} // namespace bfcp

#endif // BFCP_THREAD_POOL_H