#ifndef BFCP_SERVER_H
#define BFCP_SERVER_H

#include <boost/bind.hpp>

#include <muduo/base/BlockingQueue.h>
#include <muduo/net/UdpSocket.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/UdpServer.h>

#include <muduo/base/Thread.h>

#include <bfcp/common/bfcp_callbacks.h>

namespace bfcp
{
class BfcpConnection;
typedef boost::shared_ptr<BfcpConnection> BfcpConnectionPtr;

class BfcpServer
{
public:
  BfcpServer(muduo::net::EventLoop* loop, const muduo::net::InetAddress& listenAddr);

  void start()
  {
    server_.start();
  }

  void stop()
  {
    server_.stop();
  }

private:
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
};

} // namespace bfcp

#endif // BFCP_SERVER_H