#include "bfcp_conn.h"

#include <boost/bind.hpp>

#include <muduo/base/Logging.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/UdpSocket.h>
#include <muduo/net/EventLoop.h>

#include "common/bfcp_msg.h"
#include "common/bfcp_ctrans.h"
#include "common/bfcp_msg_build.h"

using muduo::net::UdpSocketPtr;
using muduo::net::InetAddress;
using muduo::net::Buffer;
using muduo::net::EventLoop;
using muduo::strerror_tl;

namespace bfcp
{
namespace detail
{

class AutoDeref
{
public:
  AutoDeref(void *buf): buf_(buf) {}
  ~AutoDeref() { mem_deref(buf_); }

private:
  void *buf_;
};

} // namespace detail
} // namespace bfcp

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

void BfcpConnection::sendFloorRequestInLoop(const BasicRequestParam &basicParam, 
                                            const FloorRequestParam &extParam)
{
  loop_->assertInLoopThread();
  // FIXME: get msg buf from pool
  mbuf_t *msgBuf = mbuf_alloc(BFCP_MBUF_SIZE);
  detail::AutoDeref derefer(msgBuf);
  if (!msgBuf)
  {
    // FIXME: do with no enough memory
    LOG_SYSFATAL << "No enough memory to build BFCP message";
  }
  
  bfcp_entity entity;
  initEntity(entity, basicParam.conferenceID, basicParam.userID);

  uint16_t bID = static_cast<uint16_t>(extParam.beneficiaryID);
  int err = build_msg_FloorRequest(
    msgBuf, BFCP_VER2, entity, 
    extParam.floorIDs, 
    extParam.beneficiaryID == -1 ? nullptr : &bID, 
    extParam.pInfo.c_str(), 
    &extParam.priority);
  // FIXME: check err(ENOMEM)

  ClientTransactionPtr ctran = boost::make_shared<ClientTransaction>(
    loop_, socket_, basicParam.dst, entity, msgBuf);
  ctrans_.insert(std::make_pair(entity, ctran));
  ctran->start();
}

uint16_t BfcpConnection::getNextTransactionID()
{
  uint16_t tid = 0;
  while ((tid = nextTid_.incrementAndGet()) == 0) {};
  return tid;
}

void BfcpConnection::sendFloorReleaseInLoop(const BasicRequestParam &basicParam,
                                            uint16_t floorRequestID)
{
  loop_->assertInLoopThread();
  // FIXME: get msg buf from pool
  mbuf_t *msgBuf = mbuf_alloc(BFCP_MBUF_SIZE);
  detail::AutoDeref derefer(msgBuf);
  if (!msgBuf)
  {
    // FIXME: do with no enough memory
    LOG_SYSFATAL << "No enough memory to build BFCP message";
  }

  bfcp_entity entity;
  initEntity(entity, basicParam.conferenceID, basicParam.userID);

  build_msg_FloorRelease(msgBuf, BFCP_VER2, entity, floorRequestID);

  ClientTransactionPtr ctran = boost::make_shared<ClientTransaction>(
    loop_, socket_, basicParam.dst, entity, msgBuf);
  ctrans_.insert(std::make_pair(entity, ctran));
  ctran->start();
}

void BfcpConnection::sendFloorRequestQueryInLoop(const BasicRequestParam &basicParam, 
                                                 uint16_t floorRequestID)
{
  loop_->assertInLoopThread();
  // FIXME: get msg buf from pool
  mbuf_t *msgBuf = mbuf_alloc(BFCP_MBUF_SIZE);
  detail::AutoDeref derefer(msgBuf);
  if (!msgBuf)
  {
    // FIXME: do with no enough memory
    LOG_SYSFATAL << "No enough memory to build BFCP message";
  }

  bfcp_entity entity;
  initEntity(entity, basicParam.conferenceID, basicParam.userID);

  build_msg_FloorRequestQuery(msgBuf, BFCP_VER2, entity, floorRequestID);

  ClientTransactionPtr ctran = boost::make_shared<ClientTransaction>(
    loop_, socket_, basicParam.dst, entity, msgBuf);
  ctrans_.insert(std::make_pair(entity, ctran));
  ctran->start();
}

void BfcpConnection::sendUserQueryInLoop( const BasicRequestParam &basicParam, uint16_t userID )
{
  loop_->assertInLoopThread();
  // FIXME: get msg buf from pool
  mbuf_t *msgBuf = mbuf_alloc(BFCP_MBUF_SIZE);
  detail::AutoDeref derefer(msgBuf);
  if (!msgBuf)
  {
    // FIXME: do with no enough memory
    LOG_SYSFATAL << "No enough memory to build BFCP message";
  }

  bfcp_entity entity;
  initEntity(entity, basicParam.conferenceID, basicParam.userID);

  build_msg_UserQuery(msgBuf, BFCP_VER2, entity, userID);

  ClientTransactionPtr ctran = boost::make_shared<ClientTransaction>(
    loop_, socket_, basicParam.dst, entity, msgBuf);
  ctrans_.insert(std::make_pair(entity, ctran));
  ctran->start();
}

} // namespace bfcp

