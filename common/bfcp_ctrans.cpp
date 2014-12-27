#include "bfcp_ctrans.h"

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>

#include <boost/bind.hpp>

using muduo::net::UdpSocketPtr;
using muduo::net::EventLoop;
using muduo::net::InetAddress;

namespace bfcp
{

void defaultResponseCallback(ResponseError err, const BfcpMsg &msg)
{
  if (err)
  {
    LOG_ERROR << "Response error: " << muduo::strerror_tl(err);
  }
}

ClientTransaction::ClientTransaction(EventLoop *loop,
                                     const UdpSocketPtr &socket, 
                                     const muduo::net::InetAddress &dst, 
                                     const bfcp_entity &entity, 
                                     mbuf_t *msgBuf)
    : loop_(CHECK_NOTNULL(loop)),
      socket_(socket),
      dst_(dst), 
      entity_(entity),
      msgBuf_(CHECK_NOTNULL(msgBuf)), 
      txc_(1),
      responseCallback_(defaultResponseCallback)
{

}

ClientTransaction::~ClientTransaction()
{
  mem_deref(msgBuf_);
}

void ClientTransaction::start()
{
  txc_ = 1;
  timer1_ = loop_->runAfter(
    BFCP_T1 / 1000.0, boost::bind(&ClientTransaction::onSendTimeout, shared_from_this()));
}

void ClientTransaction::onSendTimeout()
{
  loop_->assertInLoopThread();
  double delay = (BFCP_T1 << txc_) / 1000.0;
  if (++txc_ > BFCP_TXC)
  {
    LOG_WARN << "Client transaction("
             << entity_.conferenceID << ":"
             << entity_.transactionID << ":"
             << entity_.userID <<  ") timeout";

    if (requestTimeoutCallback_)
    {
      loop_->queueInLoop(
        boost::bind(requestTimeoutCallback_, shared_from_this()));
      return;
    }
  }

  UdpSocketPtr socket = socket_.lock();
  if (!socket)
  {
    LOG_WARN << "UDP socket has been destructed before ClientTransaction::onSendTimeout";
  }
  else
  {
    socket->send(dst_, msgBuf_->buf, msgBuf_->end);
    timer1_ = loop_->runAfter(
      delay, boost::bind(&ClientTransaction::onSendTimeout, this));
  }
}

void ClientTransaction::onResponse( ResponseError err, const BfcpMsg &msg )
{
  loop_->cancel(timer1_);
  loop_->queueInLoop(boost::bind(responseCallback_, err, msg));
}

} // namespace bfcp