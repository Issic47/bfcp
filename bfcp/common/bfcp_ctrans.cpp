#include <bfcp/common/bfcp_ctrans.h>

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>

#include <boost/bind.hpp>

using muduo::net::UdpSocketPtr;
using muduo::net::EventLoop;
using muduo::net::InetAddress;

namespace bfcp
{

const char* response_error_name( ResponseError err )
{
  switch (err)
  {
    case ResponseError::kNoError: return "NoError";
    case ResponseError::kTimeout: return "Timeout";
    default: return "???";
  }
}

void defaultResponseCallback(ResponseError err, const BfcpMsg &msg)
{
  if (err != ResponseError::kNoError)
  {
    LOG_ERROR << "Response error: " << response_error_name(err);
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
  mem_ref(msgBuf);
}

ClientTransaction::~ClientTransaction()
{
  mem_deref(msgBuf_);
}

void ClientTransaction::start()
{
  UdpSocketPtr socket = socket_.lock();
  assert(socket);
  socket->send(dst_, msgBuf_->buf, msgBuf_->end);

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
    loop_->cancel(timer1_);
    if (requestTimeoutCallback_)
    {
      loop_->queueInLoop(
        boost::bind(requestTimeoutCallback_, shared_from_this()));
    }
    return;
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
  if (err != ResponseError::kNoError)
  {
    LOG_INFO << "Client transaction" << toString(entity_)
             << " on response with error: " << response_error_name(err);
  }
  else
  {
    assert(msg.valid());
    LOG_INFO << "Client transaction" << toString(entity_)
             << " on response with " << bfcp_prim_name(msg.primitive());
  }
  if (responseCallback_)
    loop_->queueInLoop(boost::bind(responseCallback_, err, msg));
}

} // namespace bfcp