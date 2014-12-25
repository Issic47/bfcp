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

namespace bfcp
{

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

  muduo::net::EventLoop* loop_;
  muduo::net::UdpServer server_;
  //muduo::Thread connThread_;
};

} // namespace bfcp

#endif // BFCP_SERVER_H