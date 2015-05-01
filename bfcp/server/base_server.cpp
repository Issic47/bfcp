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

const double BaseServer::kDefaultUserObsoletedTime = 30;

BaseServer::BaseServer(muduo::net::EventLoop* loop, 
                       const muduo::net::InetAddress& listenAddr) 
   : loop_(CHECK_NOTNULL(loop)),
     server_(loop, listenAddr, "BfcpServer", UdpServer::kReuseAddr),
     connectionLoop_(loop),
     numThreads_(0),
     threadPool_(new ThreadPool("BfcpServerThreadPool")),
     enableConnectionThread_(false),
     userObsoletedTime_(kDefaultUserObsoletedTime)
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
  if (!connection_)
  {
    connection_ = boost::make_shared<BfcpConnection>(connectionLoop_, socket);
    connection_->setNewRequestCallback(
      boost::bind(&BaseServer::onNewRequest, this, _1));
  }
  connection_->onMessage(buf, src, time);
}

void BaseServer::onWriteComplete( const UdpSocketPtr& socket, int messageId )
{
  LOG_TRACE << "Message " << messageId << " write completed";
}

void BaseServer::enableConnectionThread()
{
  // base loop for UdpServer receiving msg
  // and the extract one for BfcpConnection to send msg and dispatch msg
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
    threadPool_->stop();
    if (connection_)
    {
      connection_->stopCacheTimer();
      connection_ = nullptr;
    }
    if (enableConnectionThread_)
    {
      connectionThread_.reset(nullptr);
    }
    server_.stop();
  }
}

void BaseServer::addConference(uint32_t conferenceID, 
                               const ConferenceConfig &config, 
                               const ResultCallback &cb)
{
  if (connectionLoop_->isInLoopThread())
  {
    addConferenceInLoop(conferenceID, config, cb);
  }
  else
  {
    connectionLoop_->runInLoop(
      boost::bind(&BaseServer::addConferenceInLoop,
      this, conferenceID, config, cb));
  }
}

