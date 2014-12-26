#include "common/bfcp_msg.h"

using muduo::net::Buffer;
using muduo::net::InetAddress;

namespace bfcp
{

BfcpMsg::BfcpMsg(Buffer *buf, const InetAddress &src)
   : msg_(nullptr)
{
  mbuf_t mb;
  mbuf_init(&mb);
  mb.buf = reinterpret_cast<uint8_t*>(const_cast<char*>(buf->peek()));
  mb.size = buf->readableBytes();
  mb.end = mb.size;
  err_ = bfcp_msg_decode(&msg_, &mb);
  buf->retrieveAll();

  if (err_ == 0)
    setSrc(src);
}

std::list<BfcpAttr> BfcpMsg::getAttributes() const
{
  std::list<BfcpAttr> attrs;
  struct le *le = list_head(&msg_->attrl);
  while (le)
  {
    bfcp_attr_t *attr = static_cast<bfcp_attr_t*>(le->data);
    le = le->next;
    attrs.push_back(BfcpAttr(*attr));
  }
  return attrs;
}

std::list<BfcpAttr> BfcpMsg::findAttributes( ::bfcp_attrib attrType ) const
{
  std::list<BfcpAttr> attrs;
  struct le *le = list_head(&msg_->attrl);
  while (le)
  {
    bfcp_attr_t *attr = static_cast<bfcp_attr_t*>(le->data);
    le = le->next;
    if (attr->type == attrType)
      attrs.push_back(BfcpAttr(*attr));
  }
  return attrs;
}

} // namespace bfcp