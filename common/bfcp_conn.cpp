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
  LOG_TRACE << "BfcpConnection::onTimer deleting cached reply messages";
  cachedReplys_.push_back(ReplyBucket());
}

void BfcpConnection::onMessage(Buffer *buf, const InetAddress &src)
{
  BfcpMsg msg(buf, src);
  if (!msg.valid())
  {
    // FIXME: check error and report to the sender
    LOG_ERROR << "Parse error{errcode=" << msg.error() << ",errstr=" << strerror_tl(msg.error()) 
              << "} in BfcpConnection::onMessage";
    return;
  }

  runInLoop(&BfcpConnection::onMessageInLoop, msg);
}

void BfcpConnection::onMessageInLoop( const BfcpMsg &msg )
{
  loop_->assertInLoopThread();
  
  LOG_INFO << "Received BFCP message{cid=" << msg.getConferenceID() 
           << ",tid=" << msg.getTransactionID()
           << ",uid=" << msg.getUserID()
           << ",prim=" << bfcp_prim_name(msg.primivity()) 
           << "} from " << msg.getSrc().toIpPort();

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

void BfcpConnection::sendChairActionInLoop(const BasicRequestParam &basicParam, 
                                           const FloorRequestInfoParam &frqInfo)
{
  sendRequestInLoop(&build_msg_ChairAction, basicParam, frqInfo); 
}

void BfcpConnection::sendHelloInLoop( const BasicRequestParam &basicParam )
{
  sendRequestInLoop(&build_msg_Hello, basicParam);
}

void BfcpConnection::sendGoodByeInLoop( const BasicRequestParam &basicParam )
{
  sendRequestInLoop(&build_msg_GoodBye, basicParam);
}

void BfcpConnection::notifyFloorStatusInLoop(const BasicRequestParam &basicParam, 
                                             const FloorStatusParam &floorStatus)
{
  sendRequestInLoop(
    boost::bind(&build_msg_FloorStatus, _1, false, _2, _3, _4), 
    basicParam, 
    floorStatus);
}

void BfcpConnection::notifyFloorRequestStatusInLoop(const BasicRequestParam &basicParam, 
                                                    const FloorRequestInfoParam &frqInfo)
{
  sendRequestInLoop(
    boost::bind(&build_msg_FloorRequestStatus, _1, false, _2, _3, _4),
    basicParam,
    frqInfo);
}

template <typename BuildMsgFunc>
void bfcp::BfcpConnection::sendRequestInLoop(BuildMsgFunc buildFunc, 
                                             const BasicRequestParam &basicParam)
{
  loop_->assertInLoopThread();

  // TODO: log

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
  // FIXME: check error

  startNewClientTransaction(basicParam.dst, entity, msgBuf);
}

template <typename BuildMsgFunc, typename ExtParam>
void bfcp::BfcpConnection::sendRequestInLoop(BuildMsgFunc buildFunc, 
                                             const BasicRequestParam &basicParam, 
                                             const ExtParam &extParam)
{
  loop_->assertInLoopThread();

  // TODO: log

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
  // FIXME: check error

  startNewClientTransaction(basicParam.dst, entity, msgBuf);
}

void BfcpConnection::startNewClientTransaction(const muduo::net::InetAddress &dst,
                                               const bfcp_entity &entity, 
                                               mbuf_t *msgBuf)
{
  ClientTransactionPtr ctran = 
    boost::make_shared<ClientTransaction>(loop_, socket_, dst, entity, msgBuf);
  ctrans_.insert(std::make_pair(entity, ctran));
  ctran->start();
}

void BfcpConnection::replyWithUserStatusInLoop(const BfcpMsg &msg, 
                                               const UserStatusParam &userStatus)
{ 
  assert(msg.primivity() == BFCP_USER_QUERY);
  sendReplyInLoop(&build_msg_UserStatus, msg, userStatus); 
}

void BfcpConnection::replyWithChairActionAckInLoop( const BfcpMsg &msg )
{
  assert(msg.primivity() == BFCP_CHAIR_ACTION);
  sendReplyInLoop(&build_msg_ChairActionAck, msg);
}

void BfcpConnection::replyWithHelloAckInLoop( const BfcpMsg &msg, const HelloAckParam &helloAck )
{
  assert(msg.primivity() == BFCP_HELLO);
  sendReplyInLoop(&build_msg_HelloAck, msg, helloAck);
}

void BfcpConnection::replyWithErrorInLoop( const BfcpMsg &msg, const ErrorParam &error )
{
  sendReplyInLoop(&build_msg_Error, msg, error);
}

void BfcpConnection::replyWithFloorRequestStatusAckInLoop( const BfcpMsg &msg )
{
  assert(msg.primivity() == BFCP_FLOOR_REQ_STATUS);
  sendReplyInLoop(&build_msg_FloorRequestStatusAck, msg);
}

void BfcpConnection::replyWithFloorStatusAckInLoop( const BfcpMsg &msg )
{
  assert(msg.primivity() == BFCP_FLOOR_STATUS);
  sendReplyInLoop(&build_msg_FloorStatusAck, msg);
}

void BfcpConnection::replyWithGoodByeAckInLoop( const BfcpMsg &msg )
{
  assert(msg.primivity() == BFCP_GOODBYE);
  sendReplyInLoop(&build_msg_GoodByeAck, msg);
}

void BfcpConnection::replyWithFloorRequestStatusInLoop(const BfcpMsg &msg,
                                                       const FloorRequestInfoParam &frqInfo)
{
  assert(msg.primivity() == BFCP_FLOOR_REQUEST_QUERY);
  sendReplyInLoop(
    boost::bind(&build_msg_FloorRequestStatus, _1, true, _2, _3, _4),
    msg,
    frqInfo);
}

void BfcpConnection::replyWithFloorStatusInLoop(const BfcpMsg &msg,
                                                const FloorStatusParam &floorStatus)
{
  assert(msg.primivity() == BFCP_FLOOR_QUERY);
  sendReplyInLoop(
    boost::bind(&build_msg_FloorStatus, _1, true, _2, _3, _4),
    msg, 
    floorStatus);
}

template <typename BuildMsgFunc>
void bfcp::BfcpConnection::sendReplyInLoop(BuildMsgFunc buildFunc, const BfcpMsg &msg)
{
  loop_->assertInLoopThread();
  assert(msg.valid());
  assert(!msg.isResponse());

  // TODO: log

  mbuf_t *msgBuf = mbuf_alloc(BFCP_MBUF_SIZE);
  detail::AutoDeref derefer(msgBuf);
  if (!msgBuf)
  {
    // FIXME: do with no enough memory
    LOG_SYSFATAL << "No enough memory to build BFCP message";
  }

  bfcp_entity entity =  msg.getEntity();
  int err = buildFunc(msgBuf, msg.getVersion(), entity);
  // FIXME: check error

  startNewServerTransaction(msg.getSrc(), entity, msg.primivity(), msgBuf);
}

template <typename BuildMsgFunc, typename ExtParam>
void bfcp::BfcpConnection::sendReplyInLoop(BuildMsgFunc buildFunc,
                                           const BfcpMsg &msg,
                                           const ExtParam &extParam)
{
  loop_->assertInLoopThread();
  assert(msg.valid());
  assert(!msg.isResponse());

  // TODO: log

  mbuf_t *msgBuf = mbuf_alloc(BFCP_MBUF_SIZE);
  detail::AutoDeref derefer(msgBuf);
  if (!msgBuf)
  {
    // FIXME: do with no enough memory
    LOG_SYSFATAL << "No enough memory to build BFCP message";
  }

  bfcp_entity entity =  msg.getEntity();
  int err = buildFunc(msgBuf, msg.getVersion(), entity, extParam);
  // FIXME: check error

  startNewServerTransaction(msg.getSrc(), entity, msg.primivity(), msgBuf);
}

void BfcpConnection::startNewServerTransaction(const muduo::net::InetAddress &dst,
                                               const bfcp_entity &entity, 
                                               bfcp_prim primivity, 
                                               mbuf_t *msgBuf)
{
  detail::bfcp_strans_entry entry;
  entry.entity = entity;
  entry.prim = primivity;
  cachedReplys_.back().insert(std::make_pair(entry, MBufPtr(msgBuf)));

  socket_->send(dst, msgBuf->buf, msgBuf->end);
}

} // namespace bfcp

