#ifndef BFCP_CONN_H
#define BFCP_CONN_H

#include <boost/noncopyable.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/circular_buffer.hpp>

#include <muduo/net/Callbacks.h>
#include <muduo/base/Atomic.h>
#include <muduo/net/TimerId.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>

#include <bfcp/common/bfcp_callbacks.h>
#include <bfcp/common/bfcp_ex.h>
#include <bfcp/common/bfcp_mbuf_wrapper.h>
#include <bfcp/common/bfcp_param.h>
#include <bfcp/common/bfcp_msg.h>

#include <vector>
#include <map>

namespace bfcp
{

class ClientTransaction;
typedef boost::shared_ptr<ClientTransaction> ClientTransactionPtr;

namespace detail
{

typedef struct bfcp_strans_entry
{
  bfcp_prim prim;
  bfcp_entity entity;
} bfcp_strans_entry;

inline bool operator<(const bfcp_strans_entry &lhs, const bfcp_strans_entry &rhs)
{
  int r = compare(lhs.entity, rhs.entity);
  if (r < 0) return true;
  if (r == 0) return lhs.prim < rhs.prim;
  return false;
}

} // namespace detail

class BasicRequestParam
{
public:
  BasicRequestParam() {}
  BasicRequestParam(BasicRequestParam &&param)
      : cb(std::move(param.cb)),
        dst(param.dst),
        conferenceID(param.conferenceID),
        userID(param.userID)
  {}

