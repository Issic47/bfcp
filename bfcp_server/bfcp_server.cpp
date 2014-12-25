#include "bfcp_server.h"

#include <muduo/base/Logging.h>

#include "common/bfcp_buf_pool.h"

using namespace bfcp;
using namespace muduo;
using namespace muduo::net;

BfcpServer::BfcpServer(EventLoop* loop, const InetAddress& listenAddr ) : loop_(loop),
  server_(loop, listenAddr, "EchoServer", UdpServer::kReuseAddr)
{
  server_.setMessageCallback(
    boost::bind(&BfcpServer::onMessage, this, _1, _2, _3, _4));
}


void BfcpServer::onMessage( const UdpSocketPtr& socket, Buffer* buf, const InetAddress& src, Timestamp time )
{
  LOG_TRACE << server_.name() << " recv " << buf->readableBytes() << " bytes at " << time.toString()
            << " from " << src.toIpPort();

  BfcpBufNodePtr bufNode = BfcpBufPoolSingleton::instance().getFreeBufNode();
  bufNode->src = src;
  bufNode->time = time;
  bufNode->buf.retrieveAll();
  bufNode->buf.swap(*buf);
  BfcpBufQueueSingleton::instance().put(bufNode);
}
