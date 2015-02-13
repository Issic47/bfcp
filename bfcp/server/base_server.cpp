#include <bfcp/server/base_server.h>

#include <muduo/base/Logging.h>

#include <bfcp/common/bfcp_buf_pool.h>
#include <bfcp/common/bfcp_conn.h>
#include <bfcp/server/thread_pool.h>
#include <bfcp/server/conference.h>

using namespace muduo;
using namespace muduo::net;

namespace bfcp
{

BaseServer::BaseServer(EventLoop* loop, const InetAddress& listenAddr ) : 
  loop_(CHECK_NOTNULL(loop)),
  server_(loop, listenAddr, "BfcpServer", UdpServer::kReuseAddr),
  connectionLoop_(loop),
  numThreads_(0),
  threadPool_(new ThreadPool("BfcpServerThreadPool")),
  enableConnectionThread_(false)

{
  server_.setStartedRecvCallback(
    boost::bind(&BaseServer::onStartedRecv, this, _1));
  server_.setMessageCallback(
    boost::bind(&BaseServer::onMessage, this, _1, _2, _3, _4));
  server_.setWriteCompleteCallback(
    boost::bind(&BaseServer::onWriteComplete, this, _1, _2));
}

void BaseServer::onStartedRecv( const muduo::net::UdpSocketPtr& socket )
{
  LOG_TRACE << "Start receiving data at " << socket->getLocalAddr().toIpPort();
  if (!connection_)
  {
    connection_ = boost::make_shared<BfcpConnection>(connectionLoop_, socket);
    connection_->setNewRequestCallback(
      boost::bind(&BaseServer::onNewRequest, this, _1));
  }
}

void BaseServer::onMessage( const UdpSocketPtr& socket, Buffer* buf, const InetAddress& src, Timestamp time )
{
  LOG_TRACE << server_.name() << " recv " << buf->readableBytes() << " bytes at " << time.toString()
            << " from " << src.toIpPort();
  assert(connection_);
  connection_->onMessage(buf, src);
}

void BaseServer::onWriteComplete( const UdpSocketPtr& socket, int messageId )
{
  LOG_TRACE << "Message " << messageId << " write completed";
}

void BaseServer::enableConnectionThread()
{
  // base loop for UdpServer receiving msg
  // and the extract one for BfcpConnection to send msg and dipatch msg
  enableConnectionThread_ = true;
}

void BaseServer::start()
{
  if (started_.getAndSet(1) == 0)
  {
    if (enableConnectionThread_)
    {
      assert(!connection_);
      connectionThread_.reset(new EventLoopThread);
      connectionLoop_ = connectionThread_->startLoop();
    }
    threadPool_->setThreadInitCallback(workerThreadInitCallback_);
    threadPool_->start(numThreads_);
    server_.start();
  }
}

void BaseServer::stop()
{
  if (started_.getAndSet(0) == 1)
  {
    if (connection_)
    {
      connection_ = nullptr;
    }
    if (enableConnectionThread_)
    {
      connectionThread_.reset(nullptr);
    }
    threadPool_->stop();
    server_.stop();
  }
}

void BaseServer::addConference(uint32_t conferenceID, 
                               uint16_t maxFloorRequest, 
                               AcceptPolicy policy, 
                               double timeForChairAction, 
                               const ResultCallback &cb)
{
  if (connectionLoop_->isInLoopThread())
  {
    addConferenceInLoop(
      conferenceID, maxFloorRequest, policy, timeForChairAction, cb);
  }
  else
  {
    connectionLoop_->runInLoop(
      boost::bind(&BaseServer::addConferenceInLoop,
        this, conferenceID, maxFloorRequest, policy, timeForChairAction, cb));
  }
}

void BaseServer::addConferenceInLoop(uint32_t conferenceID, 
                                     uint16_t maxFloorRequest, 
                                     AcceptPolicy policy, 
                                     double timeForChairAction, 
                                     const ResultCallback &cb)
{
  LOG_TRACE << "Add Conference " << conferenceID
            << " (" << maxFloorRequest
            << ", " << toString(policy) 
            << ", " << timeForChairAction << ")";
  connectionLoop_->assertInLoopThread();
  auto lb = conferenceMap_.lower_bound(conferenceID);
  if (lb != conferenceMap_.end() && (*lb).first == conferenceID)
  {
    if (cb)
    {
      LOG_TRACE << "Conference " << conferenceID << " already exist";
      cb(ControlError::kConferenceAlreadyExist);
    }
  }
  else
  {
    ConferencePtr newConference = 
      boost::make_shared<Conference>(
        connectionLoop_, 
        connection_,
        conferenceID,
        maxFloorRequest,
        policy,
        timeForChairAction);
    conferenceMap_.insert(lb, std::make_pair(conferenceID, newConference));
    int res = threadPool_->createQueue(conferenceID, 0);
    assert(res == 0);
    if (cb)
    {
      cb(ControlError::kNoError);
    }
  }
}

void BaseServer::removeConference(uint32_t conferenceID, const ResultCallback &cb)
{
  runInLoop(&BaseServer::removeConferenceInLoop, conferenceID, cb);
}

void BaseServer::removeConferenceInLoop(uint32_t conferenceID, 
                                        const ResultCallback &cb)
{
  LOG_TRACE << "Remove Conference " << conferenceID;
  connectionLoop_->assertInLoopThread();
  auto it = conferenceMap_.find(conferenceID);
  if (it == conferenceMap_.end())
  {
    if (cb)
    {
      LOG_TRACE << "Conference " << conferenceID << " not exist";
      cb(ControlError::kConferenceNotExist);
    }
  }
  else
  {
    conferenceMap_.erase(it);
    threadPool_->releaseQueue(conferenceID);
    if (cb)
    {
      cb(ControlError::kNoError);
    }
  }
}

void BaseServer::changeMaxFloorRequest(uint32_t conferenceID, 
                                       uint16_t maxFloorRequest, 
                                       const ResultCallback &cb)
{
  runInLoop(
    &BaseServer::changeMaxFloorRequestInLoop, 
    conferenceID, maxFloorRequest, cb);
}

void BaseServer::changeMaxFloorRequestInLoop(uint32_t conferenceID, 
                                             uint16_t maxFloorRequest, 
                                             const ResultCallback &cb)
{
  LOG_TRACE << "Change max floor request to " << maxFloorRequest 
            << " in Conference " << conferenceID;
  runTask(&Conference::setMaxFloorRequest, conferenceID, maxFloorRequest, cb);
}

void BaseServer::changeAcceptPolicy(uint32_t conferenceID, 
                                    AcceptPolicy policy,
                                    double timeForChairAction, 
                                    const ResultCallback &cb)
{
  runInLoop(
    &BaseServer::changeAcceptPolicyInLoop, 
    conferenceID, policy, timeForChairAction, cb);
}

void BaseServer::changeAcceptPolicyInLoop(uint32_t conferenceID, 
                                          AcceptPolicy policy, 
                                          double timeForChairAction, 
                                          const ResultCallback &cb)
{
  LOG_TRACE << "Change accept policy to " << toString(policy) 
            << " with " << timeForChairAction 
            << "s for chair action in Conference " << conferenceID;
  runTask(&Conference::setAcceptPolicy, 
    conferenceID, policy, timeForChairAction, cb);
}

void BaseServer::addFloor(uint32_t conferenceID, 
                          uint16_t floorID, 
                          uint16_t maxGrantedNum, 
                          const ResultCallback &cb)
{
  runInLoop(&BaseServer::addFloorInLoop, conferenceID, floorID, maxGrantedNum, cb);
}

void BaseServer::addFloorInLoop(uint32_t conferenceID, 
                                uint16_t floorID,
                                uint16_t maxGrantedNum, 
                                const ResultCallback &cb)
{
  LOG_TRACE << "Add Floor " << floorID << " to Conference " << conferenceID;
  runTask(&Conference::addFloor, conferenceID, floorID, maxGrantedNum, cb);
}

void BaseServer::removeFloor(uint32_t conferenceID, 
                             uint16_t floorID, 
                             const ResultCallback &cb)
{
  runInLoop(&BaseServer::removeFloorInLoop, conferenceID, floorID, cb);
}

void BaseServer::removeFloorInLoop(uint32_t conferenceID, 
                                   uint16_t floorID, 
                                   const ResultCallback &cb)
{
  LOG_TRACE << "Remove Floor " << floorID 
            << " from Conference " << conferenceID;
  runTask(&Conference::removeFloor, conferenceID, floorID, cb);
}


void BaseServer::changeMaxGrantedNum(uint32_t conferenceID, 
                                     uint16_t floorID, 
                                     uint16_t maxGrantedNum, 
                                     const ResultCallback &cb)
{
  runInLoop(
    &BaseServer::changeMaxGrantedNumInLoop, 
    conferenceID, floorID, maxGrantedNum, cb);
}

void BaseServer::changeMaxGrantedNumInLoop(uint32_t conferenceID, 
                                           uint16_t floorID, 
                                           uint16_t maxGrantedNum, 
                                           const ResultCallback &cb)
{
  LOG_TRACE << "Change max granted num to " << maxGrantedNum 
            << " of Floor " << floorID << " in Conference " << conferenceID;
  runTask(&Conference::setFloorMaxGrantedCount,
    conferenceID, floorID, maxGrantedNum, cb);
}

void BaseServer::addUser(uint32_t conferenceID, 
                         const UserInfoParam &user,
                         const ResultCallback &cb)
{
  runInLoop(&BaseServer::addUserInLoop, conferenceID, user, cb);
}

void BaseServer::addUserInLoop(uint32_t conferenceID, 
                               const UserInfoParam &user, 
                               const ResultCallback &cb)
{
  LOG_TRACE << "Add User " << user.id << " to Conference " << conferenceID;
  runTask(&Conference::addUser, conferenceID, user, cb);
}

void BaseServer::removeUser(uint32_t conferenceID, 
                            uint16_t userID, 
                            const ResultCallback &cb)
{
  runInLoop(&BaseServer::removeUserInLoop, conferenceID, userID, cb);
}

void BaseServer::removeUserInLoop(uint32_t conferenceID, 
                                  uint16_t userID, 
                                  const ResultCallback &cb)
{
  LOG_TRACE << "Remove User " << userID << " from Conference " << conferenceID;
  runTask(&Conference::removeUser, conferenceID, userID, cb);
}

void BaseServer::addChair( uint32_t conferenceID, uint16_t floorID, uint16_t userID, const ResultCallback &cb )
{
  runInLoop(&BaseServer::addChairInLoop, conferenceID, floorID, userID, cb);
}

void BaseServer::addChairInLoop( uint32_t conferenceID, uint16_t floorID, uint16_t userID, const ResultCallback &cb )
{
  LOG_TRACE << "Add Chair " << userID << " of Floor " << floorID 
            << " in Conference " << conferenceID;
  runTask(&Conference::addChair, conferenceID, floorID, userID, cb);
}

void BaseServer::removeChair( uint32_t conferenceID, uint16_t floorID, const ResultCallback &cb )
{
  runInLoop(&BaseServer::removeChairInLoop, conferenceID, floorID, cb);
}

void BaseServer::removeChairInLoop( uint32_t conferenceID, uint16_t floorID, const ResultCallback &cb )
{
  LOG_TRACE << "Remove Chair of Floor " << floorID 
            << " in Conference " << conferenceID;
  runTask(&Conference::removeChair, conferenceID, floorID, cb);
}

template <typename Func, typename Arg1, typename Arg2>
void BaseServer::runInLoop(Func func, const Arg1 &arg1, const Arg2 &arg2)
{
  if (connectionLoop_->isInLoopThread())
  {
    (this->*func)(arg1, arg2);
  }
  else
  {
    connectionLoop_->runInLoop(
      boost::bind(func, this, arg1, arg2));
  }
}

template <typename Func, typename Arg1, typename Arg2, typename Arg3>
void BaseServer::runInLoop( Func func, const Arg1 &arg1, const Arg2 &arg2, const Arg3 &arg3 )
{
  if (connectionLoop_->isInLoopThread())
  {
    (this->*func)(arg1, arg2, arg3);
  }
  else
  {
    connectionLoop_->runInLoop(
      boost::bind(func, this, arg1, arg2, arg3));
  }
}

template <typename Func, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
void BaseServer::runInLoop(Func func, const Arg1 &arg1, const Arg2 &arg2, const Arg3 &arg3, const Arg4 &arg4)
{
  if (connectionLoop_->isInLoopThread())
  {
    (this->*func)(arg1, arg2, arg3, arg4);
  }
  else
  {
    connectionLoop_->runInLoop(
      boost::bind(func, this, arg1, arg2, arg3, arg4));
  }
}

template <typename Func, typename Arg1>
void BaseServer::runTask(Func func, 
                         uint32_t conferenceID, 
                         const Arg1 &arg1, 
                         const ResultCallback &cb)
{
  connectionLoop_->assertInLoopThread();
  auto it = conferenceMap_.find(conferenceID);
  if (it == conferenceMap_.end())
  {
    if (cb)
    {
      cb(ControlError::kConferenceNotExist);
    }
  }
  else
  {
    ConferenceTask task = 
      boost::bind(func, (*it).second, arg1);
    threadPool_->run(
      conferenceID, 
      boost::bind(&BaseServer::wrapTaskAndCallback, this, task, cb),
      ThreadPool::kHighPriority);
  }
}

template <typename Func, typename Arg1, typename Arg2>
void BaseServer::runTask(Func func, 
                         uint32_t conferenceID, 
                         const Arg1 &arg1, 
                         const Arg2 &arg2, 
                         const ResultCallback &cb)
{
  connectionLoop_->assertInLoopThread();
  auto it = conferenceMap_.find(conferenceID);
  if (it == conferenceMap_.end())
  {
    if (cb)
    {
      cb(ControlError::kConferenceNotExist);
    }
  }
  else
  {
    ConferenceTask task = 
      boost::bind(func, (*it).second, arg1, arg2);
    threadPool_->run(
      conferenceID, 
      boost::bind(&BaseServer::wrapTaskAndCallback, this, task, cb),
      ThreadPool::kHighPriority);
  }
}

void BaseServer::onNewRequest( const BfcpMsg &msg )
{
  LOG_TRACE << "BfcpServer received new request " << msg.toString();
  connectionLoop_->assertInLoopThread();
  auto it = conferenceMap_.find(msg.getConferenceID());
  if (it == conferenceMap_.end()) // conference not found
  {
    LOG_WARN << "Received new request for not existed conference " 
             << msg.getConferenceID();

    ErrorParam param;
    param.errorCode.code = BFCP_CONF_NOT_EXIST;
    char errorInfo[64];
    snprintf(errorInfo, sizeof errorInfo, 
      "Conference %lu does not exist", msg.getConferenceID());
    param.setErrorInfo(errorInfo);
    connection_->replyWithError(msg, param);
  }
  else // conference found
  {
    int res = threadPool_->run(
      msg.getConferenceID(), 
      boost::bind(&Conference::onNewRequest, (*it).second, msg),
      ThreadPool::kNormalPriority);
    assert(res == 0);
  }
}

void BaseServer::onResponse(uint32_t conferenceID, 
                            bfcp_prim expectedPrimitive, 
                            uint16_t userID,
                            ResponseError err, 
                            const BfcpMsg &msg)
{
  LOG_TRACE << "BfcpServer received response";
  connectionLoop_->assertInLoopThread();
  auto it = conferenceMap_.find(msg.getConferenceID());
  if (it == conferenceMap_.end()) // conference not found
  {
    LOG_ERROR << "Conference " << conferenceID << " not found";
  }
  else
  {
    int res = threadPool_->run(
      conferenceID,
      boost::bind(&Conference::onResponse, 
        (*it).second, expectedPrimitive, userID, err, msg),
      ThreadPool::kNormalPriority);
    assert(res == 0);
  }
}

void BaseServer::onChairActionTimeout(uint32_t conferenceID, 
                                      uint16_t floorRequestID)
{
  LOG_TRACE << "BfcpServer received response";
  connectionLoop_->assertInLoopThread();
  auto it = conferenceMap_.find(conferenceID);
  if (it == conferenceMap_.end()) // conference not found
  {
    LOG_ERROR << "Conference " << conferenceID << " not found";
  }
  else
  {
    int res = threadPool_->run(
      conferenceID,
      boost::bind(&Conference::onTimeoutForChairAction, 
        (*it).second, floorRequestID),
      ThreadPool::kNormalPriority);
    assert(res == 0);
  }
}

} // namespace bfcp