#ifndef BFCP_CONN_H
#define BFCP_CONN_H

#include <boost/noncopyable.hpp>
#include <boost/ptr_container/ptr_unordered_map.hpp>
#include <boost/weak_ptr.hpp>

#include <muduo/net/UdpSocket.h>
#include <muduo/net/TimerId.h>

#include "bfcp_msg.h"

namespace bfcp
{

class BfcpConnection : boost::noncopyable
{
public:
  BfcpConnection(const muduo::net::UdpSocketPtr socket);
  ~BfcpConnection();

private:
  boost::weak_ptr<muduo::net::UdpSocket> socket_;

};


} // namespace bfcp




#endif // BFCP_CONN_H