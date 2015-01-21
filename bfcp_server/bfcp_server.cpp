#include "bfcp_server.h"

#include <muduo/base/Logging.h>

#include "common/bfcp_buf_pool.h"
#include "common/bfcp_conn.h"

using namespace muduo;
using namespace muduo::net;

namespace bfcp
{

BfcpServer::BfcpServer(EventLoop* loop, const InetAddress& listenAddr ) : loop_(loop),
  server_(loop, listenAddr, "BfcpServer", UdpServer::kReuseAddr)
{
  server_.setMessageCallback(
    boost::bind(&BfcpServer::onMessage, this, _1, _2, _3, _4));
}

void BfcpServer::onMessage( const UdpSocketPtr& socket, Buffer* buf, const InetAddress& src, Timestamp time )
{
  LOG_TRACE << server_.name() << " recv " << buf->readableBytes() << " bytes at " << time.toString()
            << " from " << src.toIpPort();

  if (!connection_)
  {
    connection_ = boost::make_shared<BfcpConnection>(loop_, socket);
    connection_->setNewRequestCallback(
      boost::bind(&BfcpServer::onNewRequest, this, _1));
  }

  connection_->onMessage(buf, src);
}

void BfcpServer::onWriteComplete( const UdpSocketPtr& socket, int messageId )
{
  LOG_TRACE << "message " << messageId << " write completed";
}

void BfcpServer::onNewRequest( const BfcpMsg &msg )
{
  LOG_TRACE << "BfcpServer received new request";
  if (msg.primitive() == BFCP_GOODBYE) 
  {
    connection_->replyWithGoodByeAck(msg);
  }
}

void BfcpServer::onResponse( ResponseError err, const BfcpMsg &msg )
{
  LOG_TRACE << "BfcpServer received response";
}


} // namespace bfcp