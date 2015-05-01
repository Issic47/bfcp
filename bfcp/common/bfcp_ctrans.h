#ifndef BFCP_CTRANS_H
#define BFCP_CTRANS_H

#include <vector>

#include <boost/noncopyable.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <muduo/net/UdpSocket.h>
#include <muduo/net/TimerId.h>
#include <muduo/net/InetAddress.h>

#include <bfcp/common/bfcp_msg.h>
#include <bfcp/common/bfcp_callbacks.h>

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

  typedef boost::function<void (const ClientTransactionPtr&)> RequestTimeoutCallback;

  ClientTransaction(muduo::net::EventLoop *loop,
                    const muduo::net::UdpSocketPtr &socket,
                    const muduo::net::InetAddress &dst, 
                    const bfcp_entity &entity,
                    std::vector<mbuf_t*> &msgBufs);

  ~ClientTransaction();

  // no thread safe
  void start();
  // no thread safe
  void onResponse(ResponseError err, const BfcpMsgPtr &msg);

  const bfcp_entity& getEntity() const { return entity_; }

  void setReponseCallback(const ResponseCallback &responseCallback)
  { responseCallback_ = responseCallback; }

  void setReponseCallback(ResponseCallback &&responseCallback)
  { responseCallback_ = std::move(responseCallback); }

  void setRequestTimeoutCallback(const RequestTimeoutCallback &requestTimeoutCallback)
  { requestTimeoutCallback_ = requestTimeoutCallback; }

  void setRequestTimeoutCallback(RequestTimeoutCallback &&requestTimeoutCallback)
  { requestTimeoutCallback_ = std::move(requestTimeoutCallback); }

private:
  void onSendTimeout();
  void sendBufs();

  muduo::net::EventLoop *loop_;
  boost::weak_ptr<muduo::net::UdpSocket> socket_;
  std::vector<mbuf_t*> bufs_;
  bfcp_entity entity_;
  muduo::net::InetAddress dst_;
  ResponseCallback responseCallback_;
  RequestTimeoutCallback requestTimeoutCallback_;
  muduo::net::TimerId timer1_;
  uint8_t txc_;
};

}

#endif // BFCP_CTRANS_H