void BaseServer::addConferenceInLoop(uint32_t conferenceID, 
                                     const ConferenceConfig &config,
                                     const ResultCallback &cb)
{
  LOG_TRACE << "{conferenceID: " << conferenceID
            << ", config: {maxFloorRequest: " << config.maxFloorRequest
            << ", policy: " << toString(config.acceptPolicy)
            << ", timeForChairAction: " << config.timeForChairAction 
            << "}}";
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
    ConferenceConfig conferenceConfig;
    conferenceConfig.maxFloorRequest = config.maxFloorRequest;
    conferenceConfig.acceptPolicy = config.acceptPolicy;
    conferenceConfig.timeForChairAction = config.timeForChairAction;
    conferenceConfig.userObsoletedTime = userObsoletedTime_;

    ConferencePtr newConference = 
      boost::make_shared<Conference>(
      connectionLoop_, 
      connection_,
      conferenceID,
      conferenceConfig);

    newConference->setChairActionTimeoutCallback(
      boost::bind(&BaseServer::onChairActionTimeout, this, _1, _2));
    newConference->setHoldingTimeoutCallback(
      boost::bind(&BaseServer::onHoldingFloorsTimeout, this, _1, _2));
    newConference->setClientReponseCallback(
      boost::bind(&BaseServer::onResponse, this, _1, _2, _3, _4, _5));
    conferenceMap_.insert(lb, std::make_pair(conferenceID, newConference));
    int res = threadPool_->createQueue(conferenceID, 0);
    (void)(res);
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

void BaseServer::modifyConference(uint32_t conferenceID, 
                                  const ConferenceConfig &config, 
                                  const ResultCallback &cb)
{
  runInLoop(&BaseServer::modifyConferenceInLoop, conferenceID, config, cb);
}

void BaseServer::modifyConferenceInLoop(uint32_t conferenceID, 
                                        const ConferenceConfig &config,
                                        const ResultCallback &cb)
{
  LOG_TRACE << "{conferenceID: " << conferenceID
            << ", config: {maxFloorRequest: " << config.maxFloorRequest
            << ", policy: " << toString(config.acceptPolicy)
            << ", timeForChairAction: " << config.timeForChairAction 
            << "}}";
  runTask(&Conference::set, conferenceID, config, cb);
}

void BaseServer::addFloor(uint32_t conferenceID, 
                          uint16_t floorID, 
                          const FloorConfig &config, 
                          const ResultCallback &cb)
{
  runInLoop(&BaseServer::addFloorInLoop, conferenceID, floorID, config, cb);
}

void BaseServer::addFloorInLoop(uint32_t conferenceID, 
                                uint16_t floorID, 
                                const FloorConfig &config,
                                const ResultCallback &cb)
{
  LOG_TRACE << "Add Floor " << floorID << " to Conference " << conferenceID;
  runTask(&Conference::addFloor, conferenceID, floorID, config, cb);
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

void BaseServer::modifyFloor(uint32_t conferenceID, 
                             uint16_t floorID, 
                             const FloorConfig &config, 
                             const ResultCallback &cb)
{
  runInLoop(
    &BaseServer::modifyFloorInLoop, 
    conferenceID, floorID, config, cb);
}

void BaseServer::modifyFloorInLoop(uint32_t conferenceID, 
                                   uint16_t floorID, 
                                   const FloorConfig &config, 
                                   const ResultCallback &cb)
{
  LOG_TRACE << "Modify floor with {conferenceID: " << conferenceID
            << ", floorID: " << floorID
            << ", maxGrantedNum: " << config.maxGrantedNum << "}";
  runTask(&Conference::modifyFloor,
    conferenceID, floorID, config, cb);
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

void BaseServer::setChair( uint32_t conferenceID, uint16_t floorID, uint16_t userID, const ResultCallback &cb )
{
  runInLoop(&BaseServer::setChairInLoop, conferenceID, floorID, userID, cb);
}

void BaseServer::setChairInLoop( uint32_t conferenceID, uint16_t floorID, uint16_t userID, const ResultCallback &cb )
{
  LOG_TRACE << "Set Chair " << userID << " of Floor " << floorID 
            << " in Conference " << conferenceID;
  runTask(&Conference::setChair, conferenceID, floorID, userID, cb);
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

void BaseServer::getConferenceIDs( const ResultWithDataCallback &cb )
{
  runInLoop(&BaseServer::getConferenceIDsInLoop, cb);
}

void BaseServer::getConferenceIDsInLoop(const ResultWithDataCallback &cb)
{
  LOG_TRACE << "Get conference IDs";
  connectionLoop_->assertInLoopThread();
  ConferenceIDList conferenceIDs;
  conferenceIDs.reserve(conferenceMap_.size());
  for (auto &conference : conferenceMap_)
  {
    conferenceIDs.push_back(conference.first);
  }
  cb(ControlError::kNoError, &conferenceIDs);
}

void BaseServer::getConferenceInfo(uint32_t conferenceID, 
                                   const ResultWithDataCallback &cb)
{
  runInLoop(&BaseServer::getConferenceInfoInLoop, conferenceID, cb);
}

void BaseServer::getConferenceInfoInLoop(uint32_t conferenceID, 
                                         const ResultWithDataCallback &cb)
{
  LOG_TRACE << "Get Information of Conference " << conferenceID;
  connectionLoop_->assertInLoopThread();
  auto it = conferenceMap_.find(conferenceID);
  if (it == conferenceMap_.end())
  {
    if (cb)
    {
      LOG_TRACE << "Conference " << conferenceID << " not exist";
      cb(ControlError::kConferenceNotExist, nullptr);
    }
  }
  else
  {
    auto task = boost::bind(&Conference::getConferenceInfo, (*it).second);
    // FIXME: use a wrap func instead
    threadPool_->run(
      conferenceID, 
      [task, cb]() {
        auto res = task();
        if (cb) 
        {
          cb(ControlError::kNoError, &res);
        }
      },
      ThreadPool::kHighPriority);
  }
}

template <typename Func, typename Arg1>
void BaseServer::runInLoop( Func func, const Arg1 &arg1 )
{
  if (connectionLoop_->isInLoopThread())
  {
    (this->*func)(arg1);
  }
  else
  {
    connectionLoop_->runInLoop(
      boost::bind(func, this, arg1));
  }
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

void BaseServer::onNewRequest( const BfcpMsgPtr &msg )
{
  LOG_TRACE << "BfcpServer received new request " << msg->toString();
  connectionLoop_->assertInLoopThread();
  auto it = conferenceMap_.find(msg->getConferenceID());
  if (it == conferenceMap_.end()) // conference not found
  {
    LOG_WARN << "Received new request for not existed conference " 
             << msg->getConferenceID();

    ErrorParam param;
    param.errorCode.code = BFCP_CONF_NOT_EXIST;
    char errorInfo[64];
    snprintf(errorInfo, sizeof errorInfo, 
      "Conference %u does not exist", msg->getConferenceID());
    param.setErrorInfo(errorInfo);
    connection_->replyWithError(msg, param);
  }
  else // conference found
  {
    int res = threadPool_->run(
      msg->getConferenceID(), 
      boost::bind(&Conference::onNewRequest, (*it).second, msg),
      ThreadPool::kNormalPriority);
    (void)(res);
    assert(res == 0);
  }
}

void BaseServer::onResponse(uint32_t conferenceID, 
                            bfcp_prim expectedPrimitive, 
                            uint16_t userID,
                            ResponseError err, 
                            const BfcpMsgPtr &msg)
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
      boost::bind(&Conference::onResponse, 
        (*it).second, expectedPrimitive, userID, err, msg),
      ThreadPool::kNormalPriority);
    (void)(res);
    assert(res == 0);
  }
}

void BaseServer::onChairActionTimeout(uint32_t conferenceID, 
                                      uint16_t floorRequestID)
{
  LOG_TRACE << "Chair action timeout with {conferenceID:" << conferenceID
            << ", floorRequestID:" << floorRequestID << "}";
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
    (void)(res);
    assert(res == 0);
  }
}

void BaseServer::onHoldingFloorsTimeout(uint32_t conferenceID, 
                                        uint16_t floorRequestID)
{
  LOG_TRACE << "Holding Floor(s) timeout with {conferenceID:" << conferenceID
            << ", floorRequestID:" << floorRequestID << "}";
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
      boost::bind(&Conference::onTimeoutForHoldingFloors, 
        (*it).second, floorRequestID),
      ThreadPool::kHighPriority);
    (void)(res);
    assert(res == 0);
  }
}


} // namespace bfcp