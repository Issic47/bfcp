#ifndef BFCP_BASE_SERVER_H
#define BFCP_BASE_SERVER_H

#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>

#include <muduo/base/Atomic.h>
#include <muduo/base/BlockingQueue.h>
#include <muduo/net/UdpSocket.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/UdpServer.h>
#include <muduo/net/EventLoopThread.h>

#include <muduo/base/Thread.h>

#include <bfcp/common/bfcp_callbacks.h>
#include <bfcp/server/conference.h>

namespace bfcp
{
class BfcpConnection;
typedef boost::shared_ptr<BfcpConnection> BfcpConnectionPtr;

class BaseServer
{
public:
  typedef boost::function<void()> WorkerThreadInitCallback;

  BaseServer(muduo::net::EventLoop* loop, const muduo::net::InetAddress& listenAddr);

  // NOTE: call before start
  void enableConnectionThread();
  void start();
  void stop();

  int addConference(uint32_t conferenceID, 
                    uint16_t maxFloorRequest, 
                    Conference::AcceptPolicy policy, 
                    uint32_t timeForChairAction);
  int removeConference(uint32_t conferenceID);
  int changeMaxFloorRequest(uint32_t conferenceID, uint16_t maxFloorRequest);
  int changeAcceptPolicy(uint32_t conferenceID, Conference::AcceptPolicy policy);

  int addFloor(uint32_t conferenceID, 
               uint16_t floorID, 
               int chairID,
               uint16_t maxGrantedNum);
  int removeFloor(uint32_t conferenceID, uint16_t floorID);
  int changeMaxGrantedNum(uint32_t conferenceID, uint16_t floorID, uint16_t maxGrantedNum);

  int addUser(uint32_t conferenceID,
              uint16_t userID,
              const string &displayName,
              const string &uri);
  int removeUser(uint32_t conferenceID, uint16_t userID);

  int addChair(uint32_t conferenceID, uint16_t floorID, uint16_t userID);
  int removeChair(uint32_t conferenceID, uint16_t floorID);
 
private:
  void onStartedRecv(const muduo::net::UdpSocketPtr& socket);
  void onMessage(const muduo::net::UdpSocketPtr& socket, 
                 muduo::net::Buffer* buf, 
                 const muduo::net::InetAddress& src, 
                 muduo::Timestamp time);

  void onWriteComplete(const muduo::net::UdpSocketPtr& socket, int messageId);
  void onNewRequest(const BfcpMsg &msg);
  void onResponse(ResponseError err, const BfcpMsg &msg);

  muduo::net::EventLoop* loop_;
  muduo::net::UdpServer server_;

  BfcpConnectionPtr connection_;
  muduo::net::EventLoop *connectionLoop_;
  boost::scoped_ptr<muduo::net::EventLoopThread> connectionThread_;

  muduo::AtomicInt32 started_;
  bool enableConnectionThread_;
};

} // namespace bfcp

#endif // BFCP_BASE_SERVER_H