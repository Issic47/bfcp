#include "bfcp_ctrans.h"

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>

#include <boost/bind.hpp>

using muduo::net::UdpSocketPtr;
using muduo::net::EventLoop;

namespace bfcp
{

bfcp::ClientTransaction::ClientTransaction(const muduo::net::UdpSocketPtr &socket, 
                                           const muduo::net::InetAddress &dst, 
                                           const bfcp_entity &entity, 
                                           mbuf_t *msgBuf)
    : socket_(socket),
      dst_(dst), 
      entity_(entity),
      msgBuf_(msgBuf), 
      txc_(1)
{

}

ClientTransaction::~ClientTransaction()
{
  // TODO: use memory pool
  mem_deref(msgBuf_);
}

void ClientTransaction::onSendTimeout()
{
  LOG_DEBUG << "Client transaction to " << dst_.toIpPort() << " send timeout";
  UdpSocketPtr socket = socket_.lock();
  if (!socket)
  {
    return;
  }

  double delay = (BFCP_T1 << txc_) / 1000.0;
  int err = ETIMEDOUT;
  if (++txc_ > BFCP_TXC)
  {
    if (sendFailedCallback_)
    {
      sendFailedCallback_(shared_from_this());
      return;
    }
  }

  socket->send(dst_, msgBuf_->buf, msgBuf_->end);

  timer1_ = socket->getLoop()->runAfter(
    delay, boost::bind(&ClientTransaction::onSendTimeout, shared_from_this()));
}



} // namespace bfcp