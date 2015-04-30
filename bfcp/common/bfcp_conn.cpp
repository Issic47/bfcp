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
      currentCachedReplys_(0),
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
  currentCachedReplys_ -= cachedReplys_.front().size();
  cachedReplys_.push_back(ReplyBucket());
  
  if (!cachedFragments_.front().empty())
  {
    LOG_WARN << "Some cached fragments was cleared";
  }
  cachedFragments_.push_back(FragmentBucket());
}

void BfcpConnection::onMessage(muduo::net::Buffer *buf, 
                               const muduo::net::InetAddress &src, 
                               muduo::Timestamp receivedTime)
{
  BfcpMsgPtr msg = boost::make_shared<BfcpMsg>(buf, src, receivedTime);
  LOG_INFO << "Received BFCP message" << msg->toString() 
           << " from " << src.toIpPort();
  runInLoop(&BfcpConnection::onMessageInLoop, msg);
}

void BfcpConnection::onMessageInLoop( const BfcpMsgPtr &msg )
{
  loop_->assertInLoopThread();

  BfcpMsgPtr completedMsg;
  if (tryHandleFragmentMessage(msg, completedMsg))
    return;

  LOG_INFO << "Received complete BFCP message in detail: \n" 
           << completedMsg->toStringInDetail();

  if (tryHandleMessageError(completedMsg))
    return;

  if (tryHandleResponse(completedMsg))
    return;

  if (tryHandleRequest(completedMsg))
    return;

  if (newRequestCallback_ && !msg->isResponse())
  {
    LOG_INFO << "Received new BFCP request message";
    newRequestCallback_(completedMsg);
  }
}

bool BfcpConnection::tryHandleFragmentMessage(const BfcpMsgPtr &msg,
                                              BfcpMsgPtr &completedMsg)
{
  if (msg->isComplete())
  {
    completedMsg = msg;
    return false;
  }

  detail::bfcp_msg_entry entry;
  entry.prim = msg->primitive();
  entry.entity = msg->getEntity();

  for (auto &bucket : cachedFragments_)
  {
    auto it = bucket.find(entry);
    if (it != bucket.end())
    {
      BfcpMsgPtr &fragMsg = (*it).second;
      fragMsg->addFragment(msg);
      if (!fragMsg->valid())
      {
        LOG_WARN << "Invalid fragments: " << fragMsg->toString();
        bucket.erase(it);
      }
      else if (fragMsg->isComplete())
      {
        LOG_DEBUG << "Complete fragments: " << fragMsg->toString();
        completedMsg = fragMsg;
        bucket.erase(it);
        return false;
      }
      return true;
    }
  }

  // this msg is first received fragment
  LOG_DEBUG << "Insert new fragments: " << msg->toString();
  cachedFragments_.back().insert(std::make_pair(entry, msg));
  return true;
}

bool BfcpConnection::tryHandleMessageError( const BfcpMsgPtr &msg )
{
  if (!msg->valid() || msg->getVersion() == BFCP_VER1)
  {
    // msg->error(): ENOMEM, ENODATA, EBADMSG, ENOSYS,
    // FIXME: check error and report to the sender
    LOG_WARN << "Ignore invalid BFCP message" << msg->toString();
    return true;
  }
  return false;
}

bool BfcpConnection::tryHandleResponse(const BfcpMsgPtr &msg)
{
  if (!msg->isResponse()) return false;

  ::bfcp_entity entity = msg->getEntity();
  auto it = ctrans_.find(entity);
  if (it != ctrans_.end())
  {
    (*it).second->onResponse(ResponseError::kNoError, msg);
    ctrans_.erase(it);
    return true;
  }

  return false;
}

