#ifndef BFCP_BUF_POOL_H
#define BFCP_BUF_POOL_H

#include <boost/noncopyable.hpp>
#include <muduo/base/Timestamp.h>
#include <muduo/base/BlockingQueue.h>
#include <muduo/base/Singleton.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/Buffer.h>

namespace bfcp
{

typedef struct BfcpBufNode
{
  static const size_t kMaxBufSize = 65536;

  BfcpBufNode() : buf(kMaxBufSize) {}
  muduo::net::InetAddress src;
  muduo::Timestamp time;
  muduo::net::Buffer buf;
} BfcpBufNode;

typedef boost::shared_ptr<BfcpBufNode> BfcpBufNodePtr;

class BfcpBufPool : boost::noncopyable
{
public:
  static const size_t kMaxBufNodeCount = 1000;

  BfcpBufPool(size_t maxBufCount = kMaxBufNodeCount)
    : usedBufCount_(0), maxBufCount_(maxBufCount)
  {
  }

  ~BfcpBufPool()
  {
    // FIXME(cbj): delete all bfcp buf node
    // NOTE: currently let the operation system do it.
  }

  BfcpBufNodePtr getFreeBufNode()
  {
    {
      muduo::MutexLockGuard lock(mutex_);
      if (bufQueue_.empty() && usedBufCount_ < kMaxBufNodeCount)
      {
        bufQueue_.put(boost::make_shared<BfcpBufNode>());
        ++usedBufCount_;
      }
    }
    return bufQueue_.take();
  }


  void releaseBufNode(BfcpBufNodePtr &bufNode)
  {
    bufQueue_.put(bufNode);
    bufNode = nullptr;
  }

private:
  size_t usedBufCount_;
  const size_t maxBufCount_;
  
  mutable muduo::MutexLock mutex_;
  muduo::BlockingQueue<BfcpBufNodePtr> bufQueue_;
};

typedef muduo::Singleton<BfcpBufPool> BfcpBufPoolSingleton;
typedef muduo::Singleton< muduo::BlockingQueue<BfcpBufNodePtr> > BfcpBufQueueSingleton;

} // namespace bfcp

#endif // BFCP_BUF_POOL_H