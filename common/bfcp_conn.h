#ifndef BFCP_CONN_H
#define BFCP_CONN_H

#include <boost/noncopyable.hpp>
#include <boost/ptr_container/ptr_unordered_map.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/shared_ptr.hpp>

#include <muduo/net/Callbacks.h>
#include <muduo/base/Atomic.h>
#include <muduo/net/TimerId.h>

#include "bfcp_callbacks.h"

#include <vector>

namespace bfcp
{

class ClientTransaction;
typedef boost::shared_ptr<ClientTransaction> ClientTransactionPtr;

class BfcpMsg;

class BfcpConnection : public boost::shared_ptr<BfcpConnection>,
                       boost::noncopyable
{
public:
  BfcpConnection(const muduo::net::UdpSocketPtr &socket);
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
  typedef muduo::detail::AtomicIntegerT<uint16_t> AtomicUInt16;
  void handleParseError(int err, const muduo::net::InetAddress &src);
  bool tryHandleResponse(const BfcpMsg &msg);
  bool tryHandleRequest(const BfcpMsg &msg);

  muduo::net::UdpSocketPtr socket_;
  std::vector<ClientTransactionPtr> ctrans_;
  NewRequestCallback newRequestCallback_;
  AtomicUInt16 nextTid;
};


} // namespace bfcp




#endif // BFCP_CONN_H