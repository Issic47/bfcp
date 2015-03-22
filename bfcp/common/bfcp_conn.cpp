#include <bfcp/common/bfcp_conn.h>

#include <boost/bind.hpp>

#include <muduo/base/Logging.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/UdpSocket.h>
#include <muduo/net/EventLoop.h>

#include <bfcp/common/bfcp_msg.h>
#include <bfcp/common/bfcp_ctrans.h>
#include <bfcp/common/bfcp_msg_build.h>

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
      cachedReplys_(BFCP_T2_SEC),
      cachedFragments_(BFCP_T2_SEC),
      nextTid_(1)
{
  LOG_TRACE << "BfcpConnection::BfcpConnection constructing";
  // FIXME: unsafe
  responseTimer_ = 
    loop_->runEvery(1.0, boost::bind(&BfcpConnection::onTimer, this));
  timerNeedStop_ = true;
  cachedReplys_.resize(BFCP_T2_SEC);
  cachedFragments_.resize(BFCP_T2_SEC);
}

BfcpConnection::~BfcpConnection()
{
  LOG_TRACE << "BfcpConnection::~BfcpConnection destructing";
  stopCacheTimer();
}

void BfcpConnection::stopCacheTimer()
{
  if (timerNeedStop_)
  {
    loop_->cancel(responseTimer_);
    timerNeedStop_ = false;
  }
}

void BfcpConnection::onTimer()
{
  //LOG_TRACE << "BfcpConnection::onTimer deleting cached reply messages";
  cachedReplys_.push_back(ReplyBucket());
  cachedFragments_.push_back(FragmentBucket());
}

void BfcpConnection::onMessage(muduo::net::Buffer *buf, 
                               const muduo::net::InetAddress &src, 
                               muduo::Timestamp receivedTime)
{
  BfcpMsg msg(buf, src, receivedTime);
  LOG_INFO << "Received BFCP message" << msg.toString() 
           << " from " << src.toIpPort();
  LOG_TRACE << "BFCP message in detail: \n"
            << msg.toStringInDetail();
  runInLoop(&BfcpConnection::onMessageInLoop, msg);
}

void BfcpConnection::onMessageInLoop( const BfcpMsg &msg )
{
  loop_->assertInLoopThread();

  BfcpMsg completedMsg;
  if (tryHandleFragmentMessage(msg, completedMsg))
    return;

  if (tryHandleMessageError(completedMsg))
    return;

  if (tryHandleResponse(completedMsg))
    return;

  if (tryHandleRequest(completedMsg))
    return;

  if (newRequestCallback_ && !msg.isResponse())
  {
    LOG_INFO << "Received new BFCP request message";
    newRequestCallback_(completedMsg);
  }
}

bool BfcpConnection::tryHandleFragmentMessage(const BfcpMsg &msg,
                                              BfcpMsg &completedMsg)
{
  if (msg.isComplete())
  {
    completedMsg = msg;
    return false;
  }

  detail::bfcp_msg_entry entry;
  entry.prim = msg.primitive();
  entry.entity = msg.getEntity();

  for (auto bucket : cachedFragments_)
  {
    auto it = bucket.find(entry);
    if (it != bucket.end())
    {
      BfcpMsg &fragMsg = (*it).second;
      fragMsg.addFragment(msg);
      if (!fragMsg.valid())
      {
        bucket.erase(it);
      }
      else if (fragMsg.isComplete())
      {
        completedMsg = fragMsg;
        bucket.erase(it);
        return false;
      }
      return true;
    }
  }

  // this msg is first received fragment
  cachedFragments_.back().insert(std::make_pair(entry, msg));
  return true;
}

bool BfcpConnection::tryHandleMessageError( const BfcpMsg &msg )
{
  if (!msg.valid() || msg.getVersion() == BFCP_VER1)
  {
    // msg.error(): ENOMEM, ENODATA, EBADMSG, ENOSYS,
    // FIXME: check error and report to the sender
    LOG_WARN << "Ignore invalid BFCP message" << msg.toString();
    return true;
  }
  return false;
}

