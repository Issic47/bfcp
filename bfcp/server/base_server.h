#ifndef BFCP_BASE_SERVER_H
#define BFCP_BASE_SERVER_H

#include <map>

#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>

#include <muduo/base/Atomic.h>
#include <muduo/net/UdpSocket.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/UdpServer.h>
#include <muduo/net/EventLoopThread.h>

#include <bfcp/common/bfcp_callbacks.h>
#include <bfcp/common/bfcp_param.h>
#include <bfcp/server/conference_define.h>

namespace bfcp
{
class BfcpConnection;
class ThreadPool;
class Conference;
typedef boost::shared_ptr<BfcpConnection> BfcpConnectionPtr;
typedef boost::shared_ptr<Conference> ConferencePtr;

class BaseServer
{
public:
  typedef boost::function<void ()> WorkerThreadInitCallback;
  typedef boost::function<void (ControlError)> ResultCallback;
  typedef boost::function<void (ControlError, void*)> ResultWithDataCallback;
  typedef std::vector<uint32_t> ConferenceIDList;

  static const double kDefaultUserObsoletedTime;

  BaseServer(muduo::net::EventLoop* loop, 
             const muduo::net::InetAddress& listenAddr);

  muduo::net::EventLoop* getLoop() { return loop_; }

  // NOTE: call before start
  void enableConnectionThread();

  void setWorkerThreadNum(int numThreads) { numThreads_ = numThreads; }
  
  void setWorkerThreadInitCallback(const WorkerThreadInitCallback &cb)
  { workerThreadInitCallback_ = cb; }
  void setWorkerThreadInitCallback(WorkerThreadInitCallback &&cb)
  { workerThreadInitCallback_ = std::move(cb); }

  void setUserObsoleteTime(double timeInSec) { userObsoletedTime_ = timeInSec; }

  void start();
  void stop();

  void addConference(
    uint32_t conferenceID, 
    const ConferenceConfig &config,
    const ResultCallback &cb);

  void removeConference(
    uint32_t conferenceID, 
    const ResultCallback &cb);

  void modifyConference(
    uint32_t conferenceID,
    const ConferenceConfig &config,
    const ResultCallback &cb);

  void addFloor(
    uint32_t conferenceID, 
    uint16_t floorID, 
    const FloorConfig &config,
    const ResultCallback &cb);

  void removeFloor(
    uint32_t conferenceID, 
    uint16_t floorID, 
    const ResultCallback &cb);

  void modifyFloor(
    uint32_t conferenceID, 
    uint16_t floorID, 
    const FloorConfig &config,
    const ResultCallback &cb);

  void addUser(
    uint32_t conferenceID,
    const UserInfoParam &user,
    const ResultCallback &cb);

  void removeUser(
    uint32_t conferenceID, 
    uint16_t userID,
    const ResultCallback &cb);

  void setChair(
    uint32_t conferenceID, 
    uint16_t floorID, 
    uint16_t userID,
    const ResultCallback &cb);

  void removeChair(
    uint32_t conferenceID,
    uint16_t floorID,
    const ResultCallback &cb);

  void getConferenceIDs(
    const ResultWithDataCallback &cb);

  void getConferenceInfo(
    uint32_t conferenceID,
    const ResultWithDataCallback &cb);
 
private:
  typedef boost::function<ControlError ()> ConferenceTask;

  void onStartedRecv(const muduo::net::UdpSocketPtr& socket);
  void onMessage(const muduo::net::UdpSocketPtr& socket, 
                 muduo::net::Buffer* buf, 
                 const muduo::net::InetAddress& src, 
                 muduo::Timestamp time);
  void onWriteComplete(const muduo::net::UdpSocketPtr& socket, int messageId);

  void onNewRequest(const BfcpMsgPtr &msg);

  void onResponse(
    uint32_t conferenceID, 
    bfcp_prim expectedPrimitive, 
    uint16_t userID,
    ResponseError err, 
    const BfcpMsgPtr &msg);

