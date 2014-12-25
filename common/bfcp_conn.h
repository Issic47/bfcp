#ifndef BFCP_CONN_H
#define BFCP_CONN_H

#include <boost/noncopyable.hpp>
#include <boost/ptr_container/ptr_unordered_map.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/shared_ptr.hpp>

#include <muduo/base/Atomic.h>
#include <muduo/net/UdpSocket.h>
#include <muduo/net/TimerId.h>

#include "bfcp_msg.h"

namespace bfcp
{

class ClientTransaction;
typedef boost::shared_ptr<ClientTransaction> ClientTransactionPtr;

class BfcpConnection : public boost::shared_ptr<BfcpConnection>,
                       boost::noncopyable
{
public:
  BfcpConnection(uint8_t version, const muduo::net::UdpSocketPtr &socket);
  ~BfcpConnection();

  /* request */
  void sendHello(uint32_t conferenceID, uint16_t userID);
  //void notify();
  //void reply();

private:
  typedef muduo::detail::AtomicIntegerT<uint16_t> AtomicUInt16;

  boost::weak_ptr<muduo::net::UdpSocket> socket_;
  uint8_t version_;
  std::vector<ClientTransactionPtr> ctrans_;
  AtomicUInt16 nextTid;
};


} // namespace bfcp




#endif // BFCP_CONN_H