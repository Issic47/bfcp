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
    // FIXME: check error and report to the sender
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
                                            const FloorRequestParam &floorRequest)
{
  sendRequestInLoop(&build_msg_FloorRequest, basicParam, floorRequest);
}

void BfcpConnection::sendFloorReleaseInLoop(const BasicRequestParam &basicParam,
                                            uint16_t floorRequestID)
{
  sendRequestInLoop(&build_msg_FloorRelease, basicParam, floorRequestID);
}

void BfcpConnection::sendFloorRequestQueryInLoop(const BasicRequestParam &basicParam, 
                                                 uint16_t floorRequestID)
{
  sendRequestInLoop(&build_msg_FloorRequestQuery, basicParam, floorRequestID);
}

void BfcpConnection::sendUserQueryInLoop(const BasicRequestParam &basicParam, 
                                         uint16_t userID)
{
  sendRequestInLoop(&build_msg_UserQuery, basicParam, userID);
}

void BfcpConnection::sendFloorQueryInLoop(const BasicRequestParam &basicParam, 
                                          const bfcp_floor_id_list &floorIDs)
{
  sendRequestInLoop(&build_msg_FloorQuery, basicParam, floorIDs);
}

void BfcpConnection::sendFloorStatusInLoop(const BasicRequestParam &basicParam, 
                                           const FloorStatusParam &floorStatus)
{
  sendRequestInLoop(
    boost::bind(&build_msg_FloorStatus, _1, false, _2, _3, _4), 
    basicParam, 
    floorStatus);
}

void BfcpConnection::sendChairActionInLoop(const BasicRequestParam &basicParam, 
                                           const FloorRequestInfoParam &frqInfo)
{
  sendRequestInLoop(&build_msg_ChairAction, basicParam, frqInfo); 
}

void BfcpConnection::sendHelloInLoop( const BasicRequestParam &basicParam )
{
  sendRequestInLoop(&build_msg_Hello, basicParam);
}

template <typename BuildFunc>
void bfcp::BfcpConnection::sendRequestInLoop(BuildFunc buildFunc, 
                                             const BasicRequestParam &basicParam)
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

  int err = buildFunc(msgBuf, BFCP_VER2, entity);
  // TODO: check error

  ClientTransactionPtr ctran = boost::make_shared<ClientTransaction>(
    loop_, socket_, basicParam.dst, entity, msgBuf);
  ctrans_.insert(std::make_pair(entity, ctran));
  ctran->start();
}

template <typename BuildFunc, typename ExtParam>
void bfcp::BfcpConnection::sendRequestInLoop(BuildFunc buildFunc, 
                                             const BasicRequestParam &basicParam, 
                                             const ExtParam &extParam)
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

  int err = buildFunc(msgBuf, BFCP_VER2, entity, extParam);
  // TODO: check error

  ClientTransactionPtr ctran = boost::make_shared<ClientTransaction>(
    loop_, socket_, basicParam.dst, entity, msgBuf);
  ctrans_.insert(std::make_pair(entity, ctran));
  ctran->start();
}

void BfcpConnection::replyWithUserStatusInLoop(const BfcpMsg &msg, 
                                               const UserStatusParam &userStatus)
{ 
  sendReplyInLoop(&build_msg_UserStatus, msg, userStatus); 
}

void BfcpConnection::replyWithChairActionAckInLoop( const BfcpMsg &msg )
{
  sendReplyInLoop(&build_msg_ChairActionAck, msg);
}

void BfcpConnection::replyWithHelloAckInLoop( const BfcpMsg &msg, const HelloAckParam &helloAck )
{
  sendReplyInLoop(&build_msg_HelloAck, msg, helloAck);
}

template <typename BuildFunc>
void bfcp::BfcpConnection::sendReplyInLoop(BuildFunc buildFunc, const BfcpMsg &msg)
{
  loop_->assertInLoopThread();
  assert(msg.valid());

  mbuf_t *msgBuf = mbuf_alloc(BFCP_MBUF_SIZE);
  detail::AutoDeref derefer(msgBuf);
  if (!msgBuf)
  {
    // FIXME: do with no enough memory
    LOG_SYSFATAL << "No enough memory to build BFCP message";
  }

  bfcp_entity entity =  msg.getEntity();
  int err = buildFunc(msgBuf, msg.getVersion(), entity);
  // check error

  detail::bfcp_strans_entry entry;
  entry.entity = entity;
  entry.prim = msg.primivity();
  cachedReplys_.back().insert(std::make_pair(entry, MBufPtr(msgBuf)));

  socket_->send(msg.getSrc(), msgBuf->buf, msgBuf->end);
}

template <typename BuildFunc, typename ExtParam>
void bfcp::BfcpConnection::sendReplyInLoop(BuildFunc buildFunc,
                                           const BfcpMsg &msg,
                                           const ExtParam &extParam)
{
  loop_->assertInLoopThread();
  assert(msg.valid());

  mbuf_t *msgBuf = mbuf_alloc(BFCP_MBUF_SIZE);
  detail::AutoDeref derefer(msgBuf);
  if (!msgBuf)
  {
    // FIXME: do with no enough memory
    LOG_SYSFATAL << "No enough memory to build BFCP message";
  }

  bfcp_entity entity =  msg.getEntity();
  int err = buildFunc(msgBuf, msg.getVersion(), entity, extParam);
  // check error

  detail::bfcp_strans_entry entry;
  entry.entity = entity;
  entry.prim = msg.primivity();
  cachedReplys_.back().insert(std::make_pair(entry, MBufPtr(msgBuf)));

  socket_->send(msg.getSrc(), msgBuf->buf, msgBuf->end);
}

} // namespace bfcp