  void onChairActionTimeout(
    uint32_t conferenceID,
    uint16_t floorRequestID);

  void onHoldingFloorsTimeout(
    uint32_t conferenceID,
    uint16_t floorRequestID);

  void addConferenceInLoop(
    uint32_t conferenceID, 
    uint16_t maxFloorRequest,
    AcceptPolicy policy,
    double timeForChairAction,
    const ResultCallback &cb);

  void addConferenceInLoop(
    uint32_t conferenceID,
    const ConferenceConfig &config,
    const ResultCallback &cb);

  void removeConferenceInLoop(
    uint32_t conferenceID, 
    const ResultCallback &cb);

  void modifyConferenceInLoop(
    uint32_t conferenceID,
    const ConferenceConfig &config,
    const ResultCallback &cb);

  void addFloorInLoop(
    uint32_t conferenceID, 
    uint16_t floorID, 
    const FloorConfig &config,
    const ResultCallback &cb);

  void removeFloorInLoop(
    uint32_t conferenceID, 
    uint16_t floorID, 
    const ResultCallback &cb);

  void modifyFloorInLoop(
    uint32_t conferenceID, 
    uint16_t floorID, 
    const FloorConfig &config,
    const ResultCallback &cb);

  void addUserInLoop(
    uint32_t conferenceID, 
    const UserInfoParam &user,
    const ResultCallback &cb);

  void removeUserInLoop(
    uint32_t conferenceID, 
    uint16_t userID, 
    const ResultCallback &cb);

  void setChairInLoop(
    uint32_t conferenceID,
    uint16_t floorID,
    uint16_t userID,
    const ResultCallback &cb);

  void removeChairInLoop(
    uint32_t conferenceID,
    uint16_t floorID,
    const ResultCallback &cb);

  void getConferenceIDsInLoop(
    const ResultWithDataCallback &cb);

  void getConferenceInfoInLoop(
      uint32_t conferenceID, 
      const ResultWithDataCallback &cb);

  void wrapTaskAndCallback(
    const ConferenceTask &task, 
    const ResultCallback &cb)
  {
    auto res = task();
    if (cb) 
    {
      cb(res);
    }
  }

  template <typename Func, typename Arg1>
  void runInLoop(Func func, const Arg1 &arg1);

  template <typename Func, typename Arg1, typename Arg2>
  void runInLoop(
    Func func, const Arg1 &arg1, const Arg2 &arg2);

  template <typename Func, typename Arg1, typename Arg2, typename Arg3>
  void runInLoop(
    Func func, const Arg1 &arg1, const Arg2 &arg2, const Arg3 &arg3);

  template <typename Func, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
  void runInLoop(
    Func func, const Arg1 &arg1, const Arg2 &arg2, const Arg3 &arg3, const Arg4 &arg4);

  template <typename Func, typename Arg1>
  void runTask(
    Func func,
    uint32_t conferenceID, 
    const Arg1 &arg1, 
    const ResultCallback &cb);

  template <typename Func, typename Arg1>
  void runTask(
    Func func,
    uint32_t conferenceID, 
    const ResultWithDataCallback &cb);

  template <typename Func, typename Arg1, typename Arg2>
  void runTask(
    Func func, 
    uint32_t conferenceID, 
    const Arg1 &arg1,
    const Arg2 &arg2, 
    const ResultCallback &cb);

private:
  muduo::net::EventLoop* loop_;
  muduo::net::UdpServer server_;
  BfcpConnectionPtr connection_;
  muduo::net::EventLoop *connectionLoop_;
  boost::scoped_ptr<muduo::net::EventLoopThread> connectionThread_;
  WorkerThreadInitCallback workerThreadInitCallback_;
  int numThreads_;
  boost::shared_ptr<ThreadPool> threadPool_;
  muduo::AtomicInt32 started_;
  std::map<uint32_t, ConferencePtr> conferenceMap_;
  bool enableConnectionThread_;
  double userObsoletedTime_;
};

} // namespace bfcp

#endif // BFCP_BASE_SERVER_H