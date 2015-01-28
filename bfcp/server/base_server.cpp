#include <bfcp/server/base_server.h>

#include <muduo/base/Logging.h>

#include <bfcp/common/bfcp_buf_pool.h>
#include <bfcp/common/bfcp_conn.h>

using namespace muduo;
using namespace muduo::net;

namespace bfcp
{

BaseServer::BaseServer(EventLoop* loop, const InetAddress& listenAddr ) : loop_(loop),
  server_(loop, listenAddr, "BfcpServer", UdpServer::kReuseAddr)
{
  server_.setMessageCallback(
    boost::bind(&BaseServer::onMessage, this, _1, _2, _3, _4));
}

void BaseServer::onMessage( const UdpSocketPtr& socket, Buffer* buf, const InetAddress& src, Timestamp time )
{
  LOG_TRACE << server_.name() << " recv " << buf->readableBytes() << " bytes at " << time.toString()
            << " from " << src.toIpPort();

  if (!connection_)
  {
    connection_ = boost::make_shared<BfcpConnection>(loop_, socket);
    connection_->setNewRequestCallback(
      boost::bind(&BaseServer::onNewRequest, this, _1));
  }

  connection_->onMessage(buf, src);
}

void BaseServer::onWriteComplete( const UdpSocketPtr& socket, int messageId )
{
  LOG_TRACE << "message " << messageId << " write completed";
}

void BaseServer::onNewRequest( const BfcpMsg &msg )
{
  LOG_TRACE << "BfcpServer received new request";
  if (msg.primitive() == BFCP_GOODBYE) 
  {
    connection_->replyWithGoodByeAck(msg);
  }
}

void BaseServer::onResponse( ResponseError err, const BfcpMsg &msg )
{
  LOG_TRACE << "BfcpServer received response";
}


} // namespace bfcp