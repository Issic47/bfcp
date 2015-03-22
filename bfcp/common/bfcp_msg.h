#ifndef BFCP_MSG_H
#define BFCP_MSG_H

#include <set>
#include <boost/scoped_ptr.hpp>

#include <muduo/net/Buffer.h>
#include <muduo/net/InetAddress.h>
#include <muduo/base/copyable.h>
#include <muduo/base/Timestamp.h>

#include <bfcp/common/bfcp_ex.h>
#include <bfcp/common/bfcp_attr.h>

namespace bfcp
{

class Fragment : public muduo::copyable
{
public:
  Fragment(uint16_t offset, uint16_t len, mbuf_t *buf)
    : offset_(offset), len_(len), buf_(buf)
  {
    mem_ref(buf_);
  }

  Fragment(const Fragment &other) 
    : offset_(other.offset_), len_(other.len_), buf_(other.buf_)
  {
    mem_ref(other.buf_);
  }

  ~Fragment()
  {
    mem_deref(buf_);
  }

  Fragment& operator=(const Fragment &other)
  {
    offset_ = other.offset_;
    len_ = other.len_;
    mem_ref(other.buf_);
    mem_deref(buf_);
    buf_ = other.buf_;
  }

  uint16_t getOffset() const { return offset_; }
  uint16_t getLen() const { return len_; }
  mbuf_t* getBuf() const { return buf_; }

  bool operator<(const Fragment &rhs) const
  {
    return offset_ < rhs.offset_;
  }

private:
  uint16_t offset_; // in 4-octet unit
  uint16_t len_; // in 4-octet unit
  mbuf_t *buf_;
};

class BfcpMsg : public muduo::copyable
{
public:
  BfcpMsg() 
    : msg_(nullptr), 
      err_(EINVAL), 
      isComplete_(true)
  {}

  BfcpMsg(muduo::net::Buffer *buf, 
          const muduo::net::InetAddress &src, 
          muduo::Timestamp receivedTime);
  
  BfcpMsg(const BfcpMsg &other) 
   : msg_(other.msg_), 
     err_(other.err_),
     receivedTime_(other.receivedTime_),
     isComplete_(other.isComplete_)
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
  bool isFragment() const { return msg_->f == 1; }
  bfcp_prim primitive() const { return msg_->prim; }

  muduo::net::InetAddress getSrc() const 
  { return muduo::net::InetAddress(msg_->src.u.sa); }

  muduo::Timestamp getReceivedTime() const { return receivedTime_; }

  uint8_t getVersion() const { return msg_->ver; }
  uint32_t getConferenceID() const { return msg_->confid; }
  uint16_t getUserID() const { return msg_->userid; }
  uint16_t getTransactionID() const { return msg_->tid; }
  uint16_t getLength() const { return msg_->len; }
  uint16_t getOffset() const { return msg_->fragoffset; }
  uint16_t getOffsetLen() const { return msg_->fraglen; }
  mbuf_t* getFragmentData() const { return msg_->fragdata; }

  const bfcp_unknown_attr_t& getUnknownAttrs() const { return msg_->uma; }
  std::list<BfcpAttr> getAttributes() const;

  const ::bfcp_attr_t* findAttribute(::bfcp_attrib attrType) const 
  { return bfcp_msg_attr(msg_, attrType); }

  std::list<BfcpAttr> findAttributes(::bfcp_attrib attrType) const;

  bfcp_floor_id_list getFloorIDs() const;

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
    return *this;
  }

  string toString() const;
  string toStringInDetail() const;

  void addFragment(const BfcpMsg &msg);
  bool isComplete() const { return isComplete_; }

private:
  bool canMergeWith(const BfcpMsg &msg) const;
  bool checkComplete();
  void mergeFragments();

  void setSrc(const muduo::net::InetAddress &src)
  {
    auto &rawAddr = src.getRawSockAddr();
    ::memcpy(&msg_->src.u.sa, &rawAddr.u.sa, rawAddr.len);
    msg_->src.len = rawAddr.len;
  }

private:
  typedef std::set<Fragment> FragmentSet;

  ::bfcp_msg_t *msg_;
  int err_;
  muduo::Timestamp receivedTime_;
 
  boost::scoped_ptr<FragmentSet> fragments_;
  bool isComplete_;
};

} // namespace bfcp

#endif // !BFCP_MSG_H