  ResponseCallback cb;
  muduo::net::InetAddress dst;
  uint32_t conferenceID;
  uint16_t userID;
};

class BfcpConnection : public boost::shared_ptr<BfcpConnection>,
                       boost::noncopyable
{
public:
  BfcpConnection(muduo::net::EventLoop *loop, const muduo::net::UdpSocketPtr &socket);
  ~BfcpConnection();

  void onMessage(muduo::net::Buffer *buf, const muduo::net::InetAddress &src);

  void setNewRequestCallback(const NewRequestCallback &cb)
  { newRequestCallback_ = cb; }

  void setNewRequestCallback(NewRequestCallback &&cb)
  { newRequestCallback_ = std::move(cb); }

  void sendFloorRequest(const BasicRequestParam &basicParam, const FloorRequestParam &floorRequest)
  { runInLoop(&BfcpConnection::sendFloorRequestInLoop, basicParam, floorRequest); }

  void sendFloorRelease(const BasicRequestParam &basicParam, uint16_t floorRequestID)
  { runInLoop(&BfcpConnection::sendFloorReleaseInLoop, basicParam, floorRequestID); }

  void sendFloorRequestQuery(const BasicRequestParam &basicParam, uint16_t floorRequestID)
  { runInLoop(&BfcpConnection::sendFloorRequestQueryInLoop, basicParam, floorRequestID); }

  void sendUserQuery(const BasicRequestParam &basicParam, const UserQueryParam &userQuery)
  { runInLoop(&BfcpConnection::sendUserQueryInLoop, basicParam, userQuery); }

  void sendFloorQuery(const BasicRequestParam &basicParam, const bfcp_floor_id_list &floorIDs)
  { runInLoop(&BfcpConnection::sendFloorQueryInLoop, basicParam, floorIDs); }

  void sendChairAction(const BasicRequestParam &basicParam, const FloorRequestInfoParam &frqInfo)
  { runInLoop(&BfcpConnection::sendChairActionInLoop, basicParam, frqInfo); }

  void sendHello(const BasicRequestParam &basicParam)
  { runInLoop(&BfcpConnection::sendHelloInLoop, basicParam); }

  void sendGoodBye(const BasicRequestParam &basicParam)
  { runInLoop(&BfcpConnection::sendGoodByeInLoop, basicParam); }

  void replyWithFloorRequestStatus(const BfcpMsg &msg, const FloorRequestInfoParam &frqInfo)
  { runInLoop(&BfcpConnection::replyWithFloorRequestStatusInLoop, msg, frqInfo); }

  void replyWithFloorStatus(const BfcpMsg &msg, const FloorStatusParam &floorStatus) 
  { runInLoop(&BfcpConnection::replyWithFloorStatusInLoop, msg, floorStatus); }

  void replyWithUserStatus(const BfcpMsg &msg, const UserStatusParam &userStatus)
  { runInLoop(&BfcpConnection::replyWithUserStatusInLoop, msg, userStatus); }

  void replyWithChairActionAck(const BfcpMsg &msg) 
  { runInLoop(&BfcpConnection::replyWithChairActionAckInLoop, msg); }

  void replyWithHelloAck(const BfcpMsg &msg, const HelloAckParam &helloAck)
  { runInLoop(&BfcpConnection::replyWithHelloAckInLoop, msg, helloAck); }

  void replyWithError(const BfcpMsg &msg, const ErrorParam &error)
  { runInLoop(&BfcpConnection::replyWithErrorInLoop, msg, error); }

  void replyWithFloorRequestStatusAck(const BfcpMsg &msg)
  { runInLoop(&BfcpConnection::replyWithFloorRequestStatusAckInLoop, msg); }

  void replyWithFloorStatusAck(const BfcpMsg &msg)
  { runInLoop(&BfcpConnection::replyWithFloorStatusAckInLoop, msg); }

  void replyWithGoodByeAck(const BfcpMsg &msg)
  { runInLoop(&BfcpConnection::replyWithGoodByeAckInLoop, msg); }

  void notifyFloorRequestStatus(const BasicRequestParam &basicParam, 
                                const FloorRequestInfoParam &frqInfo)
  {
    runInLoop(&BfcpConnection::notifyFloorRequestStatusInLoop, basicParam, frqInfo);
  }

  void notifyFloorStatus(const BasicRequestParam &basicParam, 
    const FloorStatusParam &floorStatus)
  { 
    runInLoop(&BfcpConnection::notifyFloorStatusInLoop, basicParam, floorStatus); 
  }

private:
  static const int BFCP_T2_SEC = 10;
  static const int BFCP_MBUF_SIZE = 65536;

  typedef muduo::detail::AtomicIntegerT<uint16_t> AtomicUInt16;
  typedef MBufWrapper MBufPtr;
  typedef std::map<detail::bfcp_strans_entry, MBufPtr> ReplyBucket;

  template <typename Func, typename Arg1>
  void runInLoop(Func requestFunc, const Arg1 &basic);

  template <typename Func, typename Arg1, typename Arg2>
  void runInLoop(Func requestFunc, const Arg1 &basic, const Arg2 &ext);

  template <typename BuildMsgFunc>
  void sendRequestInLoop(BuildMsgFunc buildFunc, 
                         const BasicRequestParam &basicParam);

  template <typename BuildMsgFunc, typename ExtParam>
  void sendRequestInLoop(BuildMsgFunc buildFunc, 
                         const BasicRequestParam &basicParam, 
                         const ExtParam &extParam);

  template <typename BuildMsgFunc>
  void sendReplyInLoop(BuildMsgFunc buildFunc, const BfcpMsg &msg);

  template <typename BuildMsgFunc, typename ExtParam>
  void sendReplyInLoop(BuildMsgFunc buildFunc, const BfcpMsg &msg, const ExtParam &extParam);

  bool tryHandleMessageError(const BfcpMsg &msg);
  bool tryHandleResponse(const BfcpMsg &msg);
  bool tryHandleRequest(const BfcpMsg &msg);
  void onRequestTimeout(const ClientTransactionPtr &ctran);
  void onMessageInLoop(const BfcpMsg &msg);
  void onTimer();

  void sendFloorRequestInLoop(const BasicRequestParam &basicParam, const FloorRequestParam &floorRequest);
  void sendFloorReleaseInLoop(const BasicRequestParam &basicParam, uint16_t floorRequestID);
  void sendFloorRequestQueryInLoop(const BasicRequestParam &basicParam, uint16_t floorRequestID);
  void sendUserQueryInLoop(const BasicRequestParam &basicParam, const UserQueryParam &userQuery);
  void sendFloorQueryInLoop(const BasicRequestParam &basicParam, const bfcp_floor_id_list &floorIDs);
  void sendChairActionInLoop(const BasicRequestParam &basicParam, const FloorRequestInfoParam &frqInfo);
  void sendHelloInLoop(const BasicRequestParam &basicParam);
  void sendGoodByeInLoop(const BasicRequestParam &basicParam);

  void replyWithUserStatusInLoop(const BfcpMsg &msg, const UserStatusParam &userStatus);
  void replyWithChairActionAckInLoop(const BfcpMsg &msg);
  void replyWithHelloAckInLoop(const BfcpMsg &msg, const HelloAckParam &helloAck);
  void replyWithErrorInLoop(const BfcpMsg &msg, const ErrorParam &error);
  void replyWithFloorRequestStatusAckInLoop(const BfcpMsg &msg);
  void replyWithFloorStatusAckInLoop(const BfcpMsg &msg);
  void replyWithGoodByeAckInLoop(const BfcpMsg &msg);
  void replyWithFloorRequestStatusInLoop(const BfcpMsg &msg, const FloorRequestInfoParam &frqInfo);
  void replyWithFloorStatusInLoop(const BfcpMsg &msg, const FloorStatusParam &floorStatus);

  void notifyFloorRequestStatusInLoop(const BasicRequestParam &basicParam, 
                                      const FloorRequestInfoParam &frqInfo);
  void notifyFloorStatusInLoop(const BasicRequestParam &basicParam, 
                               const FloorStatusParam &floorStatus);
  
  inline void initEntity(bfcp_entity &entity, uint32_t cid, uint16_t uid);
  inline uint16_t getNextTransactionID();

  void startNewClientTransaction(const muduo::net::InetAddress &dst,
                                 const bfcp_entity &entity,
                                 mbuf_t *msgBuf,
                                 const ResponseCallback &cb);

  void startNewServerTransaction(const muduo::net::InetAddress &dst,
                                 const bfcp_entity &entity,
                                 bfcp_prim primitive,
                                 mbuf_t *msgBuf);

private:
  muduo::net::EventLoop *loop_;
  muduo::net::UdpSocketPtr socket_;
  std::map<::bfcp_entity, ClientTransactionPtr> ctrans_;  // use hash_map?
  NewRequestCallback newRequestCallback_;

  boost::circular_buffer<ReplyBucket> cachedReplys_;
  muduo::net::TimerId replyMsgTimer_;
  
  AtomicUInt16 nextTid_;
};

// TODO: use variadic template
template <typename Func, typename Arg1>
void BfcpConnection::runInLoop(Func func, const Arg1 &arg1)
{
  if (loop_->isInLoopThread())
  {
    (this->*func)(arg1);
  }
  else
  {
    loop_->runInLoop(
      boost::bind(func, 
      this, // FIXME
      arg1)); // std::move?
  }
}

template <typename Func, typename Arg1, typename Arg2>
void BfcpConnection::runInLoop(Func func, const Arg1 &arg1, const Arg2 &arg2)
{
  if (loop_->isInLoopThread())
  {
    (this->*func)(arg1, arg2);
  }
  else
  {
    loop_->runInLoop(
      boost::bind(func, 
      this, // FIXME
      arg1, // std::move?
      arg2)); // std::move?
  }
}

inline void BfcpConnection::initEntity(bfcp_entity &entity, uint32_t cid, uint16_t uid) 
{
  entity.conferenceID = cid;
  entity.transactionID = getNextTransactionID();
  entity.userID = uid;
}

inline uint16_t BfcpConnection::getNextTransactionID()
{
  uint16_t tid = 0;
  while ((tid = nextTid_.incrementAndGet()) == 0) {};
  return tid;
}

} // namespace bfcp




#endif // BFCP_CONN_H