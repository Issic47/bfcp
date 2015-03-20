#include <bfcp/common/bfcp_msg.h>

#include <muduo/base/Logging.h>

using muduo::net::Buffer;
using muduo::net::InetAddress;
using muduo::strerror_tl;

namespace bfcp
{

namespace detail
{

int printToString(const char *p, size_t size, void *arg)
{
  string *str = static_cast<string*>(arg);
  str->append(p, p + size);
  return 0;
}

} // namespace detail

BfcpMsg::BfcpMsg(muduo::net::Buffer *buf,
                 const muduo::net::InetAddress &src, 
                 muduo::Timestamp receivedTime)
   : msg_(nullptr), receivedTime_(receivedTime)
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

string BfcpMsg::toString() const
{
  char buf[256];
  if (!valid()) 
  {
    snprintf(buf, sizeof buf, "{errcode=%d,errstr='%s'}",
      err_, strerror_tl(err_));
  }
  else
  {
    snprintf(buf, sizeof buf, "{ver=%u,cid=%u,tid=%u,uid=%u,prim=%s}",
      msg_->ver, msg_->confid, msg_->tid, msg_->userid, 
      bfcp_prim_name(msg_->prim));
  }
  return buf;
  
}

bfcp_floor_id_list BfcpMsg::getFloorIDs() const
{
  bfcp_floor_id_list floorIDs;
  auto attrs = findAttributes(BFCP_FLOOR_ID);
  floorIDs.reserve(attrs.size());
  for (auto &attr : attrs)
  {
    floorIDs.push_back(attr.getFloorID());
  }
  return floorIDs;
}

string BfcpMsg::toStringInDetail() const
{
  string str;
  struct re_printf printFunc;
  printFunc.vph = &detail::printToString;
  printFunc.arg = &str;
  bfcp_msg_print(&printFunc, msg_);
  return str;
}

} // namespace bfcp