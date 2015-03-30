#include "BfcpService.h"

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <bfcp/server/base_server.h>


namespace details
{

bfcp::AcceptPolicy convertTo(ns__Policy policy)
{
  switch (policy)
  {
    case kAutoAccept:
      return bfcp::AcceptPolicy::kAutoAccept; break;
    case kAutoDeny:
      return bfcp::AcceptPolicy::kAutoDeny; break;
    default:
      LOG_ERROR << "Unknown policy: " << static_cast<int>(policy)
                << " set as auto deny policy";
      return bfcp::AcceptPolicy::kAutoDeny; break;
  }
}

ns__ErrorCode convertTo(bfcp::ControlError error)
{
  switch (error)
  {
  case bfcp::ControlError::kNoError:
    return ns__ErrorCode::kNoError;
    break;
  case bfcp::ControlError::kUserNotExist:
    return ns__ErrorCode::kUserNotExist;
    break;
  case bfcp::ControlError::kUserAlreadyExist:
    return ns__ErrorCode::kUserAlreadyExist;
    break;
  case bfcp::ControlError::kFloorNotExist:
    return ns__ErrorCode::kFloorNotExist;
    break;
  case bfcp::ControlError::kFloorAlreadyExist:
    return ns__ErrorCode::kFloorAlreadyExist;
    break;
  case bfcp::ControlError::kChairNotExist:
    return ns__ErrorCode::kChairNotExost;
    break;
  case bfcp::ControlError::kChairAlreadyExist:
    return ns__ErrorCode::kChairAlreadyExist;
    break;
  case bfcp::ControlError::kConferenceNotExist:
    return ns__ErrorCode::kConferenceNotExist;
    break;
  case bfcp::ControlError::kConferenceAlreadyExist:
    return ns__ErrorCode::kConferenceAlreadyExist;
    break;
  default:
    // FIXME: unknow error
    assert(false);
    return ns__ErrorCode::kNoError;
    break;
  }
}

} // namespace details

BfcpService::BfcpService()
  : BFCPServiceService(), 
    cond_(mutex_), 
    loop_(nullptr), 
    isRunning_(false)
{

}

BfcpService::BfcpService( const struct soap &soap )
  : BFCPServiceService(soap), 
    cond_(mutex_), 
    loop_(nullptr), 
    isRunning_(false)
{

}

BfcpService::BfcpService( soap_mode iomode )
  : BFCPServiceService(iomode), 
    cond_(mutex_), 
    loop_(nullptr), 
    isRunning_(false)
{

}

BfcpService::BfcpService( soap_mode imode, soap_mode omode )
  : BFCPServiceService(imode, omode),
    cond_(mutex_), 
    loop_(nullptr), 
    isRunning_(false)
{

}

BfcpService::~BfcpService()
{
  if (server_)
  {
    server_->stop();
    // NOTE: server_ should destructor in the loop thread
    loop_->runInLoop(boost::bind(&BfcpService::resetServer, std::move(server_)));
    server_ = nullptr;
  }
}

int BfcpService::run( int port )
{
  isRunning_ = true;
  char buf[1024];
  if (soap_valid_socket(this->master) || soap_valid_socket(bind(NULL, port, 100)))
  {	
    while (isRunning_)
    {
      if (!soap_valid_socket(accept()))
      {
        soap_sprint_fault(buf, sizeof buf);
        buf[sizeof(buf)-1] = '\0';
        LOG_WARN << buf;
        continue;
      }
      serve();
      soap_destroy(this);
      soap_end(this);
    }
  }
  return this->error;
}

BFCPServiceService * BfcpService::copy()
{
  BfcpService *dup = SOAP_NEW_COPY(BfcpService(*(struct soap*)this));
  return dup;
}

int BfcpService::start(enum ns__AddrFamily af, 
                       unsigned short port, 
                       bool enbaleConnectionThread, 
                       int workThreadNum, 
                       double userObsoletedTime, 
                       enum ns__ErrorCode *errorCode)
{
  LOG_INFO << "start {af:" << (af == kIPv4 ? "IPv4" : "IPv6")
           << ", port: " << port 
           << ", enableConnectionThread: " << (enbaleConnectionThread ? "true" : "false")
           << ", workThreadNum: " << workThreadNum 
           << ", userObsoletedTime: " << userObsoletedTime << "}";

  if (server_)
  {
    LOG_WARN << "Server already start";
    *errorCode = ns__ErrorCode::kServerAlreadyStart;
    return SOAP_OK;
  }

  if (!loop_)
  {
    loop_ = thread_.startLoop();
  }

