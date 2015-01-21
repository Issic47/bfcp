#ifndef BFCP_CLIENT_H
#define BFCP_CLIENT_H

#include <boost/bind.hpp>
#include <muduo/net/UdpSocket.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/UdpClient.h>

#include "common/bfcp_callbacks.h"

namespace bfcp
{

class BfcpConnection;
typedef boost::shared_ptr<BfcpConnection> BfcpConnectionPtr;

class BfcpClient : boost::noncopyable
{
public:
  BfcpClient(muduo::net::EventLoop* loop, 
             const muduo::net::InetAddress& serverAddr);

  void connect()
  {
    client_.connect();
  }

  void stop()
  {
    client_.disconnect();
  }

private:
  void onStartedRecv(const muduo::net::UdpSocketPtr& socket);
  void onMessage(const muduo::net::UdpSocketPtr& socket, 
                 muduo::net::Buffer* buf,
                 const muduo::net::InetAddress &src,
                 muduo::Timestamp time);

  void onWriteComplete(const muduo::net::UdpSocketPtr& socket, int messageId);
  void onNewRequest(const BfcpMsg &msg);
  void onResponse(ResponseError err, const BfcpMsg &msg);

  muduo::net::EventLoop* loop_;
  muduo::net::UdpClient client_;
  muduo::net::InetAddress serverAddr_;

  BfcpConnectionPtr connection_;
};

} // namespace bfcp



#endif // BFCP_CLIENT_H