#ifndef BFCP_MSG_H
#define BFCP_MSG_H

#include <muduo/net/Buffer.h>
#include <muduo/net/InetAddress.h>
#include <muduo/base/copyable.h>

#include <bfcp/common/bfcp_ex.h>
#include <bfcp/common/bfcp_attr.h>

namespace bfcp
{

class BfcpMsg : public muduo::copyable
{
public:
  BfcpMsg() : msg_(nullptr), err_(EINVAL) {}
  BfcpMsg(muduo::net::Buffer *buf, const muduo::net::InetAddress &src);
  
  BfcpMsg(const BfcpMsg &other) 
   : msg_(other.msg_), err_(other.err_)
  {
    mem_ref(other.msg_);
  }
  
  ~BfcpMsg() { 
    mem_deref(msg_); 
    msg_ = nullptr;
  }

  bool valid() const { return err_ == 0; }
  int error() const { return err_; }

  bool isResponse() const { return msg_->r == 1; }
  bool isFragement() const { return msg_->f == 1; }
  bfcp_prim primitive() const { return msg_->prim; }

  muduo::net::InetAddress getSrc() const 
  { return muduo::net::InetAddress(msg_->src.u.sa); }

  uint8_t getVersion() const { return msg_->ver; }
  uint32_t getConferenceID() const { return msg_->confid; }
  uint16_t getUserID() const { return msg_->userid; }
  uint16_t getTransactionID() const { return msg_->tid; }
  const bfcp_unknown_attr_t& getUnknownAttrs() const { return msg_->uma; }
  std::list<BfcpAttr> getAttributes() const;

  const ::bfcp_attr_t* findAttribute(::bfcp_attrib attrType) const 
  { return bfcp_msg_attr(msg_, attrType); }

  std::list<BfcpAttr> findAttributes(::bfcp_attrib attrType) const;

  bfcp_entity getEntity() const
  {
    bfcp_entity entity;
    entity.conferenceID = msg_->confid;
    entity.transactionID = msg_->tid;
    entity.userID = msg_->userid;
    return entity;
  }

  BfcpMsg& operator=(const BfcpMsg &other)
  {
    if (msg_ != other.msg_)
    {
      mem_deref(msg_);
      msg_ = other.msg_;
      mem_ref(other.msg_);
    }
    err_ = other.err_;
  }

  string toString() const;

private:
  void setSrc(const muduo::net::InetAddress &src)
  {
    auto &rawAddr = src.getRawSockAddr();
    ::memcpy(&msg_->src.u.sa, &rawAddr.u.sa, rawAddr.len);
    msg_->src.len = rawAddr.len;
  }

  ::bfcp_msg_t *msg_;
  int err_;
};

} // namespace bfcp

#endif // !BFCP_MSG_H