  muduo::net::InetAddress listenAddr((af == kIPv4 ? AF_INET : AF_INET6), port);
  server_ = boost::make_shared<bfcp::BaseServer>(loop_, listenAddr);
  if (enbaleConnectionThread)
    server_->enableConnectionThread();
  server_->setWorkerThreadNum(workThreadNum);
  server_->start();

  *errorCode = ns__ErrorCode::kNoError;
  return SOAP_OK;
}

int BfcpService::stop( enum ns__ErrorCode *errorCode )
{
  LOG_INFO << "stop";
  if (!server_)
  {
    LOG_WARN << "Server not start";
    *errorCode = ns__ErrorCode::kServerNotStart;
    return SOAP_OK;
  }
  assert(loop_);
  server_->stop();
  // NOTE: server_ should destructor in the loop thread
  loop_->runInLoop(boost::bind(&BfcpService::resetServer, std::move(server_)));
  server_ = nullptr;

  *errorCode = ns__ErrorCode::kNoError;
  return SOAP_OK;
}

void BfcpService::resetServer( BaseServerPtr server )
{
  server = nullptr;
}

int BfcpService::quit()
{
  isRunning_ = false;
  return send_quit_empty_response(SOAP_OK);
}

int BfcpService::addConference(unsigned int conferenceID, 
                               unsigned short maxFloorRequest, 
                               enum ns__Policy policy, 
                               double timeForChairAction, 
                               enum ns__ErrorCode *errorCode)
{
  bfcp::AcceptPolicy acceptPolicy = details::convertTo(policy);
  LOG_INFO << "addConference with {conferenceID: " << conferenceID
           << ", maxFloorRequest: " << maxFloorRequest
           << ", policy: " << bfcp::toString(acceptPolicy)
           << ", timeforChairAction: " << timeForChairAction << "}";

  if (!server_)
  {
    LOG_WARN << "Server not start";
    *errorCode = ns__ErrorCode::kServerNotStart;
    return SOAP_OK;
  }

  callFinished_ = false;

  bfcp::ConferenceConfig config;
  config.maxFloorRequest = maxFloorRequest;
  config.acceptPolicy = acceptPolicy;
  config.timeForChairAction = timeForChairAction;

  server_->addConference(
    conferenceID, 
    config,
    boost::bind(&BfcpService::handleCallResult, this, _1));

  {
    muduo::MutexLockGuard lock(mutex_);
    while (!callFinished_)
    {
      cond_.wait();
    }
  }

  *errorCode = details::convertTo(error_);

  return SOAP_OK;
}

void BfcpService::handleCallResult( bfcp::ControlError error )
{
  muduo::MutexLockGuard lock(mutex_);
  error_ = error;
  callFinished_ = true;
  cond_.notify();
}


int BfcpService::removeConference(unsigned int conferenceID,
                                  enum ns__ErrorCode *errorCode)
{
  LOG_INFO << "removeConference with {conferenceID: " << conferenceID << "}";
  if (!server_)
  {
    LOG_WARN << "Server not start";
    *errorCode = ns__ErrorCode::kServerNotStart;
    return SOAP_OK;
  }
  callFinished_ = false;

  server_->removeConference(
    conferenceID, 
    boost::bind(&BfcpService::handleCallResult, this, _1));

  {
    muduo::MutexLockGuard lock(mutex_);
    while (!callFinished_)
    {
      cond_.wait();
    }
  }
  *errorCode = details::convertTo(error_);

  return SOAP_OK;
}

int BfcpService::modifyConference(unsigned int conferenceID, 
                                  unsigned short maxFloorRequest, 
                                  enum ns__Policy policy, 
                                  double timeForChairAction, 
                                  enum ns__ErrorCode *errorCode)
{
  bfcp::AcceptPolicy acceptPolicy = details::convertTo(policy);
  LOG_INFO << "addConference with {conferenceID: " << conferenceID
           << ", maxFloorRequest: " << maxFloorRequest
           << ", policy: " << bfcp::toString(acceptPolicy)
           << ", timeforChairAction: " << timeForChairAction << "}";

  if (!server_)
  {
    LOG_WARN << "Server not start";
    *errorCode = ns__ErrorCode::kServerNotStart;
    return SOAP_OK;
  }
  callFinished_ = false;

  bfcp::ConferenceConfig config;
  config.maxFloorRequest = maxFloorRequest;
  config.acceptPolicy = acceptPolicy;
  config.timeForChairAction = timeForChairAction;

  server_->modifyConference(
    conferenceID, 
    config,
    boost::bind(&BfcpService::handleCallResult, this, _1));

