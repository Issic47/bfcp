#include <bfcp/common/bfcp_msg.h>

#include <algorithm>
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

  isComplete_ = !valid() || !msg_->f;
  if (valid() && msg_->f)
  {
    fragments_.emplace(msg_->fragoffset, msg_->fraglen, msg_->fragdata);
  }
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

void BfcpMsg::addFragment( const BfcpMsgPtr &msg )
{
  assert(this != &(*msg));
  if (!valid() || !canMergeWith(msg))
  {
    LOG_WARN << "Cannot merge fragment: " << msg->toString();
    err_ = EBADMSG;
    return;
  }
  
  if (!isComplete_ && holes_.empty()) 
  {
    holes_.emplace_back(0, getLength() - 1);
    doAddFragment(this);
  }

  doAddFragment(&(*msg));

  if (holes_.empty()) 
  {
    isComplete_ = true;
    mergeFragments();
  }
}

bool BfcpMsg::canMergeWith( const BfcpMsgPtr &msg ) const
{
  return (isFragment() && msg->isFragment()) &&
         getEntity() == msg->getEntity() &&
         primitive() == msg->primitive() &&
         getLength() == msg->getLength() &&
         isResponse() == msg->isResponse();
}

void BfcpMsg::doAddFragment(const BfcpMsg *msg)
{
  bool isInHole = false;
  uint16_t fragOffset = msg->msg_->fragoffset;
  uint16_t fragBack = fragOffset + msg->msg_->fraglen - 1;
  for (auto it = holes_.begin(); it != holes_.end(); ++it)
  {
    if ((*it).front <= fragOffset && fragBack <= (*it).back) 
    {
      isInHole = true;
      if ((*it).front < fragOffset) 
      {
        holes_.emplace_back((*it).front, fragOffset - 1);
      }
      if (fragBack < (*it).back)
      {
        holes_.emplace_back(fragBack + 1, (*it).back);
      }
      it = holes_.erase(it);
      break;
    }
  }

  if (!isInHole) 
  {
    LOG_WARN << "Ignore fragment not in the hole: " << msg->toString();
  } 
  else 
  {
    fragments_.emplace(
      msg->msg_->fragoffset, msg->msg_->fraglen, msg->msg_->fragdata);
  }
}

bool BfcpMsg::checkComplete()
{
  isComplete_ = false;
  auto it = fragments_.begin();
  if ((*it).getOffset() != 0)
  {
    return false;
  }

  auto nextIt = it;
  ++nextIt;
  size_t len = it->getLen();
  for (; nextIt != fragments_.end(); ++it, ++nextIt)
  {
    size_t offsetEnd = (*it).getOffset() + (*it).getLen();
    if (offsetEnd < (*nextIt).getOffset())
    {
      return false;
    }
    else if (offsetEnd > (*nextIt).getOffset())
    {
      LOG_WARN << "Received overlap fragments: " << toString();
      err_ = EBADMSG;
      return false;
    }
    len += (*nextIt).getLen();
  }
  if (len == msg_->len)
  {
    isComplete_ = true;
    return true;
  }
  else if (len > msg_->len)
  {
    LOG_WARN << "Received too large fragments: " << toString();
    err_ = EBADMSG;
    return false;
  }
  return false;
}

void BfcpMsg::mergeFragments()
{
  assert(valid());

  uint8_t buf[65536];
  mbuf_t mb;
  mbuf_init(&mb);
  mb.buf = buf;
  mb.pos = 0;
  mb.end = 0;
  mb.size = sizeof buf;

  for (auto &fragment : fragments_)
  {
    mbuf_t *fragBuf = fragment.getBuf();
    err_ = mbuf_write_mem(&mb, fragBuf->buf, fragBuf->end);
    assert(err_ == 0);
  }
  mb.pos = 0;
  err_ = bfcp_attrs_decode(&msg_->attrl, &mb, 4 * msg_->len, &msg_->uma);
  fragments_.clear();
}

} // namespace bfcp