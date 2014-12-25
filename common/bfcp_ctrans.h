#ifndef BFCP_CTRANS_H
#define BFCP_CTRANS_H

#include <boost/noncopyable.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <muduo/net/UdpSocket.h>
#include <muduo/net/TimerId.h>
#include <muduo/net/InetAddress.h>

#include "bfcp_msg.h"
#include "bfcp_callbacks.h"

namespace bfcp
{

class ClientTransaction : public boost::enable_shared_from_this<ClientTransaction>, 
                          boost::noncopyable
{
public:
  enum 
  {
    BFCP_T1 = 500,
    BFCP_TXC = 4,
  };

  ClientTransaction(const muduo::net::UdpSocketPtr &socket,
                    const muduo::net::InetAddress &dst, 
                    const bfcp_entity &entity, 
                    mbuf_t *msgBuf);

  ~ClientTransaction();

  void setTimerId(const muduo::net::TimerId &timer1) { timer1_ = timer1; }

  void setReponseCallback(const ResponseCallback &responseCallback)
  { responseCallback_ = responseCallback; }

  void setReponseCallback(ResponseCallback &&responseCallback)
  { responseCallback_ = std::move(responseCallback); }

  void setSendFailedCallback(const SendFailedCallback &sendFailedCallback)
  { sendFailedCallback_ = sendFailedCallback; }

  void setSendFailedCallback(SendFailedCallback &&sendFailedCallback)
  { sendFailedCallback_ = std::move(sendFailedCallback); }

  void response(int err, const bfcp_msg_t *msg)
  { responseCallback_(err, msg); }

private:
  void onSendTimeout();

  boost::weak_ptr<muduo::net::UdpSocket> socket_;
  ResponseCallback responseCallback_;
  SendFailedCallback sendFailedCallback_;
  mbuf_t *msgBuf_;
  bfcp_entity entity_;
  muduo::net::InetAddress dst_;
  muduo::net::TimerId timer1_;
  uint8_t txc_;
};

}

#endif // BFCP_CTRANS_H


