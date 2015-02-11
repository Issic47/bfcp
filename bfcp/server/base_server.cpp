#include <bfcp/server/base_server.h>

#include <muduo/base/Logging.h>

#include <bfcp/common/bfcp_buf_pool.h>
#include <bfcp/common/bfcp_conn.h>

using namespace muduo;
using namespace muduo::net;

namespace bfcp
{

BaseServer::BaseServer(EventLoop* loop, const InetAddress& listenAddr ) : 
  loop_(CHECK_NOTNULL(loop)),
  server_(loop, listenAddr, "BfcpServer", UdpServer::kReuseAddr),
  connectionLoop_(loop),
  enableConnectionThread_(false)
{
  server_.setStartedRecvCallback(
    boost::bind(&BaseServer::onStartedRecv, this, _1));
  server_.setMessageCallback(
    boost::bind(&BaseServer::onMessage, this, _1, _2, _3, _4));
}

void BaseServer::onStartedRecv( const muduo::net::UdpSocketPtr& socket )
{
  LOG_TRACE << "Start receiving data at " << socket->getLocalAddr().toIpPort();
  if (!connection_)
  {
    connection_ = boost::make_shared<BfcpConnection>(connectionLoop_, socket);
    connection_->setNewRequestCallback(
      boost::bind(&BaseServer::onNewRequest, this, _1));
  }
}

void BaseServer::onMessage( const UdpSocketPtr& socket, Buffer* buf, const InetAddress& src, Timestamp time )
{
  LOG_TRACE << server_.name() << " recv " << buf->readableBytes() << " bytes at " << time.toString()
            << " from " << src.toIpPort();

  assert(connection_);
  connection_->onMessage(buf, src);
}

void BaseServer::onWriteComplete( const UdpSocketPtr& socket, int messageId )
{
  LOG_TRACE << "message " << messageId << " write completed";
}

void BaseServer::onNewRequest( const BfcpMsg &msg )
{
  LOG_TRACE << "BfcpServer received new request";
  // TODO: check msg and dispatch messages

  if (msg.primitive() == BFCP_GOODBYE) 
  {
    connection_->replyWithGoodByeAck(msg);
  }
}

void BaseServer::onResponse(uint32_t conferenceID, 
                            uint16_t userID, 
                            bfcp_prim expectedPrimitive, 
                            ResponseError err, 
                            const BfcpMsg &msg)
{
  LOG_TRACE << "BfcpServer received response";
}

void BaseServer::enableConnectionThread()
{
  // base loop for UdpServer receiving msg
  // and the extract one for BfcpConnection to send msg
  enableConnectionThread_ = true;
}

void BaseServer::start()
{
  if (started_.getAndSet(1) == 0)
  {
    if (enableConnectionThread_)
    {
      assert(!connectionThread_);
      connectionThread_.reset(new EventLoopThread);
      connectionLoop_ = connectionThread_->startLoop();
    }
    server_.start();
  }
}

void BaseServer::stop()
{
  if (started_.getAndSet(0) == 1)
  {
    if (enableConnectionThread_)
    {
      connectionThread_.reset(nullptr);
    }
    server_.stop();
  }
}

int BaseServer::addConference(uint32_t conferenceID, 
                              uint16_t maxFloorRequest, 
                              Conference::AcceptPolicy policy,
                              uint32_t timeForChairAction)
{
  // TODO: create new conference and init
  // TODO: add new conference to the global message queue
  // TODO: notify the result
  return -1;
}

int BaseServer::removeConference( uint32_t conferenceID )
{
  // TODO: set the conference message queue to release
  // TODO: add functional to the bfcp control message queue
  return -1;
}

int BaseServer::changeMaxFloorRequest( uint32_t conferenceID, uint16_t maxFloorRequest )
{
  return -1;
}

int BaseServer::changeAcceptPolicy( uint32_t conferenceID, Conference::AcceptPolicy policy )
{
  return -1;
}








} // namespace bfcp