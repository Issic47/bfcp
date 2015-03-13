#ifndef BFCP_MBUF_WRAPPER_H
#define BFCP_MBUF_WRAPPER_H

#include <bfcp/common/bfcp_ex.h>

namespace bfcp
{

class MBufWrapper
{
public:
  MBufWrapper(mbuf_t *buf)
      : buf_(buf)
  {
    mem_ref(buf);
  }

  MBufWrapper(const MBufWrapper &other)
      : buf_(other.buf_)
  {
    mem_ref(other.buf_);
  }

  MBufWrapper(MBufWrapper &&other)
      : buf_(other.buf_)
  {
    other.buf_ = nullptr;
  }

  ~MBufWrapper()
  {
    mem_deref(buf_);
  }

  MBufWrapper& operator=(const MBufWrapper &other)
  {
    if (buf_ != other.buf_)
    {
      mem_deref(buf_);
      buf_ = other.buf_;
      mem_ref(other.buf_);
    }
    return *this;
  }

  MBufWrapper& operator=(MBufWrapper &&other)
  {
    if (buf_ != other.buf_)
    {
      mem_deref(buf_);
      buf_ = other.buf_;
      other.buf_ = nullptr;
    }
    return *this;
  }

  mbuf_t* operator->() { return buf_; }
  const mbuf_t* operator->() const { return buf_; }

  mbuf_t& operator*() { return *buf_; }
  const mbuf_t& operator*() const { return *buf_; }

private:
  mbuf_t *buf_;
};


} // namespace bfcp

#endif // BFCP_MBUF_WRAPPER_H