#include "bfcp_conn.h"

#include <muduo/base/Logging.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/UdpSocket.h>

#include "common/bfcp_msg.h"

using muduo::net::UdpSocketPtr;
using muduo::net::InetAddress;
using muduo::net::Buffer;
using muduo::strerror_tl;

namespace bfcp
{

BfcpConnection::BfcpConnection(const UdpSocketPtr &socket)
    : socket_(socket)
{
}

BfcpConnection::~BfcpConnection()
{
  LOG_TRACE << "BfcpConnection::~BfcpConnection destructing";
}

void BfcpConnection::onMessage(Buffer *buf, const InetAddress &src)
{
  BfcpMsg msg(buf, src);
  if (!msg.valid())
  {
    LOG_ERROR << "Parse error(" << msg.error() << ":" << strerror_tl(msg.error()) 
              << ") in BfcpConnection::onMessage";
    handleParseError(msg.error(), src);
    return;
  }

  if (tryHandleResponse(msg))
    return;

  if (tryHandleRequest(msg))
    return;

  if (newRequestCallback_)
    newRequestCallback_(msg);
}

void BfcpConnection::handleParseError( int err, const InetAddress &src )
{

}

bool BfcpConnection::tryHandleResponse(const BfcpMsg &msg)
{
  return false;
}

bool BfcpConnection::tryHandleRequest(const BfcpMsg &msg)
{
  return false;
}


} // namespace bfcp