bool BfcpConnection::tryHandleRequest(const BfcpMsgPtr &msg)
{
  if (msg->isResponse()) return false;

  detail::bfcp_strans_entry entry;
  entry.prim = msg->primitive();
  entry.entity = msg->getEntity();

  for (auto &bucket : cachedReplys_)
  {
    auto it = bucket.find(entry);
    if (it != bucket.end())
    {
      LOG_INFO << "Reply BFCP message" << msg->toString() << " with cached reply";
      for (auto &buf : (*it).second)
      {
        socket_->send(msg->getSrc(), buf->buf, static_cast<int>(buf->end));
      }
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
    BfcpMsgPtr msg = boost::make_shared<BfcpMsg>();
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

  msgBuf->pos = 0;
  std::vector<mbuf_t*> fragBufs;
  int err = build_msg_fragments(fragBufs, msgBuf, MAX_MSG_SIZE);
  // FIXME: check error
  (void)(err);

  ClientTransactionPtr ctran = 
    boost::make_shared<ClientTransaction>(loop_, socket_, dst, entity, fragBufs);

  ctran->setReponseCallback(cb);
  ctran->setRequestTimeoutCallback(
    boost::bind(&BfcpConnection::onRequestTimeout, this, _1));

  ctrans_.insert(std::make_pair(entity, ctran));
  ctran->start();
}

void BfcpConnection::replyWithUserStatusInLoop(const BfcpMsgPtr &msg, 
                                               const UserStatusParam &userStatus)
{ 
  assert(msg->primitive() == BFCP_USER_QUERY);
  LOG_INFO << "Reply with UserStatus to " << msg->toString();
  sendReplyInLoop(&build_msg_UserStatus, msg, userStatus); 
}

void BfcpConnection::replyWithChairActionAckInLoop( const BfcpMsgPtr &msg )
{
  assert(msg->primitive() == BFCP_CHAIR_ACTION);
  LOG_INFO << "Reply with ChairActionAck to " << msg->toString();
  sendReplyInLoop(&build_msg_ChairActionAck, msg);
}

void BfcpConnection::replyWithHelloAckInLoop( const BfcpMsgPtr &msg, const HelloAckParam &helloAck )
{
  assert(msg->primitive() == BFCP_HELLO);
  LOG_INFO << "Reply with HelloAck to " << msg->toString();
  sendReplyInLoop(&build_msg_HelloAck, msg, helloAck);
}

void BfcpConnection::replyWithErrorInLoop( const BfcpMsgPtr &msg, const ErrorParam &error )
{
  LOG_INFO << "Reply with Error to " << msg->toString();
  sendReplyInLoop(&build_msg_Error, msg, error);
}

void BfcpConnection::replyWithFloorRequestStatusAckInLoop( const BfcpMsgPtr &msg )
{
  assert(msg->primitive() == BFCP_FLOOR_REQUEST_STATUS);
  LOG_INFO << "Reply with FloorRequestStatusAck to " << msg->toString();
  sendReplyInLoop(&build_msg_FloorRequestStatusAck, msg);
}

void BfcpConnection::replyWithFloorStatusAckInLoop( const BfcpMsgPtr &msg )
{
  assert(msg->primitive() == BFCP_FLOOR_STATUS);
  LOG_INFO << "Reply with FloorStatusAck to " << msg->toString();
  sendReplyInLoop(&build_msg_FloorStatusAck, msg);
}

void BfcpConnection::replyWithGoodbyeAckInLoop( const BfcpMsgPtr &msg )
{
  assert(msg->primitive() == BFCP_GOODBYE);
  LOG_INFO << "Reply with GoodbyeAck to " << msg->toString();
  sendReplyInLoop(&build_msg_GoodbyeAck, msg);
}

void BfcpConnection::replyWithFloorRequestStatusInLoop(const BfcpMsgPtr &msg,
                                                       const FloorRequestInfoParam &frqInfo)
{
  assert(msg->primitive() == BFCP_FLOOR_REQUEST_QUERY || 
         msg->primitive() == BFCP_FLOOR_REQUEST ||
         msg->primitive() == BFCP_FLOOR_RELEASE);
  LOG_INFO << "Reply with FloorRequestStatus to " << msg->toString();
  sendReplyInLoop(
    boost::bind(&build_msg_FloorRequestStatus, _1, true, _2, _3, _4),
    msg,
    frqInfo);
}

void BfcpConnection::replyWithFloorStatusInLoop(const BfcpMsgPtr &msg,
                                                const FloorStatusParam &floorStatus)
{
  assert(msg->primitive() == BFCP_FLOOR_QUERY);
  LOG_INFO << "Reply with FloorStatus to " << msg->toString();
  sendReplyInLoop(
    boost::bind(&build_msg_FloorStatus, _1, true, _2, _3, _4),
    msg, 
    floorStatus);
}

template <typename BuildMsgFunc>
void bfcp::BfcpConnection::sendReplyInLoop(BuildMsgFunc buildFunc, const BfcpMsgPtr &msg)
{
  loop_->assertInLoopThread();
  assert(msg->valid());
  assert(!msg->isResponse());

  mbuf_t *msgBuf = mbuf_alloc(BFCP_MBUF_SIZE);
  detail::AutoDeref derefer(msgBuf);
  if (!msgBuf)
  {
    // FIXME: do with no enough memory
    LOG_SYSFATAL << "No enough memory to build BFCP message";
  }

  bfcp_entity entity =  msg->getEntity();
  int err = buildFunc(msgBuf, msg->getVersion(), entity);
  // FIXME: check error
  (void)(err);

  startNewServerTransaction(msg->getSrc(), entity, msg->primitive(), msgBuf);
}

template <typename BuildMsgFunc, typename ExtParam>
void bfcp::BfcpConnection::sendReplyInLoop(BuildMsgFunc buildFunc,
                                           const BfcpMsgPtr &msg,
                                           const ExtParam &extParam)
{
  loop_->assertInLoopThread();
  assert(msg->valid());
  assert(!msg->isResponse());

  mbuf_t *msgBuf = mbuf_alloc(BFCP_MBUF_SIZE);
  detail::AutoDeref derefer(msgBuf);
  if (!msgBuf)
  {
    // FIXME: do with no enough memory
    LOG_SYSFATAL << "No enough memory to build BFCP message";
  }

  bfcp_entity entity =  msg->getEntity();
  int err = buildFunc(msgBuf, msg->getVersion(), entity, extParam);
  // FIXME: check error
  (void)(err);

  startNewServerTransaction(msg->getSrc(), entity, msg->primitive(), msgBuf);
}

void BfcpConnection::startNewServerTransaction(const muduo::net::InetAddress &dst,
                                               const bfcp_entity &entity, 
                                               bfcp_prim primitive, 
                                               mbuf_t *msgBuf)
{
  LOG_INFO << "Start new server transaction to " << toString(entity, primitive);
  
  msgBuf->pos = 0;
  std::vector<mbuf_t*> fragBufs;
  int err = build_msg_fragments(fragBufs, msgBuf, MAX_MSG_SIZE);
  // FIXME: check error
  (void)(err);
  
  MBufList bufs;
  bufs.reserve(fragBufs.size());
  bufs.assign(fragBufs.begin(), fragBufs.end());
  
  for (auto buf : fragBufs)
  {
    mem_deref(buf);
  }
  fragBufs.clear();

  if (currentCachedReplys_ < MAX_CACHED_REPLY_SIZE) {
    detail::bfcp_strans_entry entry;
    entry.entity = entity;
    entry.prim = primitive;
    cachedReplys_.back().insert(std::make_pair(entry, bufs));
    ++currentCachedReplys_;
  }

  for (auto &buf : bufs)
  {
    socket_->send(dst, buf->buf, static_cast<int>(buf->end));
  }
}

} // namespace bfcp

