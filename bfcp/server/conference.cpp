#include <bfcp/server/conference.h>

#include <boost/bind.hpp>
#include <muduo/base/Logging.h>
#include <tinyxml2.h>

#include <bfcp/common/bfcp_conn.h>
#include <bfcp/server/floor.h>
#include <bfcp/server/user.h>
#include <bfcp/server/floor_request_node.h>

namespace bfcp
{

namespace detail
{

class FloorRequestCmp
{
public:
  FloorRequestCmp(uint16_t floorRequestID)
      : floorRequestID_(floorRequestID)
  {}

  bool operator()(const FloorRequestNodePtr &floorRequest)
  {
    return floorRequest->getFloorRequestID() == floorRequestID_;
  }

private:
  uint16_t floorRequestID_;
};

const bfcp_prim SUPPORTED_PRIMS[] = 
{
  BFCP_FLOOR_REQUEST,
  BFCP_FLOOR_RELEASE,
  BFCP_FLOOR_REQUEST_QUERY,
  BFCP_FLOOR_REQUEST_STATUS,
  BFCP_USER_QUERY,
  BFCP_USER_STATUS,
  BFCP_FLOOR_QUERY,
  BFCP_FLOOR_STATUS ,
  BFCP_CHAIR_ACTION,
  BFCP_CHAIR_ACTION_ACK,
  BFCP_HELLO,
  BFCP_HELLO_ACK,
  BFCP_ERROR,
  BFCP_FLOOR_REQ_STATUS_ACK,
  BFCP_FLOOR_STATUS_ACK,
  BFCP_GOODBYE,
  BFCP_GOODBYE_ACK,
};

const bfcp_attrib SUPPORTED_ATTRS[] =
{
  BFCP_BENEFICIARY_ID,
  BFCP_FLOOR_ID,
  BFCP_FLOOR_REQUEST_ID,
  BFCP_PRIORITY,
  BFCP_REQUEST_STATUS,
  BFCP_ERROR_CODE,
  BFCP_ERROR_INFO,
  BFCP_PART_PROV_INFO,
  BFCP_STATUS_INFO,
  BFCP_SUPPORTED_ATTRS,
  BFCP_SUPPORTED_PRIMS,
  BFCP_USER_DISP_NAME,
  BFCP_USER_URI,
  /* grouped: */
  BFCP_BENEFICIARY_INFO,
  BFCP_FLOOR_REQ_INFO,
  BFCP_REQUESTED_BY_INFO,
  BFCP_FLOOR_REQ_STATUS,
  BFCP_OVERALL_REQ_STATUS,
};

} // namespace detail

const char * toString( AcceptPolicy policy )
{
  switch (policy)
  {
  case AcceptPolicy::kAutoAccept:
    return "AutoAccept";
  case AcceptPolicy::kAutoDeny:
    return "AutoDeny";
  default:
    return "???";
  }
}

Conference::Conference(muduo::net::EventLoop *loop, 
                       const BfcpConnectionPtr &connection, 
                       uint32_t conferenceID, 
                       const ConferenceConfig &config)
    : loop_(CHECK_NOTNULL(loop)),
      connection_(connection),
      conferenceID_(conferenceID),
      nextFloorRequestID_(0),
      maxFloorRequest_(config.maxFloorRequest),
      timeForChairAction_(config.timeForChairAction),
      acceptPolicy_(config.acceptPolicy),
      userObsoletedTime_(config.userObsoletedTime)
{
  LOG_TRACE << "Conference::Conference [" << conferenceID << "] constructing";
  assert(connection_);
  initRequestHandlers();
}

void Conference::initRequestHandlers()
{
  requestHandler_.insert(std::make_pair(
    BFCP_FLOOR_REQUEST,
    boost::bind(&Conference::handleFloorRequest, this, _1)));

  requestHandler_.insert(std::make_pair(
    BFCP_FLOOR_RELEASE,
    boost::bind(&Conference::handleFloorRelease, this, _1)));

  requestHandler_.insert(std::make_pair(
    BFCP_FLOOR_REQUEST_QUERY,
    boost::bind(&Conference::handleFloorRequestQuery, this, _1)));

  requestHandler_.insert(std::make_pair(
    BFCP_USER_QUERY,
    boost::bind(&Conference::handleUserQuery, this, _1)));

  requestHandler_.insert(std::make_pair(
    BFCP_FLOOR_QUERY,
    boost::bind(&Conference::handleFloorQuery, this, _1)));

  requestHandler_.insert(std::make_pair(
    BFCP_CHAIR_ACTION,
    boost::bind(&Conference::handleChairAction, this, _1)));

  requestHandler_.insert(std::make_pair(
    BFCP_HELLO,
    boost::bind(&Conference::handleHello, this, _1)));

  requestHandler_.insert(std::make_pair(
    BFCP_GOODBYE,
    boost::bind(&Conference::handleGoodbye, this, _1)));
}

Conference::~Conference()
{
  LOG_TRACE << "Conference::~Conference [" << conferenceID_ << "] destructing";
}

bfcp::ControlError Conference::set(const ConferenceConfig &config)
{
  LOG_TRACE << "{conferencID:" << conferenceID_
            << ", config: {maxFloorRequest: " << config.maxFloorRequest
            << ", acceptPolicy: " << toString(config.acceptPolicy)
            << ", timeForChairAction: " << config.timeForChairAction << "}}";
  maxFloorRequest_ = config.maxFloorRequest;
  acceptPolicy_ = config.acceptPolicy;
  timeForChairAction_ = config.timeForChairAction;
  return ControlError::kNoError;
}

ControlError Conference::setMaxFloorRequest( uint16_t maxFloorRequest )
{
  LOG_TRACE << "Set max floor request " << maxFloorRequest 
            << " in Conference " << conferenceID_;
  maxFloorRequest_ = maxFloorRequest;
  return ControlError::kNoError;
}

ControlError Conference::modifyFloor(
  uint16_t floorID, const FloorConfig &config)
{
  LOG_TRACE << "{conferencID:" << conferenceID_ 
            << ", floorID:" << floorID
            << ", config:{maxGrantedNum:" << config.maxGrantedNum 
            << ", maxHoldingTime:" << config.maxHoldingTime << "}}";
  auto floor = findFloor(floorID);
  if (!floor)
  {
    LOG_ERROR << "Floor " << floorID 
              << " not exist in Conference " << conferenceID_;
    return ControlError::kFloorNotExist;
  }
  floor->setMaxGrantedCount(config.maxGrantedNum);
  floor->setMaxHoldingTime(config.maxHoldingTime);
  return ControlError::kNoError;
}

ControlError Conference::setAcceptPolicy(
  AcceptPolicy policy, double timeForChairAction)
{
  LOG_TRACE << "Set accept policy " << toString(policy)
            << " and chair action time to " << timeForChairAction 
            << "s in Conference " << conferenceID_;
  timeForChairAction_ = timeForChairAction;
  acceptPolicy_ = policy;
  return ControlError::kNoError;
}

ControlError Conference::addUser(const UserInfoParam &user)
{
  LOG_TRACE << "Add User " << user.id << " to Conference " << conferenceID_;
  ControlError err = ControlError::kNoError;
  auto lb = users_.lower_bound(user.id);
  if (lb != users_.end() && !users_.key_comp()(user.id, (*lb).first))
  {
    LOG_ERROR << "User " << user.id 
              << " already exist in Conference " << conferenceID_;
    err = ControlError::kUserAlreadyExist;
  }
  else
  {
    auto res = users_.emplace_hint(
      lb,
      user.id, 
      boost::make_shared<User>(user.id, user.username, user.useruri));
    // FIXME: check if insert success
    (void)(res);
  }
  return err;
}

ControlError Conference::removeUser( uint16_t userID )
{
  LOG_TRACE << "Remove User " << userID 
            << " from Conference " << conferenceID_;
  auto user = findUser(userID);
  if (!user)
  {
    LOG_ERROR << "User " << userID 
              << " not exist in Conference " << conferenceID_;
    return ControlError::kUserNotExist;
  }

  // remove floor query
  for (auto floor : floors_)
  {
    floor.second->removeQueryUser(userID);
  }

  // checks if the user is chair of any floors
  for (auto floor : floors_)
  {
    if (floor.second->isAssigned() && floor.second->getChairID() == userID)
    {
      removeChair(floor.first);
    }
  }

  cancelFloorRequestsFromPendingByUserID(userID);
  cancelFloorRequestsFromAcceptedByUserID(userID);
  releaseFloorRequestsFromGrantedByUserID(userID);

  users_.erase(userID);

  tryToGrantFloorRequestsWithAllFloors();

  return ControlError::kNoError;
}


void Conference::cancelFloorRequestsFromPendingByUserID(uint16_t userID)
{
  for (auto it = pending_.begin(); it != pending_.end();)
  {
    auto floorRequest = *it;
    if (floorRequest->getUserID() == userID ||
        (floorRequest->hasBeneficiary() && 
         floorRequest->getBeneficiaryID() == userID))
    {
      LOG_INFO << "Cancel FloorRequest " << floorRequest->getFloorRequestID()
               << " from Pending Queue in Conference " << conferenceID_;
      it = pending_.erase(it);
      loop_->cancel(floorRequest->getExpiredTimer()); // cancel the chair action timer
      floorRequest->setOverallStatus(BFCP_CANCELLED);
      floorRequest->removeQueryUser(userID);
      revokeFloorsFromFloorRequest(floorRequest);
      notifyFloorAndRequestInfo(floorRequest);
      continue;
    }
    ++it;
  }
}


void Conference::cancelFloorRequestsFromAcceptedByUserID( uint16_t userID )
{
  for (auto it = accepted_.begin(); it != accepted_.end();)
  {
    auto floorRequest = *it;
    if (floorRequest->getUserID() == userID ||
        (floorRequest->hasBeneficiary() && 
         floorRequest->getBeneficiaryID() == userID))
    {
      LOG_INFO << "Cancel FloorRequest " << floorRequest->getFloorRequestID()
               << " from Accepted Queue in Conference " << conferenceID_;
      it = accepted_.erase(it);
      floorRequest->setOverallStatus(BFCP_CANCELLED);
      floorRequest->removeQueryUser(userID);
      floorRequest->setQueuePosition(0);
      revokeFloorsFromFloorRequest(floorRequest);
      updateQueuePosition(accepted_);
      notifyFloorAndRequestInfo(floorRequest);
      continue;
    }
    ++it;
  }
}

void Conference::releaseFloorRequestsFromGrantedByUserID( uint16_t userID )
{
  for (auto it = granted_.begin(); it != granted_.end();)
  {
    auto floorRequest = *it;
    if (floorRequest->getUserID() == userID ||
        (floorRequest->hasBeneficiary() && 
         floorRequest->getBeneficiaryID() == userID))
    {
      LOG_INFO << "Release FloorRequest " << floorRequest->getFloorRequestID()
               << " from Granted Queue in Conference " << conferenceID_;
      it = granted_.erase(it);
      loop_->cancel(floorRequest->getExpiredTimer()); // cancel the holding timer
      floorRequest->setOverallStatus(BFCP_RELEASED);
      floorRequest->removeQueryUser(userID);
      revokeFloorsFromFloorRequest(floorRequest);
      notifyFloorAndRequestInfo(floorRequest);
      continue;
    }
    ++it;
  }
}

ControlError Conference::addFloor(uint16_t floorID, const FloorConfig &config)
{
  LOG_TRACE << "Add Floor " << floorID << " to Conference " << conferenceID_;
  ControlError err = ControlError::kNoError;
  auto lb = floors_.lower_bound(floorID);
  if (lb != floors_.end() && !floors_.key_comp()(floorID, (*lb).first))
  { // This floor already exists
    LOG_ERROR << "Floor " << floorID 
              << " already exist in Conference " << conferenceID_;
    err = ControlError::kFloorAlreadyExist;
  } 
  else
  {
    auto res = floors_.emplace_hint(
      lb, 
      floorID, 
      boost::make_shared<Floor>(floorID, config.maxGrantedNum, config.maxHoldingTime));
    // FIXME: check if insert success
    (void)(res);
  }
  return err;
}

ControlError Conference::removeFloor( uint16_t floorID )
{
  LOG_TRACE << "Remove Floor " << floorID 
            << " from Conference " << conferenceID_;
  auto floor = findFloor(floorID);
  if (!floor)
  {
    LOG_ERROR << "Floor " << floorID
              << " not exist in Conference " << conferenceID_;
    return ControlError::kFloorNotExist;
  }
  cancelFloorRequestsFromPendingByFloorID(floorID);
  cancelFloorRequestsFromAcceptedByFloorID(floorID);
  releaseFloorRequestsFromGrantedByFloorID(floorID);
  
  floors_.erase(floorID);

  tryToGrantFloorRequestsWithAllFloors();
  return ControlError::kNoError;
}

void Conference::cancelFloorRequestsFromPendingByFloorID(uint16_t floorID)
{
  for (auto it = pending_.begin(); it != pending_.end();)
  {
    if ((*it)->findFloor(floorID))
    {
      LOG_INFO << "Cancel FloorRequest " << (*it)->getFloorRequestID()
               << " from Pending Queue in Conference " << conferenceID_;
      auto floorRequest = *it;
      loop_->cancel(floorRequest->getExpiredTimer()); // cancel the chair action timer
      floorRequest->setOverallStatus(BFCP_CANCELLED);
      it = pending_.erase(it);
      revokeFloorsFromFloorRequest(floorRequest);
      notifyFloorAndRequestInfo(floorRequest);
      continue;
    }
    ++it;
  }
}

void Conference::cancelFloorRequestsFromAcceptedByFloorID(uint16_t floorID)
{
  for (auto it = accepted_.begin(); it != accepted_.end();)
  {
    if ((*it)->findFloor(floorID))
    {
      LOG_INFO << "Cancel FloorRequest " << (*it)->getFloorRequestID()
               << " from Accepted Queue in Conference " << conferenceID_;
      auto floorRequest = *it;
      floorRequest->setOverallStatus(BFCP_CANCELLED);
      it = accepted_.erase(it);
      revokeFloorsFromFloorRequest(floorRequest);
      updateQueuePosition(accepted_);
      notifyFloorAndRequestInfo(floorRequest);
      continue;
    }
    ++it;
  }
}

void Conference::releaseFloorRequestsFromGrantedByFloorID(uint16_t floorID)
{
  for (auto it = granted_.begin(); it != granted_.end();)
  {
    if ((*it)->findFloor(floorID))
    {
      LOG_INFO << "Release FloorRequest " << (*it)->getFloorRequestID()
               << " from Granted Queue in Conference " << conferenceID_;
      auto floorRequest = *it;
      floorRequest->setOverallStatus(BFCP_RELEASED);
      it = granted_.erase(it);
      revokeFloorsFromFloorRequest(floorRequest);
      notifyFloorAndRequestInfo(floorRequest);
      continue;
    }
    ++it;
  }
}

ControlError Conference::setChair( uint16_t floorID, uint16_t userID )
{
  LOG_TRACE << "Set Chair " << userID << " of Floor " << floorID
            << " in Conference " << conferenceID_;
  ControlError err = ControlError::kNoError;
  do 
  {
    auto user = findUser(userID);
    if (!user)
    {
      LOG_ERROR << "User " << userID 
                << " not exist in Conference " << conferenceID_;
      err = ControlError::kUserNotExist;
      break;
    }
    auto floor = findFloor(floorID);
    if (!floor)
    {
      LOG_ERROR << "Floor " << floorID 
                << " not exist in Conference " << conferenceID_;
      err = ControlError::kFloorNotExist;
      break;
    }
    floor->unassigned();
    floor->assignedToChair(userID);
  } while (false);
  return err;
}

UserPtr Conference::findUser( uint16_t userID )
{
  auto it = users_.find(userID);
  return it != users_.end() ? (*it).second : nullptr;
}

bfcp::FloorPtr Conference::findFloor( uint16_t floorID )
{
  auto it = floors_.find(floorID);
  return it != floors_.end() ? (*it).second : nullptr;
}

ControlError Conference::removeChair( uint16_t floorID )
{
  LOG_TRACE << "Remove Chair of Floor " << floorID 
            << " in Conference " << conferenceID_;
  ControlError err = ControlError::kNoError;
  do 
  {
    auto floor = findFloor(floorID);
    if (!floor)
    {
      LOG_ERROR << "Floor " << floorID 
                << " not exist in Conference " << conferenceID_;
      err = ControlError::kFloorNotExist;
      break;
    }

    if (!floor->isAssigned())
    {
      err = ControlError::kChairNotExist;
      break;
    }

    assert(findUser(floor->getChairID()));
    if (acceptPolicy_ == AcceptPolicy::kAutoDeny)
    {
      cancelFloorRequestsFromPendingByFloorID(floorID);
    }
    else if (acceptPolicy_ == AcceptPolicy::kAutoAccept)
    {
      if (tryToAcceptFloorRequestsWithFloor(floor))
      {
        tryToGrantFloorRequestsWithFloor(floor);
      }
    }
    floor->unassigned();

  } while (false);
  
  return err;
}


bool Conference::tryToAcceptFloorRequestsWithFloor(FloorPtr &floor)
{
  if (!floor) return false;
  uint16_t floorID = floor->getFloorID();
  bool hasAcceptFloor = false;
  for (auto it = pending_.begin(); it != pending_.end();)
  {
    auto floorRequest = *it;
    auto floorNode = floorRequest->findFloor(floorID);
    if (floorNode && floorNode->getStatus() != BFCP_ACCEPTED)
    {
      floorNode->setStatus(BFCP_ACCEPTED);
      floorNode->setStatusInfo(nullptr);
      hasAcceptFloor = true;
      if (!floorRequest->isAllFloorStatus(BFCP_ACCEPTED))
      {
        notifyWithFloorRequestStatus(floorRequest);
      }
      else // all floor status in the floor request is accepted
      {
        // remove the floor request from pending queue
        it = pending_.erase(it);
        loop_->cancel(floorRequest->getExpiredTimer()); // cancel the chair action timer
        insertFloorRequestToAcceptedQueue(floorRequest);
        continue;
      }
    }
    ++it;
  }
  return hasAcceptFloor;
}

void Conference::insertFloorRequestToPendingQueue( 
  FloorRequestNodePtr &floorRequest)
{
  LOG_INFO << "Insert FloorRequest " << floorRequest->getFloorRequestID()
           << " to Pending Queue in Conference " << conferenceID_;
  assert(floorRequest->isAnyFloorStatus(BFCP_PENDING));
  floorRequest->setOverallStatus(BFCP_PENDING);
  floorRequest->setQueuePosition(0);
  insertFloorRequestToQueue(pending_, floorRequest);
  notifyFloorAndRequestInfo(floorRequest);
}

void Conference::insertFloorRequestToAcceptedQueue( 
  FloorRequestNodePtr &floorRequest)
{
  LOG_INFO << "Insert FloorRequest " << floorRequest->getFloorRequestID()
           << "to Accepted Queue in Conference " << conferenceID_;
  assert(floorRequest->isAllFloorStatus(BFCP_ACCEPTED));
  floorRequest->setOverallStatus(BFCP_ACCEPTED);
  floorRequest->setPrioriy(BFCP_PRIO_LOWEST);
  insertFloorRequestToQueue(accepted_, floorRequest);
  updateQueuePosition(accepted_);
  notifyFloorAndRequestInfo(floorRequest);
}

void Conference::insertFloorRequestToGrantedQueue(
  FloorRequestNodePtr &floorRequest)
{
  LOG_INFO << "Insert FloorRequest " << floorRequest->getFloorRequestID()
           << " to Granted Queue in Conference " << conferenceID_;
  assert(floorRequest->isAllFloorStatus(BFCP_GRANTED));
  floorRequest->setOverallStatus(BFCP_GRANTED);
  floorRequest->setQueuePosition(0);
  floorRequest->setPrioriy(BFCP_PRIO_LOWEST);
  insertFloorRequestToQueue(granted_, floorRequest);
  notifyFloorAndRequestInfo(floorRequest);

  // set holding timer
  double minHoldingTime = -1.0;
  for (auto &floorNode : floorRequest->getFloorNodeList())
  {
    auto floor = findFloor(floorNode.getFloorID());
    assert(floor);
    if (floor->getMaxHoldingTime() > 0.0) 
    {
      double holdingTime = floor->getMaxHoldingTime();
      minHoldingTime = minHoldingTime < 0.0 ? 
        holdingTime : (std::min)(minHoldingTime, holdingTime); 
    }
  }
  if (minHoldingTime > 0.0)
  {
    setFloorRequestExpired(
      floorRequest, minHoldingTime, holdingTimeoutCallback_);
  }
}

void Conference::insertFloorRequestToQueue(
  FloorRequestQueue &queue, FloorRequestNodePtr &floorRequest)
{
  // NOTE: The queue stores floor requests from low priority to high priority
  // If queue position is not 0, we set the one the chair stated
  auto it = queue.begin();
  if (floorRequest->getQueuePosition() != 0 && it != queue.end())
  {
    int pos = 1;
    auto rit = queue.rbegin();
    while (pos < floorRequest->getQueuePosition() && 
           rit != queue.rend())
    {
      ++pos;
      ++rit;
    }
    if (pos == floorRequest->getQueuePosition())
    {
      it = rit.base();
    }
    else
    {
      it = queue.begin();
    }
    floorRequest->setPrioriy(BFCP_PRIO_LOWEST);
  }
  floorRequest->setQueuePosition(0);

  while (it != queue.end() && (*it)->getPriority() < floorRequest->getPriority()) {
    ++it;
  }
  queue.insert(it, floorRequest);
}

void Conference::updateQueuePosition( FloorRequestQueue &queue )
{
  // NOTE: queue position starts from 1
  uint8_t qpos = 1;
  for (auto it = queue.rbegin(); it != queue.rend(); ++it)
  {
    (*it)->setQueuePosition(qpos++);
  }
}

void Conference::onTimeoutForHoldingFloors( uint16_t floorRequestID )
{
  LOG_TRACE << "FloorRequest " << floorRequestID 
            << " is expired in Conference " << conferenceID_;
  auto floorRequest = findFloorRequest(granted_, floorRequestID);
  if (!floorRequest) return;

  LOG_INFO << "Revoke FloorRequest " << floorRequestID
           << " from Granted Queue in Conference " << conferenceID_;
  granted_.remove(floorRequest);
  revokeFloorsFromFloorRequest(floorRequest);

  for (auto &floorNode : floorRequest->getFloorNodeList())
  {
    floorNode.setStatus(BFCP_REVOKED);
  }

  floorRequest->setStatusInfo("Timeout for holding floor(s).");
  floorRequest->setOverallStatus(BFCP_REVOKED);
  notifyFloorAndRequestInfo(floorRequest);
  tryToGrantFloorRequestsWithAllFloors();
}

bool Conference::tryToGrantFloorRequestsWithAllFloors()
{
  bool hasGrantFloor = false;
  for (auto &floor : floors_)
  {
    if (tryToGrantFloorRequestsWithFloor(floor.second))
    {
      hasGrantFloor = true;
    }
  }
  return hasGrantFloor;
}

bool Conference::tryToGrantFloorRequestsWithFloor( FloorPtr &floor )
{
  if (!floor) return false;

  bool hasGrantFloor = false;
  // NOTE: floor request store floor requests from low priority to high priority
  for (auto it = accepted_.rbegin(); it != accepted_.rend();)
  {
    if (!floor->isFreeToGrant()) break;
    if (tryToGrantFloor(*it, floor))
    {
      hasGrantFloor = true;
      if ((*it)->isAllFloorStatus(BFCP_GRANTED))
      {
        auto floorRequest = *it;
        // remove the floor request from accepted queue
        it = FloorRequestQueue::reverse_iterator(
          accepted_.erase(std::next(it).base()));

        // move the floor request to granted queue
        insertFloorRequestToGrantedQueue(floorRequest);
        continue;
      }
    }
    ++it;
  }
  return hasGrantFloor;
}

bool Conference::tryToGrantFloor(FloorRequestNodePtr &floorRequest,
                                 FloorPtr &floor)
{
  if (floor && floor->isFreeToGrant())
  {
    uint16_t floorID = floor->getFloorID();
    auto floorNode = floorRequest->findFloor(floorID);
    if (floorNode && floorNode->getStatus() != BFCP_GRANTED)
    {
      bool res = floor->tryToGrant();
      (void)(res);
      assert(res);
      floorNode->setStatus(BFCP_GRANTED);
      floorNode->setStatusInfo(nullptr);
      return true;
    }
  }
  
  return false;
}

void Conference::notifyFloorAndRequestInfo(
  const FloorRequestNodePtr &floorRequest)
{
  for (auto &floorNode : floorRequest->getFloorNodeList())
  {
    notifyWithFloorStatus(floorNode.getFloorID());
  }

  notifyWithFloorRequestStatus(floorRequest);
}

void Conference::notifyWithFloorStatus( uint16_t floorID )
{
  auto floor = findFloor(floorID);
  if (!floor) return;
  BasicRequestParam basicParam;
  basicParam.conferenceID = conferenceID_;
  auto floorStatusParam = getFloorStatusParam(floorID);
  // make the chair of the floor to be notified
  if (floor->isAssigned())
  {
    floor->addQueryUser(floor->getChairID());
  }
  // notify all query users about the floor status
  for (auto userID : floor->getQueryUsers())
  {
    auto user = findUser(userID);
    if (user && isUserAvailable(user))
    {
      basicParam.userID = userID;
      basicParam.dst = user->getAddr();
      assert(clientReponseCallback_);
      basicParam.cb = boost::bind(clientReponseCallback_,
        conferenceID_, BFCP_FLOOR_STATUS_ACK, userID, _1, _2);
      user->runSendMessageTask(
        boost::bind(&BfcpConnection::notifyFloorStatus,
        connection_, basicParam, floorStatusParam));
    }
  }
}

void Conference::notifyWithFloorRequestStatus(
  const FloorRequestNodePtr &floorRequest)
{
  const auto &queryUsers = floorRequest->getFloorRequestQueryUsers();
  BasicRequestParam param;
  param.conferenceID = conferenceID_;
  auto frqInfo = floorRequest->toFloorRequestInfoParam(users_);
  for (auto userID : queryUsers)
  {
    auto user = findUser(userID);
    if (user && isUserAvailable(user))
    {
      param.userID = userID;
      param.dst = user->getAddr();
      assert(clientReponseCallback_);
      param.cb = boost::bind(clientReponseCallback_, 
        conferenceID_, BFCP_FLOOR_REQ_STATUS_ACK, userID, _1, _2);
      user->runSendMessageTask(
        boost::bind(&BfcpConnection::notifyFloorRequestStatus, 
        connection_, param, frqInfo));
    }
  }
}

void Conference::onNewRequest( const BfcpMsgPtr &msg )
{
  LOG_TRACE << "Conference received new request " << msg->toString();
  assert(msg->valid());
  assert(!msg->isResponse());
  assert(msg->getConferenceID() == conferenceID_);

  if (checkUnknownAttrs(msg) && checkUserID(msg, msg->getUserID()))
  {
    auto user = findUser(msg->getUserID());
    assert(user);
    if (!isUserAvailable(user))
    {
      user->setAvailable(true);
      user->setAddr(msg->getSrc());
    }
    user->setActiveTime(msg->getReceivedTime());

    bfcp_prim prim = msg->primitive();
    auto it = requestHandler_.find(prim);
    if (it != requestHandler_.end())
    {
      (*it).second(msg);
    }
    else
    {
      char errorInfo[128];
      snprintf(errorInfo, sizeof errorInfo,
        "Unknown primitive %s(%i) as request", bfcp_prim_name(prim), prim);
      replyWithError(msg, BFCP_UNKNOWN_PRIM, errorInfo);
    }
  }
}

bool Conference::isUserAvailable( const UserPtr &user ) const
{
  if (!user->isAvailable()) return false;
  if (userObsoletedTime_ > 0.0) 
  {
    muduo::Timestamp now = muduo::Timestamp::now();
    double livingTime = muduo::timeDifference(now, user->getActiveTime());
    return livingTime < userObsoletedTime_;
  }
  return true;
}

bool Conference::checkUnknownAttrs( const BfcpMsgPtr &msg )
{
  auto &unknownAttrs = msg->getUnknownAttrs();
  if (unknownAttrs.typec > 0)
  {
    bfcp_errcode_t errcode;
    errcode.code = BFCP_UNKNOWN_MAND_ATTR;
    errcode.details = const_cast<uint8_t*>(unknownAttrs.typev);
    errcode.len = unknownAttrs.typec;

    ErrorParam param;
    param.setErrorCode(errcode);
    connection_->replyWithError(msg, param);
    return false;
  }
  return true;
}

bool Conference::checkUserID( const BfcpMsgPtr &msg, uint16_t userID )
{
  if (!findUser(userID))
  {
    char errorInfo[128];
    snprintf(errorInfo, sizeof errorInfo, 
      "User %hu does not exist in Conference %u",
      userID, msg->getConferenceID());
    replyWithError(msg, BFCP_USER_NOT_EXIST, errorInfo);
    return false;
  }
  return true;
}

void Conference::replyWithError(const BfcpMsgPtr &msg, 
                                bfcp_err err, 
                                const char *errInfo)
{
  ErrorParam param;
  param.errorCode.code = err;
  param.setErrorInfo(errInfo);
  connection_->replyWithError(msg, param);
}

void Conference::handleHello( const BfcpMsgPtr &msg )
{
  assert(msg->primitive() == BFCP_HELLO);

  HelloAckParam param;
  // set supported primitives
  const size_t primCount = 
    sizeof(detail::SUPPORTED_PRIMS) / sizeof(detail::SUPPORTED_PRIMS[0]);
  param.primitives.reserve(primCount);
  param.primitives.assign(
    detail::SUPPORTED_PRIMS, detail::SUPPORTED_PRIMS + primCount);
  // set supported attributes
  const size_t attrCount =
    sizeof(detail::SUPPORTED_ATTRS) / sizeof(detail::SUPPORTED_ATTRS[0]);
  param.attributes.reserve(attrCount);
  param.attributes.assign(
    detail::SUPPORTED_ATTRS, detail::SUPPORTED_ATTRS + attrCount);

  connection_->replyWithHelloAck(msg, param);
}

void Conference::handleGoodbye( const BfcpMsgPtr &msg )
{
  assert(msg->primitive() == BFCP_GOODBYE);
  
  connection_->replyWithGoodbyeAck(msg);

  auto user = findUser(msg->getUserID());
  assert(user);
  user->setAvailable(false);
  user->clearAllSendMessageTasks();
}

void Conference::handleFloorRequest( const BfcpMsgPtr &msg )
{
  assert(msg->primitive() == BFCP_FLOOR_REQUEST);

  FloorRequestParam param;
  if (!parseFloorRequestParam(param, msg)) return;

  auto user = findUser(msg->getUserID());
  assert(user);
  auto beneficiaryUser = user;
  if (param.hasBeneficiaryID)
  {
    if (!checkUserID(msg, param.beneficiaryID)) return;
    beneficiaryUser = findUser(param.beneficiaryID);
    assert(beneficiaryUser);
  }

  if (!checkFloorIDs(msg, param.floorIDs)) return;

  FloorRequestNodePtr newFloorRequest = 
    boost::make_shared<FloorRequestNode>(
      nextFloorRequestID_++, msg->getUserID(), param);

  for (auto &floorNode : newFloorRequest->getFloorNodeList())
  {
    uint16_t floorID = floorNode.getFloorID();
    auto floor = findFloor(floorID);
    assert(floor);
    if (!floor->isAssigned() && acceptPolicy_ == AcceptPolicy::kAutoDeny)
    {
      floorNode.setStatus(BFCP_DENIED);
      newFloorRequest->setOverallStatus(BFCP_DENIED);
      replyWithFloorRequestStatus(msg, newFloorRequest);
      return;
    }

    // NOTE: must check all request counts before adding request of floors
    uint16_t requestCount = beneficiaryUser->getRequestCountOfFloor(floorID);
    if (maxFloorRequest_ <= requestCount)
    {
      char errorInfo[200];
      snprintf(errorInfo, sizeof errorInfo,
        "User %hu has already reached "
        "the maximum allowed number of requests (%hu)" 
        "for the same floor in Conference %u",
        beneficiaryUser->getUserID(), 
        requestCount, 
        conferenceID_);
      replyWithError(msg, BFCP_MAX_FLOOR_REQ_REACHED, errorInfo);
      return;
    }
  }

  // add request of floors
  for (auto &floorNode : newFloorRequest->getFloorNodeList())
  {
    beneficiaryUser->addOneRequestOfFloor(floorNode.getFloorID());
  }

  // insert to the pending queue
  insertFloorRequestToPendingQueue(newFloorRequest);
  replyWithFloorRequestStatus(msg, newFloorRequest);
  newFloorRequest->addQueryUser(msg->getUserID());

  // handle chair part
  bool needChairAction = false;
  for (auto &floorNode : newFloorRequest->getFloorNodeList())
  {
    auto floor = findFloor(floorNode.getFloorID());
    assert(floor);
    if (!floor->isAssigned())
    {
      assert(acceptPolicy_ == AcceptPolicy::kAutoAccept);
      floorNode.setStatus(BFCP_ACCEPTED);
    }
    else
    {
      uint16_t chairID = floor->getChairID();
      assert(findUser(chairID));
      (void)(chairID);
      needChairAction = true;
    }
  }

  if (needChairAction) 
  {
    // start timer for chair action
    if (timeForChairAction_ > 0.0) 
    {
      setFloorRequestExpired(
        newFloorRequest, timeForChairAction_, chairActionTimeoutCallback_);
    }
  }

  // check if the floor request is accepted
  if (newFloorRequest->isAllFloorStatus(BFCP_ACCEPTED))
  {
    // put the floor request to the accepted queue
    pending_.remove(newFloorRequest);
    insertFloorRequestToAcceptedQueue(newFloorRequest);
    tryToGrantFloorRequestWithAllFloors(newFloorRequest);
  }
}

bool Conference::parseFloorRequestParam(FloorRequestParam &param, const BfcpMsgPtr &msg)
{
  param.floorIDs = msg->getFloorIDs();
  if (param.floorIDs.empty())
  {
    replyWithError(msg, BFCP_PARSE_ERROR, 
      "Missing FLOOR-ID attribute");
    return false;
  }

  auto attr = msg->findAttribute(BFCP_BENEFICIARY_ID);
  if (attr)
  {
    BfcpAttr beneficiaryIDAttr(*attr);
    param.hasBeneficiaryID = true;
    param.beneficiaryID = beneficiaryIDAttr.getBeneficiaryID();
  }

  attr = msg->findAttribute(BFCP_PART_PROV_INFO);
  if (attr)
  {
    BfcpAttr partProvInfo(*attr);
    detail::setString(param.pInfo, partProvInfo.getParticipantProvidedInfo());
  }

  attr = msg->findAttribute(BFCP_PRIORITY);
  if (attr)
  {
    BfcpAttr priorityAttr(*attr);
    param.priority = priorityAttr.getPriority();
  }

  return true;
}

bool Conference::isChair( uint16_t userID ) const
{
  for (auto floor : floors_)
  {
    if (floor.second->isAssigned() && 
      floor.second->getChairID() == userID)
    {
      return true;
    }
  }
  return false;
}

bool Conference::checkFloorIDs( const BfcpMsgPtr &msg, const bfcp_floor_id_list &floorIDs )
{
  for (auto floorID : floorIDs)
  {
    if (!checkFloorID(msg, floorID)) 
    {
      return false;
    }
  }
  return true;
}

bool Conference::checkFloorID( const BfcpMsgPtr &msg, uint16_t floorID )
{
  if (!findFloor(floorID))
  {
    char errorInfo[128];
    snprintf(errorInfo, sizeof errorInfo, 
      "Floor %hu does not exist in Conference %u",
      floorID, conferenceID_);
    replyWithError(msg, BFCP_INVALID_FLOOR_ID, errorInfo);
    return false;
  }
  return true;
}

void Conference::replyWithFloorRequestStatus(
  const BfcpMsgPtr &msg, FloorRequestNodePtr &floorRequest)
{
  FloorRequestInfoParam param = 
    floorRequest->toFloorRequestInfoParam(users_);
  connection_->replyWithFloorRequestStatus(msg, param);
}

void Conference::setFloorRequestExpired(FloorRequestNodePtr &floorRequest,
                                        double expiredTime, 
                                        const FloorRequestExpiredCallback &cb)
{
  assert(cb);
  assert(expiredTime > 0.0);
  uint16_t floorRequestID = floorRequest->getFloorRequestID();
  muduo::net::TimerId timerId = loop_->runAfter(
    expiredTime, 
    boost::bind(cb, conferenceID_, floorRequestID));
  floorRequest->setExpiredTimer(timerId);
}

bool Conference::tryToGrantFloorRequestWithAllFloors(
  FloorRequestNodePtr &floorRequest)
{
  assert(!floorRequest->isAllFloorStatus(BFCP_GRANTED));
  bool hasGrantedFloor = false;
  for (auto &floorNode : floorRequest->getFloorNodeList())
  {
    auto floor = findFloor(floorNode.getFloorID());
    if (floorNode.getStatus() != BFCP_GRANTED && floor->isFreeToGrant())
    {
      bool res = floor->tryToGrant();
      (void)(res);
      assert(res);
      floorNode.setStatus(BFCP_GRANTED);
      floorNode.setStatusInfo(nullptr);
      hasGrantedFloor = true;
    }
  }

  if (hasGrantedFloor && floorRequest->isAllFloorStatus(BFCP_GRANTED))
  {
    accepted_.remove(floorRequest);
    updateQueuePosition(accepted_);
    insertFloorRequestToGrantedQueue(floorRequest);
    return true;
  }
  return false;
}

void Conference::onTimeoutForChairAction( uint16_t floorRequestID )
{
  LOG_TRACE << "FloorRequest " << floorRequestID 
            << " is expired in Conference " << conferenceID_;
  auto floorRequest = findFloorRequest(pending_, floorRequestID);
  if (!floorRequest) return;

  for (auto &floorNode : floorRequest->getFloorNodeList())
  {
    if (floorNode.getStatus() == BFCP_PENDING)
      floorNode.setStatus(BFCP_CANCELLED);
  }

  LOG_INFO << "Cancel FloorRequest " << floorRequestID
           << " from Pending Queue in Conference " << conferenceID_;
  pending_.remove(floorRequest);
  revokeFloorsFromFloorRequest(floorRequest);
  floorRequest->setOverallStatus(BFCP_CANCELLED);
  notifyFloorAndRequestInfo(floorRequest);
}

void Conference::handleFloorRelease( const BfcpMsgPtr &msg )
{
  assert(msg->primitive() == BFCP_FLOOR_RELEASE);
  
  auto attr = msg->findAttribute(BFCP_FLOOR_REQUEST_ID);
  if (!attr)
  {
    replyWithError(msg, BFCP_PARSE_ERROR, 
      "Missing FLOOR-REQUEST-ID attribute");
    return;
  }

  uint16_t floorRequestID;
  {
    BfcpAttr floorRequestIDAttr(*attr);
    floorRequestID = floorRequestIDAttr.getFloorRequestID();
  }

  // FIXME: reply with error BFCP_UNAUTH_OPERATION if user ID not matched?
  FloorRequestNodePtr floorRequest = 
    removeFloorRequest(floorRequestID, msg->getUserID());
  if (!floorRequest)
  {
    char errorInfo[128];
    snprintf(errorInfo, sizeof errorInfo, 
      "FloorRequest %hu does not exist in Conference %u",
      floorRequestID, conferenceID_);
    replyWithError(msg, BFCP_FLOOR_REQ_ID_NOT_EXIST, errorInfo);
    return;
  }

  // reply the releaser about the floor request status of the release request
  replyWithFloorRequestStatus(msg, floorRequest);
  floorRequest->removeQueryUser(msg->getUserID());
  // notify the interested users
  notifyWithFloorRequestStatus(floorRequest);

  if (revokeFloorsFromFloorRequest(floorRequest))
  {
    tryToGrantFloorRequestsWithAllFloors();
  }
}

FloorRequestNodePtr Conference::removeFloorRequest(
  uint16_t floorRequestID, uint16_t userID)
{
  // check the pending queue
  auto floorRequest = 
    extractFloorRequestFromQueue(pending_, floorRequestID, userID);
  if (floorRequest)
  {
    LOG_INFO << "Cancel FloorRequest " << floorRequestID 
             << " from Pending Queue in Conference " << conferenceID_;
    floorRequest->setOverallStatus(BFCP_CANCELLED);
    // cancel the chair action timer
    loop_->cancel(floorRequest->getExpiredTimer());
    return floorRequest;
  }

  // check the accepted queue
  floorRequest = 
    extractFloorRequestFromQueue(accepted_, floorRequestID, userID);
  if (floorRequest)
  {
    LOG_INFO << "Cancel FloorRequest " << floorRequestID 
             << " from Accepted Queue in Conference " << conferenceID_;
    floorRequest->setOverallStatus(BFCP_CANCELLED);
    floorRequest->setQueuePosition(0);
    updateQueuePosition(accepted_);
    return floorRequest;
  }

  // check the granted queue
  floorRequest = 
    extractFloorRequestFromQueue(granted_, floorRequestID, userID);
  if (floorRequest)
  {
    LOG_INFO << "Release FloorRequest " << floorRequestID 
             << " from Granted Queue in Conference " << conferenceID_;
    floorRequest->setOverallStatus(BFCP_RELEASED);
    // cancel the holding timer
    loop_->cancel(floorRequest->getExpiredTimer());
    return floorRequest;
  }

  return nullptr;
}

FloorRequestNodePtr Conference::extractFloorRequestFromQueue(
  FloorRequestQueue &queue, uint16_t floorRequestID, uint16_t userID)
{
  auto it = std::find_if(queue.begin(), queue.end(), 
    detail::FloorRequestCmp(floorRequestID));
  if (it != queue.end() && (*it)->getUserID() == userID)
  {
    auto floorRequest = *it;
    queue.erase(it);
    return floorRequest;
  }
  return nullptr;
}

bool Conference::revokeFloorsFromFloorRequest(FloorRequestNodePtr &floorRequest)
{
  UserPtr beneficiaryUser = floorRequest->hasBeneficiary() ? 
    findUser(floorRequest->getBeneficiaryID()) :
    findUser(floorRequest->getUserID());
  assert(beneficiaryUser);

  bool hasRevokeFloor = false;
  for (auto &floorNode : floorRequest->getFloorNodeList())
  {
    auto floor = findFloor(floorNode.getFloorID());
    assert(floor);
    beneficiaryUser->removeOneRequestOfFloor(floor->getFloorID());
    if (floorNode.getStatus() == BFCP_GRANTED)
    {
      floor->revoke();
      hasRevokeFloor = true;
    } 
  }
  return hasRevokeFloor;
}

void Conference::handleChairAction( const BfcpMsgPtr &msg )
{
  assert(msg->primitive() == BFCP_CHAIR_ACTION);

  auto attr = msg->findAttribute(BFCP_FLOOR_REQ_INFO);
  if (!attr)
  {
    replyWithError(msg, BFCP_PARSE_ERROR, 
      "Missing FLOOR-REQUEST-INFORMATION attribute");
    return;
  }

  BfcpAttr floorReqInfoAttr(*attr);
  auto info = floorReqInfoAttr.getFloorRequestInfo();
  if (!checkFloorRequestInfo(msg, info)) return;
  
  assert(info.oRS.requestStatus);
  bfcp_reqstat status = info.oRS.requestStatus->status;
  switch (status)
  {
    case BFCP_ACCEPTED: 
      acceptFloorRequest(msg, info); break;
    case BFCP_DENIED: 
      denyFloorRequest(msg, info); break;
    case BFCP_REVOKED: 
      revokeFloorRequest(msg, info); break;
    default:
      replyWithError(msg, BFCP_PARSE_ERROR,
        "Only accepted, denied and revoked statuses allowed in ChairAction");
      break;
  }
}

bool Conference::checkFloorRequestInfo(
  const BfcpMsgPtr &msg, const bfcp_floor_request_info &info)
{
  if (info.fRS.empty())
  {
    replyWithError(msg, BFCP_PARSE_ERROR, 
      "Missing FLOOR-REQUEST-STATUS attribute");
    return false;
  }

  // FIXME: should have OVERALL-REQUEST-STATUS attribute
  // it's easier to have OVERALL-REQUEST-STATUS to handle chair action
  if (!(info.valueType & kHasOverallRequestStatus))
  {
    replyWithError(msg, BFCP_PARSE_ERROR, 
      "Missing OVERALL-REQUEST-STATUS attribute");
    return false;
  }

  if (info.oRS.floorRequestID != info.floorRequestID)
  {
    replyWithError(msg, BFCP_PARSE_ERROR, 
      "Floor request ID in OVERALL-REQUEST-STATUS is different from "
      "the one in FLOOR-REQUEST-INFORMATION");
    return false;
  }

  if (!info.oRS.requestStatus)
  {
    replyWithError(msg, BFCP_PARSE_ERROR,
      "Missing REQUEST-STATUS in OVERALL-REQUEST-STATUS attribute");
    return false;
  }

  // check if all the floors exist in the conference
  // check if the user is allowed to do this operation
  for (auto &floorReqStatus : info.fRS)
  {
    if (!(checkFloorID(msg, floorReqStatus.floorID) && 
          checkFloorChair(msg, floorReqStatus.floorID, msg->getUserID())))
    {
      return false;
    }
  }
  return true;
}

bool Conference::checkFloorChair(
  const BfcpMsgPtr &msg, uint16_t floorID, uint16_t userID)
{
  auto floor = findFloor(floorID);
  assert(floor);
  if (!floor->isAssigned() || floor->getChairID() != userID)
  {
    char errorInfo[128];
    snprintf(errorInfo, sizeof errorInfo,
      "User %hu is not chair of Floor %hu in Conference %u",
      userID, floorID, conferenceID_);
    replyWithError(msg, BFCP_UNAUTH_OPERATION, errorInfo);
    return false;
  }
  return true;
}

void Conference::acceptFloorRequest(
  const BfcpMsgPtr &msg, const bfcp_floor_request_info &info)
{
  assert(info.oRS.requestStatus);
  assert(info.oRS.requestStatus->status == BFCP_ACCEPTED);
  auto floorRequest = checkFloorRequestInPendingQueue(msg, info.floorRequestID);
  if (!floorRequest) return;

  // FIXME: check if all floors in chair action are requested by the floor request
  if (!checkFloorsInFloorRequest(msg, floorRequest, info.fRS))
    return;

  // reply chair action ack
  connection_->replyWithChairActionAck(msg);
  
  for (auto &floorReqStatus : info.fRS)
  {
    FloorNode *floorNode = floorRequest->findFloor(floorReqStatus.floorID);
    assert(floorNode);
    // FIXME: currently ignore the REQUEST-STATUS in FLOOR-REQUEST-STATUS
    floorNode->setStatus(BFCP_ACCEPTED);
    floorNode->setStatusInfo(floorReqStatus.statusInfo);
  }

  assert(info.oRS.requestStatus);
  floorRequest->setQueuePosition(info.oRS.requestStatus->qpos);
  if (floorRequest->isAllFloorStatus(BFCP_ACCEPTED))
  {
    pending_.remove(floorRequest);
    // cancel the chair action timer
    loop_->cancel(floorRequest->getExpiredTimer());
    insertFloorRequestToAcceptedQueue(floorRequest);
    tryToGrantFloorRequestWithAllFloors(floorRequest);
  }
}

void Conference::denyFloorRequest(
  const BfcpMsgPtr &msg, const bfcp_floor_request_info &info)
{
  assert(info.oRS.requestStatus);
  assert(info.oRS.requestStatus->status == BFCP_DENIED);
  auto floorRequest = checkFloorRequestInPendingQueue(msg, info.floorRequestID);
  if (!floorRequest) return;

  // FIXME: check if all floors in chair action are requested by the floor request
  if (!checkFloorsInFloorRequest(msg, floorRequest, info.fRS))
    return;
  
  // reply chair action ack
  connection_->replyWithChairActionAck(msg);

  // cancel the chair action timer
  loop_->cancel(floorRequest->getExpiredTimer());
  pending_.remove(floorRequest);
  revokeFloorsFromFloorRequest(floorRequest);
  
  for (auto &floorReqStatus : info.fRS)
  {
    auto floorNode = floorRequest->findFloor(floorReqStatus.floorID);
    assert(floorNode);
    floorNode->setStatus(BFCP_DENIED);
    floorNode->setStatusInfo(floorReqStatus.statusInfo);
  }
  
  floorRequest->setStatusInfo(info.oRS.statusInfo);
  floorRequest->setOverallStatus(BFCP_DENIED);
  notifyFloorAndRequestInfo(floorRequest);
}

FloorRequestNodePtr Conference::checkFloorRequestInPendingQueue(
  const BfcpMsgPtr &msg, uint16_t floorRequestID)
{
  auto floorRequest = findFloorRequest(pending_, floorRequestID);
  if (!floorRequest)
  {
    char errorInfo[128];
    snprintf(errorInfo, sizeof errorInfo,
      "Pending FloorRequest %hu does not exist in Conference %u",
      floorRequestID, conferenceID_);
    replyWithError(msg, BFCP_FLOOR_REQ_ID_NOT_EXIST, errorInfo);
    return nullptr;
  }
  return floorRequest;
}

FloorRequestNodePtr Conference::findFloorRequest(
  FloorRequestQueue &queue, uint16_t floorRequestID)
{
  auto it = std::find_if(queue.begin(), queue.end(), 
    detail::FloorRequestCmp(floorRequestID));
  return it != queue.end() ? *it : nullptr;
}

bool Conference::checkFloorsInFloorRequest(
  const BfcpMsgPtr &msg, 
  FloorRequestNodePtr &floorRequest, 
  const bfcp_floor_request_status_list &fRS)
{
  for (auto &floorReqStatus : fRS)
  {
    if (!floorRequest->findFloor(floorReqStatus.floorID))
    {
      char errorInfo[128];
      snprintf(errorInfo, sizeof errorInfo,
        "FloorRequest %hu does not request Floor %hu",
        floorRequest->getFloorRequestID(), floorReqStatus.floorID);
      replyWithError(msg, BFCP_INVALID_FLOOR_ID, errorInfo);
      return false;
    }
  }
  return true;
}

void Conference::revokeFloorRequest(
  const BfcpMsgPtr &msg, const bfcp_floor_request_info &info)
{
  assert(info.oRS.requestStatus);
  assert(info.oRS.requestStatus->status == BFCP_REVOKED);
  auto floorRequest = checkFloorRequestInGrantedQueue(msg, info.floorRequestID);
  if (!floorRequest) return;

  // FIXME: check if all floors in chair action are requested by the floor request
  if (!checkFloorsInFloorRequest(msg, floorRequest, info.fRS))
    return;

  // reply chair action ack
  connection_->replyWithChairActionAck(msg);

  granted_.remove(floorRequest);

  revokeFloorsFromFloorRequest(floorRequest);

  for (auto &floorReqStatus : info.fRS)
  {
    auto floorNode = floorRequest->findFloor(floorReqStatus.floorID);
    assert(floorNode);
    floorNode->setStatus(BFCP_REVOKED);
    floorNode->setStatusInfo(floorReqStatus.statusInfo);
  }

  floorRequest->setStatusInfo(info.oRS.statusInfo);
  floorRequest->setOverallStatus(BFCP_REVOKED);
  notifyFloorAndRequestInfo(floorRequest);
  tryToGrantFloorRequestsWithAllFloors();
}

FloorRequestNodePtr Conference::checkFloorRequestInGrantedQueue( 
  const BfcpMsgPtr &msg, uint16_t floorRequestID)
{
  auto floorRequest = findFloorRequest(granted_, floorRequestID);
  if (!floorRequest)
  {
    char errorInfo[128];
    snprintf(errorInfo, sizeof errorInfo,
      "Granted FloorRequest %hu does not exist in Conference %u",
      floorRequestID, conferenceID_);
    replyWithError(msg, BFCP_FLOOR_REQ_ID_NOT_EXIST, errorInfo);
    return nullptr;
  }
  return floorRequest;
}

void Conference::handleFloorRequestQuery( const BfcpMsgPtr &msg )
{
  assert(msg->primitive() == BFCP_FLOOR_REQUEST_QUERY);
  auto attr = msg->findAttribute(BFCP_FLOOR_REQUEST_ID);
  if (!attr)
  {
    replyWithError(msg, BFCP_PARSE_ERROR,
      "Missing FLOOR-REQUEST-ID attribute");
    return;
  }
  BfcpAttr floorRequestIDAttr(*attr);
  uint16_t floorRequestID = floorRequestIDAttr.getFloorRequestID();

  // find floor request in pending, accepted and granted queue
  auto floorRequest = findFloorRequest(pending_, floorRequestID);
  if (!floorRequest)
  {
    floorRequest = findFloorRequest(accepted_, floorRequestID);
  }
  if (!floorRequest)
  {
    floorRequest = findFloorRequest(granted_, floorRequestID);
  }

  if (!floorRequest) // floor request not found
  {
    char errorInfo[128];
    snprintf(errorInfo, sizeof errorInfo,
      "FloorRequest %hu does not exist in Conference %u",
      floorRequestID, conferenceID_);
    replyWithError(msg, BFCP_FLOOR_REQ_ID_NOT_EXIST, errorInfo);
    return;
  }

  replyWithFloorRequestStatus(msg, floorRequest);
  LOG_INFO << "Add Query User " << msg->getUserID() 
           << " to FloorRequest " << floorRequestID
           << " in Conference " << conferenceID_;
  floorRequest->addQueryUser(msg->getUserID()); 
}

void Conference::handleUserQuery( const BfcpMsgPtr &msg )
{
  assert(msg->primitive() == BFCP_USER_QUERY);
  UserStatusParam param;

  UserPtr user;
  auto attr = msg->findAttribute(BFCP_BENEFICIARY_ID);
  if (!attr)
  {
    user = findUser(msg->getUserID());
  }
  else
  {
    BfcpAttr beneficiaryIDAttr(*attr);
    user = findUser(beneficiaryIDAttr.getBeneficiaryID());
    if (!user) // beneficiary user not found
    {
      char errorInfo[200];
      snprintf(errorInfo, sizeof errorInfo, 
        "User %hu (beneficiary of the query) does not exist in Conference %u",
        beneficiaryIDAttr.getBeneficiaryID(),
        conferenceID_);
      replyWithError(msg, BFCP_USER_NOT_EXIST, errorInfo);
      return;
    }

    param.hasBeneficiary = true;
    param.beneficiary = user->toUserInfoParam();
  }

  assert(user);

  uint16_t userID = user->getUserID();
  getFloorRequestInfoParamsByUserID(param.frqInfoList, userID, granted_);
  getFloorRequestInfoParamsByUserID(param.frqInfoList, userID, accepted_);
  getFloorRequestInfoParamsByUserID(param.frqInfoList, userID, pending_);
  
  connection_->replyWithUserStatus(msg, param);
}

void Conference::getFloorRequestInfoParamsByUserID(
  FloorRequestInfoParamList &frqInfoList,
  uint16_t userID, 
  const FloorRequestQueue &queue) const
{
  for (auto it = queue.rbegin(); it != queue.rend(); ++it)
  {
    if ((*it)->getUserID() == userID || 
        ((*it)->hasBeneficiary() && (*it)->getBeneficiaryID() == userID))
    { 
      frqInfoList.push_back((*it)->toFloorRequestInfoParam(users_));
    }
  }
}

void Conference::handleFloorQuery( const BfcpMsgPtr &msg )
{
  assert(msg->primitive() == BFCP_FLOOR_QUERY);
  uint16_t userID = msg->getUserID();
  auto floorIDs = msg->getFloorIDs();
  // first clear the floor query of the user
  for (auto floor : floors_)
  {
    floor.second->removeQueryUser(userID);
  }
  // set the new floor query of the user
  uint16_t invalidFloorID = 0;
  bool hasInvalidFloor = false;
  for (auto floorID : floorIDs)
  {
    auto floor = findFloor(floorID);
    if (floor)
    {
      LOG_INFO << "Add Query User " << msg->getUserID() 
               << " to Floor " << floorID
               << " in Conference " << conferenceID_;
      floor->addQueryUser(userID);
    }
    else
    {
      hasInvalidFloor = true;
      invalidFloorID = floorID;
    }
  }

  if (hasInvalidFloor)
  {
    char errorInfo[128]; 
    snprintf(errorInfo, sizeof errorInfo,
      "Floor %hu does not exist in Conference %u",
      invalidFloorID, conferenceID_);
    replyWithError(msg, BFCP_INVALID_FLOOR_ID, errorInfo);
    return;
  }

  if (floorIDs.empty())
  {
    replyWithFloorStatus(msg, nullptr);
  }
  else
  {
    auto it = floorIDs.begin();
    replyWithFloorStatus(msg, &(*it));
    ++it;
    for (; it != floorIDs.end(); ++it)
    {
      notifyWithFloorStatus(userID, *it);
    }
  }
}

void Conference::replyWithFloorStatus(const BfcpMsgPtr &msg, const uint16_t *floorID)
{
  FloorStatusParam param;
  if (floorID)
  {
    param = getFloorStatusParam(*floorID);
  }
  connection_->replyWithFloorStatus(msg, param);
}

void Conference::notifyWithFloorStatus(uint16_t userID, uint16_t floorID)
{
  auto user = findUser(userID);
  if (user && isUserAvailable(user))
  {
    BasicRequestParam basicParam;
    basicParam.conferenceID = conferenceID_;
    basicParam.userID = user->getUserID();
    basicParam.dst = user->getAddr();
    assert(clientReponseCallback_);
    basicParam.cb = boost::bind(clientReponseCallback_, 
      conferenceID_, BFCP_FLOOR_STATUS_ACK, user->getUserID(), _1, _2);
    user->runSendMessageTask(boost::bind(&BfcpConnection::notifyFloorStatus,
      connection_, basicParam, getFloorStatusParam(floorID)));
  }
}

FloorStatusParam Conference::getFloorStatusParam( uint16_t floorID ) const
{
  FloorStatusParam param;
  param.setFloorID(floorID);
  getFloorRequestInfoParamsByFloorID(param.frqInfoList, floorID, granted_);
  getFloorRequestInfoParamsByFloorID(param.frqInfoList, floorID, accepted_);
  getFloorRequestInfoParamsByFloorID(param.frqInfoList, floorID, pending_);
  return param;
}

void Conference::getFloorRequestInfoParamsByFloorID(
  FloorRequestInfoParamList &frqInfoList, 
  uint16_t floorID, 
  const FloorRequestQueue &queue) const
{
  for (auto it = queue.rbegin(); it != queue.rend(); ++it)
  {
    if ((*it)->findFloor(floorID))
    {
      frqInfoList.push_back((*it)->toFloorRequestInfoParam(users_));
    }
  }
}

void Conference::onResponse(bfcp_prim expectedPrimitive,
                            uint16_t userID,
                            ResponseError err, 
                            const BfcpMsgPtr &msg)
{
  auto user = findUser(userID);
  if (!user)
  {
    LOG_WARN << "Conference::onReponse: User " << userID 
             << " doesn't exist in Conference " << conferenceID_;
    return;
  }

  if (err != ResponseError::kNoError)
  {
    LOG_TRACE << "Conference received response with error " 
              << response_error_name(err);
    LOG_INFO << "Set User " << userID << " in Conference " << conferenceID_ 
             <<" to unavailable";
    user->setAvailable(false);
    user->clearAllSendMessageTasks();
  }
  else
  {
    assert(msg->valid());
    assert(msg->getUserID() == userID);
    // FIXME: currently treat unexpected primitive as ACK
    if (msg->primitive() != expectedPrimitive)
    {
      LOG_ERROR << "Expected BFCP " << bfcp_prim_name(expectedPrimitive) 
                << " but get " << bfcp_prim_name(msg->primitive());
    }
    user->setActiveTime(msg->getReceivedTime());
    if (isUserAvailable(user))
    {
      user->runNextSendMessageTask();
    }
  }
}

string Conference::getConferenceInfo() const
{
  tinyxml2::XMLDocument doc;
  tinyxml2::XMLElement *conferenceElement = doc.NewElement("conference");
  doc.InsertEndChild(conferenceElement);
  
  // add conference info
  conferenceElement->SetAttribute("id", conferenceID_);
  conferenceElement->SetAttribute("maxFloorRequest", maxFloorRequest_);
  conferenceElement->SetAttribute("timeForChairAction", timeForChairAction_);
  conferenceElement->SetAttribute("acceptPolicy", toString(acceptPolicy_));
  conferenceElement->SetAttribute("userObsoletedTime", userObsoletedTime_);

  addUserInfoToXMLNode(&doc, conferenceElement);
  addFloorInfoToXMLNode(&doc, conferenceElement);
  addQueueInfoToXMLNode(&doc, conferenceElement, pending_, "pendingQueue");
  addQueueInfoToXMLNode(&doc, conferenceElement, accepted_, "acceptedQueue");
  addQueueInfoToXMLNode(&doc, conferenceElement, granted_, "grantedQueue");

  tinyxml2::XMLPrinter printer;
  doc.Print(&printer);
  return printer.CStr();
}

void Conference::addUserInfoToXMLNode(tinyxml2::XMLDocument *doc, 
                                      tinyxml2::XMLNode *node) const
{
  tinyxml2::XMLNode *userListNode = node->InsertEndChild(doc->NewElement("users"));
  for (const auto &user : users_)
  {
    tinyxml2::XMLElement *userNode = doc->NewElement("user");
    userNode->SetAttribute("id", user.second->getUserID());
    if (!user.second->getDisplayName().empty())
    {
      userNode->SetAttribute("displayName", user.second->getDisplayName().c_str());
    }
    if (!user.second->getURI().empty())
    {
      userNode->SetAttribute("uri", user.second->getURI().c_str());
    }
    userNode->SetAttribute("isAvailable", isUserAvailable(user.second));
    userListNode->InsertEndChild(userNode);
  }
}

void Conference::addFloorInfoToXMLNode(tinyxml2::XMLDocument *doc, 
                                       tinyxml2::XMLNode *node) const
{
  tinyxml2::XMLNode *floorsListNode = node->InsertEndChild(doc->NewElement("floors"));
  for (const auto &floor : floors_)
  {
    tinyxml2::XMLElement *floorNode = doc->NewElement("floor");
    floorNode->SetAttribute("id", floor.second->getFloorID());
    floorNode->SetAttribute("isAssigned", floor.second->isAssigned());
    if (floor.second->isAssigned())
    {
      floorNode->SetAttribute("chairID", floor.second->getChairID());
    }
    floorNode->SetAttribute("maxGrantedCount", floor.second->getMaxGrantedCount());
    floorNode->SetAttribute("currentGrantedCount", floor.second->getGrantedCount());
    // add query users
    tinyxml2::XMLNode *queryUsersListNode = 
      floorNode->InsertEndChild(doc->NewElement("queryUsers"));
    for (auto userID : floor.second->getQueryUsers())
    {
      tinyxml2::XMLElement *queryUserNode = doc->NewElement("user");
      queryUserNode->SetAttribute("id", userID);
      queryUsersListNode->InsertEndChild(queryUserNode);
    }
    floorsListNode->InsertEndChild(floorNode);
  }
}

void Conference::addQueueInfoToXMLNode(tinyxml2::XMLDocument *doc, 
                                       tinyxml2::XMLNode *node, 
                                       const FloorRequestQueue &queue, 
                                       const string &queueName) const
{
  tinyxml2::XMLNode *queueNode = node->InsertEndChild(doc->NewElement(queueName.c_str()));
  for (const auto &floorRequest : queue)
  {
    addFloorRequestInfoToXMLNode(doc, queueNode, floorRequest);
  }
}

void Conference::addFloorRequestInfoToXMLNode(tinyxml2::XMLDocument *doc, 
                                              tinyxml2::XMLNode *node,
                                              const FloorRequestNodePtr &floorRequest) const
{
  tinyxml2::XMLElement *requestNode = doc->NewElement("floorRequest");
  requestNode->SetAttribute("id", floorRequest->getFloorRequestID());
  requestNode->SetAttribute("userID", floorRequest->getUserID());
  requestNode->SetAttribute("hasBeneficiaryID", floorRequest->hasBeneficiary());
  if (floorRequest->hasBeneficiary())
  {
    requestNode->SetAttribute("beneficiaryID", floorRequest->getBeneficiaryID());
  }
  requestNode->SetAttribute("priority", floorRequest->getPriority());
  if (!floorRequest->getParticipantInfo().empty())
  {
    requestNode->SetAttribute("participantInfo", floorRequest->getParticipantInfo().c_str());
  }
  requestNode->SetAttribute(
    "overallStatus", 
    bfcp_reqstatus_name(floorRequest->getOverallStatus()));

  requestNode->SetAttribute("queuePosition", floorRequest->getQueuePosition());
  if (!floorRequest->getStatusInfo().empty())
  {
    requestNode->SetAttribute("statusInfo", floorRequest->getStatusInfo().c_str());
  }
  // add floors
  tinyxml2::XMLNode *floorsListNode = 
    requestNode->InsertEndChild(doc->NewElement("floors"));
  for (const auto &floor : floorRequest->getFloorNodeList())
  {
    tinyxml2::XMLElement *floorNode = doc->NewElement("floor");
    floorNode->SetAttribute("id", floor.getFloorID());
    floorNode->SetAttribute("status", bfcp_reqstatus_name(floor.getStatus()));
    if (!floor.getStatusInfo().empty())
    {
      floorNode->SetAttribute("statusInfo", floor.getStatusInfo().c_str());
    }
    floorsListNode->InsertEndChild(floorNode);
  }
  // add query user
  tinyxml2::XMLNode *queryUsersListNode = 
    requestNode->InsertEndChild(doc->NewElement("queryUsers"));
  for (auto userID : floorRequest->getFloorRequestQueryUsers())
  {
    tinyxml2::XMLElement *queryUserNode = doc->NewElement("user");
    queryUserNode->SetAttribute("id", userID);
    queryUsersListNode->InsertEndChild(queryUserNode);
  }
  node->InsertEndChild(requestNode);
}

} // namespace bfcp