bool BfcpConnection::tryHandleResponse(const BfcpMsg &msg)
{
  if (!msg.isResponse()) return false;

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
  if (msg.isResponse()) return false;

  detail::bfcp_strans_entry entry;
  entry.prim = msg.primitive();
  entry.entity = msg.getEntity();

  for (auto bucket : cachedReplys_)
  {
    auto it = bucket.find(entry);
    if (it != bucket.end())
    {
      LOG_INFO << "Reply BFCP message" << msg.toString() << " with cached reply";
      socket_->send(msg.getSrc(), (*it).second->buf, static_cast<int>((*it).second->end));
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
  LOG_INFO << "Send FloorRequest {cid=" << basicParam.conferenceID
           << ",uid=" << basicParam.userID << "}"; 
  sendRequestInLoop(&build_msg_FloorRequest, basicParam, floorRequest);
}

void BfcpConnection::sendFloorReleaseInLoop(const BasicRequestParam &basicParam,
                                            uint16_t floorRequestID)
{
  LOG_INFO << "Send FloorRelease {cid=" << basicParam.conferenceID
           << ",uid=" << basicParam.userID << "}";
  sendRequestInLoop(&build_msg_FloorRelease, basicParam, floorRequestID);
}

void BfcpConnection::sendFloorRequestQueryInLoop(const BasicRequestParam &basicParam, 
                                                 uint16_t floorRequestID)
{
  LOG_INFO << "Send FloorRequestQuery {cid=" << basicParam.conferenceID
           << ",uid=" << basicParam.userID << "}";
  sendRequestInLoop(&build_msg_FloorRequestQuery, basicParam, floorRequestID);
}

void BfcpConnection::sendUserQueryInLoop(const BasicRequestParam &basicParam, 
                                         const UserQueryParam &userQuery)
{
  LOG_INFO << "Send UserQuery {cid=" << basicParam.conferenceID
           << ",uid=" << basicParam.userID << "}";
  sendRequestInLoop(&build_msg_UserQuery, basicParam, userQuery);
}

void BfcpConnection::sendFloorQueryInLoop(const BasicRequestParam &basicParam, 
                                          const bfcp_floor_id_list &floorIDs)
{
  LOG_INFO << "Send FloorQuery {cid=" << basicParam.conferenceID
           << ",uid=" << basicParam.userID << "}";
  sendRequestInLoop(&build_msg_FloorQuery, basicParam, floorIDs);
}

void BfcpConnection::sendChairActionInLoop(const BasicRequestParam &basicParam, 
                                           const FloorRequestInfoParam &frqInfo)
{
  LOG_INFO << "Send ChairAction {cid=" << basicParam.conferenceID
           << ",uid=" << basicParam.userID << "}";
  sendRequestInLoop(&build_msg_ChairAction, basicParam, frqInfo); 
}

void BfcpConnection::sendHelloInLoop( const BasicRequestParam &basicParam )
{
  LOG_INFO << "Send Hello {cid=" << basicParam.conferenceID
           << ",uid=" << basicParam.userID << "}";
  sendRequestInLoop(&build_msg_Hello, basicParam);
}

void BfcpConnection::sendGoodbyeInLoop( const BasicRequestParam &basicParam )
{
  LOG_INFO << "Send Goodbye {cid=" << basicParam.conferenceID
           << ",uid=" << basicParam.userID << "}";
  sendRequestInLoop(&build_msg_Goodbye, basicParam);
}

void BfcpConnection::notifyFloorStatusInLoop(const BasicRequestParam &basicParam, 
                                             const FloorStatusParam &floorStatus)
{
  LOG_INFO << "Notify FloorStatus {cid=" << basicParam.conferenceID
           << ",uid=" << basicParam.userID << "}";
  sendRequestInLoop(
    boost::bind(&build_msg_FloorStatus, _1, false, _2, _3, _4), 
    basicParam, 
    floorStatus);
}

void BfcpConnection::notifyFloorRequestStatusInLoop(const BasicRequestParam &basicParam, 
                                                    const FloorRequestInfoParam &frqInfo)
{
  LOG_INFO << "Notify FloorRequestStatus {cid=" 
           << basicParam.conferenceID
           << ",uid=" << basicParam.userID << "}";

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
  (void)(err);

  startNewClientTransaction(basicParam.dst, entity, msgBuf, basicParam.cb);
}

template <typename BuildMsgFunc, typename ExtParam>
void bfcp::BfcpConnection::sendRequestInLoop(BuildMsgFunc buildFunc, 
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
  // FIXME: check error
  (void)(err);

  startNewClientTransaction(basicParam.dst, entity, msgBuf, basicParam.cb);
}

void BfcpConnection::startNewClientTransaction(const muduo::net::InetAddress &dst,
                                               const bfcp_entity &entity,
                                               mbuf_t *msgBuf,
                                               const ResponseCallback &cb)
{
  LOG_INFO << "Start new client transaction " << toString(entity);
  ClientTransactionPtr ctran = 
    boost::make_shared<ClientTransaction>(loop_, socket_, dst, entity, msgBuf);

  ctran->setReponseCallback(cb);
  ctran->setRequestTimeoutCallback(
    boost::bind(&BfcpConnection::onRequestTimeout, this, _1));

  ctrans_.insert(std::make_pair(entity, ctran));
  ctran->start();
}

void BfcpConnection::replyWithUserStatusInLoop(const BfcpMsg &msg, 
                                               const UserStatusParam &userStatus)
{ 
  assert(msg.primitive() == BFCP_USER_QUERY);
  LOG_INFO << "Reply with UserStatus to " << msg.toString();
  sendReplyInLoop(&build_msg_UserStatus, msg, userStatus); 
}

void BfcpConnection::replyWithChairActionAckInLoop( const BfcpMsg &msg )
{
  assert(msg.primitive() == BFCP_CHAIR_ACTION);
  LOG_INFO << "Reply with ChairActionAck to " << msg.toString();
  sendReplyInLoop(&build_msg_ChairActionAck, msg);
}

void BfcpConnection::replyWithHelloAckInLoop( const BfcpMsg &msg, const HelloAckParam &helloAck )
{
  assert(msg.primitive() == BFCP_HELLO);
  LOG_INFO << "Reply with HelloAck to " << msg.toString();
  sendReplyInLoop(&build_msg_HelloAck, msg, helloAck);
}

void BfcpConnection::replyWithErrorInLoop( const BfcpMsg &msg, const ErrorParam &error )
{
  LOG_INFO << "Reply with Error to " << msg.toString();
  sendReplyInLoop(&build_msg_Error, msg, error);
}

void BfcpConnection::replyWithFloorRequestStatusAckInLoop( const BfcpMsg &msg )
{
  assert(msg.primitive() == BFCP_FLOOR_REQUEST_STATUS);
  LOG_INFO << "Reply with FloorRequestStatusAck to " << msg.toString();
  sendReplyInLoop(&build_msg_FloorRequestStatusAck, msg);
}

void BfcpConnection::replyWithFloorStatusAckInLoop( const BfcpMsg &msg )
{
  assert(msg.primitive() == BFCP_FLOOR_STATUS);
  LOG_INFO << "Reply with FloorStatusAck to " << msg.toString();
  sendReplyInLoop(&build_msg_FloorStatusAck, msg);
}

void BfcpConnection::replyWithGoodbyeAckInLoop( const BfcpMsg &msg )
{
  assert(msg.primitive() == BFCP_GOODBYE);
  LOG_INFO << "Reply with GoodbyeAck to " << msg.toString();
  sendReplyInLoop(&build_msg_GoodbyeAck, msg);
}

void BfcpConnection::replyWithFloorRequestStatusInLoop(const BfcpMsg &msg,
                                                       const FloorRequestInfoParam &frqInfo)
{
  assert(msg.primitive() == BFCP_FLOOR_REQUEST_QUERY || 
         msg.primitive() == BFCP_FLOOR_REQUEST ||
         msg.primitive() == BFCP_FLOOR_RELEASE);
  LOG_INFO << "Reply with FloorRequestStatus to " << msg.toString();
  sendReplyInLoop(
    boost::bind(&build_msg_FloorRequestStatus, _1, true, _2, _3, _4),
    msg,
    frqInfo);
}

void BfcpConnection::replyWithFloorStatusInLoop(const BfcpMsg &msg,
                                                const FloorStatusParam &floorStatus)
{
  assert(msg.primitive() == BFCP_FLOOR_QUERY);
  LOG_INFO << "Reply with FloorStatus to " << msg.toString();
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
  (void)(err);

  startNewServerTransaction(msg.getSrc(), entity, msg.primitive(), msgBuf);
}

template <typename BuildMsgFunc, typename ExtParam>
void bfcp::BfcpConnection::sendReplyInLoop(BuildMsgFunc buildFunc,
                                           const BfcpMsg &msg,
                                           const ExtParam &extParam)
{
  loop_->assertInLoopThread();
  assert(msg.valid());
  assert(!msg.isResponse());

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
  (void)(err);

  startNewServerTransaction(msg.getSrc(), entity, msg.primitive(), msgBuf);
}

void BfcpConnection::startNewServerTransaction(const muduo::net::InetAddress &dst,
                                               const bfcp_entity &entity, 
                                               bfcp_prim primitive, 
                                               mbuf_t *msgBuf)
{
  LOG_INFO << "Start new server transaction to " << toString(entity, primitive);
  detail::bfcp_strans_entry entry;
  entry.entity = entity;
  entry.prim = primitive;
  cachedReplys_.back().insert(std::make_pair(entry, MBufPtr(msgBuf)));
  socket_->send(dst, msgBuf->buf, static_cast<int>(msgBuf->end));
}

} // namespace bfcp

