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

#include "bfcp_callbacks.h"
#include "bfcp_ex.h"
#include "bfcp_mbuf_wrapper.h"

#include <vector>
#include <map>

namespace bfcp
{

class ClientTransaction;
class BfcpMsg;
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

  /* request */
  //void request();
  //void notify();
  //void reply();

private:
  static const int BFCP_T2_SEC = 10;

  typedef muduo::detail::AtomicIntegerT<uint16_t> AtomicUInt16;
  typedef MBufWrapper MBufPtr;
  // FIXME: assigned MbufPtr mem_ref
  typedef std::map<detail::bfcp_strans_entry, MBufPtr> ReplyBucket;

  bool tryHandleResponse(const BfcpMsg &msg);
  bool tryHandleRequest(const BfcpMsg &msg);
  void onRequestTimeout(const ClientTransactionPtr &ctran);
  void onMessageInLoop(const BfcpMsg &msg);
  void onTimer();

  muduo::net::EventLoop *loop_;
  muduo::net::UdpSocketPtr socket_;
  std::map<::bfcp_entity, ClientTransactionPtr> ctrans_;  // use hash_map?
  NewRequestCallback newRequestCallback_;

  boost::circular_buffer<ReplyBucket> cachedReplys_;
  muduo::net::TimerId replyMsgTimer_;
  
  AtomicUInt16 nextTid_;
};


} // namespace bfcp




#endif // BFCP_CONN_H