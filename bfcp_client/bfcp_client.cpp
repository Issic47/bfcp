#include "bfcp_client.h"

#include <muduo/base/Logging.h>

#include "common/bfcp_conn.h"

using namespace muduo;
using namespace muduo::net;

namespace bfcp
{

BfcpClient::BfcpClient(EventLoop* loop, const InetAddress& serverAddr) 
    : loop_(loop),
      client_(loop, serverAddr, "BfcpClient"),
      serverAddr_(serverAddr)
{
  client_.setMessageCallback(
    boost::bind(&BfcpClient::onMessage, this, _1, _2, _3, _4));
  client_.setStartedRecvCallback(
    boost::bind(&BfcpClient::onStartedRecv, this, _1));
  client_.setWriteCompleteCallback(
    boost::bind(&BfcpClient::onWriteComplete, this, _1, _2));
}

void BfcpClient::onStartedRecv( const UdpSocketPtr& socket )
{
  LOG_TRACE << socket->getLocalAddr().toIpPort() << " started receiving data from " 
            << socket->getPeerAddr().toIpPort();

  if (!connection_) 
  {
    connection_ = boost::make_shared<BfcpConnection>(loop_, socket);
    connection_->setNewRequestCallback(
      boost::bind(&BfcpClient::onNewRequest, this, _1));
  }

  BasicRequestParam param;
  param.cb = boost::bind(&BfcpClient::onResponse, this, _1, _2);
  param.conferenceID = 100;
  param.userID = 1;
  param.dst = serverAddr_;
  connection_->sendGoodBye(param);
}

void BfcpClient::onMessage(const UdpSocketPtr& socket, 
                           Buffer* buf, const InetAddress &src, 
                           Timestamp time)
{
  LOG_TRACE << client_.name() << " recv " << buf->readableBytes() << " bytes at " << time.toString();

  if (connection_)
  {
    connection_->onMessage(buf, src);
  }
}

void BfcpClient::onWriteComplete( const UdpSocketPtr& socket, int messageId )
{
  LOG_TRACE << "message " << messageId << " write completed";
}

void BfcpClient::onNewRequest( const BfcpMsg &msg )
{
  LOG_TRACE << "BfcpClient received new request";
}

void BfcpClient::onResponse( ResponseError err, const BfcpMsg &msg )
{
  LOG_TRACE << "BfcpClient received response";
  if (msg.valid() && msg.primitive() == BFCP_GOODBYE_ACK)
  {
    stop();
  }
}

}