  {
    muduo::MutexLockGuard lock(mutex_);
    while (!callFinished_)
    {
      cond_.wait();
    }
  }
  *errorCode = details::convertTo(error_);

  return SOAP_OK;
}

int BfcpService::addFloor(unsigned int conferenceID, 
                          unsigned short floorID, 
                          unsigned short maxGrantedNum, 
                          double maxHoldingTime, 
                          enum ns__ErrorCode *errorCode)
{
  LOG_INFO << "addFloor with {conferenceID: " << conferenceID
           << ", floorID: " << floorID
           << ", maxGrantedNum: " << maxGrantedNum << "}";

  if (!server_)
  {
    LOG_WARN << "Server not start";
    *errorCode = ns__ErrorCode::kServerNotStart;
    return SOAP_OK;
  }
  callFinished_ = false;

  bfcp::FloorConfig config;
  config.maxGrantedNum = maxGrantedNum;
  config.maxHoldingTime = maxHoldingTime;

  server_->addFloor(
    conferenceID,
    floorID, 
    config,
    boost::bind(&BfcpService::handleCallResult, this, _1));

  {
    muduo::MutexLockGuard lock(mutex_);
    while (!callFinished_)
    {
      cond_.wait();
    }
  }
  *errorCode = details::convertTo(error_);

  return SOAP_OK;
}

int BfcpService::removeFloor( unsigned int conferenceID, unsigned short floorID, enum ns__ErrorCode *errorCode )
{
  LOG_INFO << "removeFloor with (conferenceID: " << conferenceID
           << ", floorID: " << floorID << ")";
  if (!server_)
  {
    LOG_WARN << "Server not start";
    *errorCode = ns__ErrorCode::kServerNotStart;
    return SOAP_OK;
  }
  callFinished_ = false;

  server_->removeFloor(
    conferenceID,
    floorID, 
    boost::bind(&BfcpService::handleCallResult, this, _1));

  {
    muduo::MutexLockGuard lock(mutex_);
    while (!callFinished_)
    {
      cond_.wait();
    }
  }
  *errorCode = details::convertTo(error_);

  return SOAP_OK;
}

int BfcpService::modifyFloor(unsigned int conferenceID, 
                             unsigned short floorID, 
                             unsigned short maxGrantedNum, 
                             double maxHoldingTime, 
                             enum ns__ErrorCode *errorCode)
{
  LOG_INFO << "modifyFloor with {conferenceID: " << conferenceID
           << ", floorID: " << floorID
           << ", maxGrantedNum: " << maxGrantedNum << "}";

  if (!server_)
  {
    LOG_WARN << "Server not start";
    *errorCode = ns__ErrorCode::kServerNotStart;
    return SOAP_OK;
  }

  callFinished_ = false;

  bfcp::FloorConfig config;
  config.maxGrantedNum = maxGrantedNum;
  config.maxHoldingTime = maxHoldingTime;

  server_->modifyFloor(
    conferenceID,
    floorID, 
    config,
    boost::bind(&BfcpService::handleCallResult, this, _1));

  {
    muduo::MutexLockGuard lock(mutex_);
    while (!callFinished_)
    {
      cond_.wait();
    }
  }
  *errorCode = details::convertTo(error_);

  return SOAP_OK;
}

int BfcpService::addUser(unsigned int conferenceID, 
                         unsigned short userID, 
                         std::string userName, 
                         std::string userURI, 
                         enum ns__ErrorCode *errorCode)
{
    LOG_INFO << "addUser with {conferenceID: " << conferenceID
             << ", userID: " << userID
             << ", userName: " << userName
             << ", userURI: " << userURI << "}";

  if (!server_)
  {
    LOG_WARN << "Server not start";
    *errorCode = ns__ErrorCode::kServerNotStart;
    return SOAP_OK;
  }

  callFinished_ = false;

  bfcp::UserInfoParam user;
  user.id = userID;
  user.username = userName;
  user.useruri = userURI;

  server_->addUser(
    conferenceID, 
    user,
    boost::bind(&BfcpService::handleCallResult, this, _1));

  {
    muduo::MutexLockGuard lock(mutex_);
    while (!callFinished_)
    {
      cond_.wait();
    }
  }
  *errorCode = details::convertTo(error_);

  return SOAP_OK;
}

