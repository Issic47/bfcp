#include "bfcp_conn.h"

#include <boost/bind.hpp>

#include <muduo/base/Logging.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/UdpSocket.h>
#include <muduo/net/EventLoop.h>

#include "common/bfcp_msg.h"
#include "common/bfcp_ctrans.h"

using muduo::net::UdpSocketPtr;
using muduo::net::InetAddress;
using muduo::net::Buffer;
using muduo::net::EventLoop;
using muduo::strerror_tl;

namespace bfcp
{

BfcpConnection::BfcpConnection(EventLoop *loop, const UdpSocketPtr &socket)
    : loop_(CHECK_NOTNULL(loop)),
      socket_(socket),
      cachedReplys_(BFCP_T2_SEC)
{
  LOG_TRACE << "BfcpConnection::BfcpConnection constructing";
  // FIXME: unsafe
  replyMsgTimer_ = 
    loop_->runEvery(1.0, boost::bind(&BfcpConnection::onTimer, this));
}

BfcpConnection::~BfcpConnection()
{
  LOG_TRACE << "BfcpConnection::~BfcpConnection destructing";
  loop_->cancel(replyMsgTimer_);
}

void BfcpConnection::onTimer()
{
  cachedReplys_.push_back(ReplyBucket());
}

void BfcpConnection::onMessage(Buffer *buf, const InetAddress &src)
{
  BfcpMsg msg(buf, src);
  if (!msg.valid())
  {
    LOG_ERROR << "Parse error(" << msg.error() << ":" << strerror_tl(msg.error()) 
              << ") in BfcpConnection::onMessage";
    return;
  }

  if (loop_->isInLoopThread())
  {
    onMessageInLoop(msg);
  }
  else
  {
    loop_->runInLoop(
      boost::bind(&BfcpConnection::onMessageInLoop, 
                  this, // FIXME
                  msg));
  }
}

void BfcpConnection::onMessageInLoop( const BfcpMsg &msg )
{
  loop_->assertInLoopThread();
  if (tryHandleResponse(msg))
    return;

  if (tryHandleRequest(msg))
    return;

  if (newRequestCallback_)
    newRequestCallback_(msg);
}

bool BfcpConnection::tryHandleResponse(const BfcpMsg &msg)
{
  ::bfcp_entity entity = msg.getEntity();
  auto it = ctrans_.find(entity);
  if (it != ctrans_.end())
  {
    (*it).second->onResponse(ResponseError::kNoError, msg);
    ctrans_.erase(it);
    return true;
  }

  return false;
}

bool BfcpConnection::tryHandleRequest(const BfcpMsg &msg)
{
  detail::bfcp_strans_entry entry;
  entry.prim = msg.primivity();
  entry.entity = msg.getEntity();

  for (auto bucket : cachedReplys_)
  {
    auto &it = bucket.find(entry);
    if (it != bucket.end())
    {
      socket_->send(msg.getSrc(), (*it).second->buf, (*it).second->end);
      return true;
    }
  }

  return false;
}

void BfcpConnection::onRequestTimeout(const ClientTransactionPtr &transaction)
{
  auto it = ctrans_.find(transaction->getEntity());
  if (it != ctrans_.end())
  {
    BfcpMsg msg;
    (*it).second->onResponse(ResponseError::kTimeout, msg);
    ctrans_.erase(it);
  }
  else
  {
    LOG_ERROR << "Cannot find client transaction in BfcpConnection::onRequestTimeout";
  }
}

} // namespace bfcp