int BfcpService::removeUser( unsigned int conferenceID, unsigned short userID, enum ns__ErrorCode *errorCode )
{
  LOG_INFO << "removeUser with {conferenceID: " << conferenceID
           << ", userID: " << userID << "}";
  if (!server_)
  {
    LOG_WARN << "Server not start";
    *errorCode = ns__ErrorCode::kServerNotStart;
    return SOAP_OK;
  }

  callFinished_ = false;

  server_->removeUser(
    conferenceID, 
    userID,
    boost::bind(&BfcpService::handleCallResult, this, _1));

  {
    muduo::MutexLockGuard lock(mutex_);
    while (!callFinished_)
    {
      cond_.wait();
    }
  }
  *errorCode = details::convertTo(error_);

  return SOAP_OK;
}

int BfcpService::addChair( unsigned int conferenceID, unsigned short floorID, unsigned short userID, enum ns__ErrorCode *errorCode )
{
  LOG_INFO << "addChair with {conferenceID: " << conferenceID
           << ", floorID: " << floorID
           << ", userID: " << userID << "}";

  if (!server_)
  {
    LOG_WARN << "Server not start";
    *errorCode = ns__ErrorCode::kServerNotStart;
    return SOAP_OK;
  }

  callFinished_ = false;

  server_->addChair(
    conferenceID, 
    floorID,
    userID,
    boost::bind(&BfcpService::handleCallResult, this, _1));

  {
    muduo::MutexLockGuard lock(mutex_);
    while (!callFinished_)
    {
      cond_.wait();
    }
  }
  *errorCode = details::convertTo(error_);
  return SOAP_OK;
}

int BfcpService::removeChair( unsigned int conferenceID, unsigned short floorID, enum ns__ErrorCode *errorCode )
{
  LOG_INFO << "addChair with {conferenceID: " << conferenceID
           << ", floorID: " << floorID << "}";

  if (!server_)
  {
    LOG_WARN << "Server not start";
    *errorCode = ns__ErrorCode::kServerNotStart;
    return SOAP_OK;
  }

  callFinished_ = false;

  server_->removeChair(
    conferenceID, 
    floorID,
    boost::bind(&BfcpService::handleCallResult, this, _1));

  {
    muduo::MutexLockGuard lock(mutex_);
    while (!callFinished_)
    {
      cond_.wait();
    }
  }
  *errorCode = details::convertTo(error_);

  return SOAP_OK;
}

int BfcpService::getConferenceIDs( ns__ConferenceListResult *result )
{
  LOG_INFO << "getConferenceIDs";
  if (!server_)
  {
    LOG_WARN << "Server not start";
    result->errorCode = ns__ErrorCode::kServerNotStart;
    return SOAP_OK;
  }

  callFinished_ = false;

  server_->getConferenceIDs(boost::bind(
    &BfcpService::handleGetCoferenceIDsResult, 
    this, &result->conferenceIDs, _1, _2));

  {
    muduo::MutexLockGuard lock(mutex_);
    while (!callFinished_)
    {
      cond_.wait();
    }
  }
  result->errorCode = details::convertTo(error_);

  return SOAP_OK;
}

void BfcpService::handleGetCoferenceIDsResult(ConferenceIDList *ids, 
                                              bfcp::ControlError error, 
                                              void *data)
{
  muduo::MutexLockGuard lock(mutex_);
  error_ = error;
  callFinished_ = true;
  if (data)
  {
    bfcp::BaseServer::ConferenceIDList *res = 
      static_cast<bfcp::BaseServer::ConferenceIDList*>(data);
    ids->swap(*res);
  }
  cond_.notify();
}


int BfcpService::getConferenceInfo( unsigned int conferenceID, ns__ConferenceInfoResult *result )
{
  LOG_INFO << "getConferenceInfo with {conferenceID: " << conferenceID  << "}";
  if (!server_)
  {
    LOG_WARN << "Server not start";
    result->errorCode = ns__ErrorCode::kServerNotStart;
    return SOAP_OK;
  }

  callFinished_ = false;

  server_->getConferenceInfo(
    conferenceID, 
    boost::bind(&BfcpService::handleGetConferenceInfoResult, 
    this, result->conferenceInfo, _1, _2));

  {
    muduo::MutexLockGuard lock(mutex_);
    while (!callFinished_)
    {
      cond_.wait();
    }
  }
  result->errorCode = details::convertTo(error_);
  return SOAP_OK;
}

void BfcpService::handleGetConferenceInfoResult(std::string &info, 
                                                bfcp::ControlError error, 
                                                void *data)
{
  muduo::MutexLockGuard lock(mutex_);
  error_ = error;
  callFinished_ = true;
  if (data)
  {
    bfcp::string *res = static_cast<bfcp::string*>(data);
    info.swap(*res);
  }
  cond_.notify();
}
