#include <bfcp/server/conference.h>

#include <boost/bind.hpp>
#include <muduo/base/Logging.h>

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

Conference::Conference(muduo::net::EventLoop *loop,
                       const BfcpConnectionPtr &connection, 
                       uint32_t conferenceID, 
                       uint16_t maxFloorRequest, 
                       AcceptPolicy acceptPolicy, 
                       double timeForChairAction)
    : loop_(CHECK_NOTNULL(loop)),
      connection_(connection),
      conferenceID_(conferenceID),
      nextFloorRequestID_(0),
      maxFloorRequest_(maxFloorRequest),
      timeForChairAction_(timeForChairAction),
      acceptPolicy_(acceptPolicy),
      chairActionTimeoutCallback_(
        boost::bind(&Conference::onTimeoutForChairAction, this, _1)),
      clientReponseCallback_(
        boost::bind(&Conference::onResponse, this, _1, _2, _3, _4))
{
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
    boost::bind(&Conference::handleGoodBye, this, _1)));
}

Conference::ControlError 
Conference::addUser( uint16_t userID, const string &userURI, const string &displayName )
{
  ControlError err = kNoError;
  auto lb = users_.lower_bound(userID);
  if (lb == users_.end() || users_.key_comp()((*lb).first, userID))
  {
    auto res = users_.emplace_hint(
      lb,
      userID, 
      boost::make_shared<User>(userID, displayName, userURI));
    // FIXME: check if insert success
  }
  else // This user already exists
  {
    err = kUserAlreadyExist;
  }
  return err;
}

Conference::ControlError Conference::removeUser( uint16_t userID )
{
  auto user = findUser(userID);
  if (!user)
  {
    return kUserNoExist;
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
  cancelFloorRequestsFromGrantedByUserID(userID);

  users_.erase(userID);

  tryToGrantFloorRequestsWithAllFloors();

  return kNoError;
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
      it = pending_.erase(it);
      loop_->cancel(floorRequest->getTimerForChairAction());
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

void Conference::cancelFloorRequestsFromGrantedByUserID( uint16_t userID )
{
  for (auto it = granted_.begin(); it != granted_.end();)
  {
    auto floorRequest = *it;
    if (floorRequest->getUserID() == userID ||
        (floorRequest->hasBeneficiary() && 
         floorRequest->getBeneficiaryID() == userID))
    {
      it = granted_.erase(it);
      floorRequest->setOverallStatus(BFCP_RELEASED);
      floorRequest->removeQueryUser(userID);
      revokeFloorsFromFloorRequest(floorRequest);
      notifyFloorAndRequestInfo(floorRequest);
      continue;
    }
    ++it;
  }
}

Conference::ControlError 
Conference::addFloor( uint16_t floorID, uint16_t maxGrantedCount )
{
  ControlError err = kNoError;
  auto lb = floors_.lower_bound(floorID);
  if (lb == floors_.end() || floors_.key_comp()((*lb).first, floorID))
  {
    auto res = floors_.emplace_hint(
      lb, 
      floorID, 
      boost::make_shared<Floor>(floorID, maxGrantedCount));
    // FIXME: check if insert success
  }
  else // This floor already exists
  {
    err = kFloorAlreadyExist;
  }
  return err;
}

Conference::ControlError Conference::removeFloor( uint16_t floorID )
{
  auto floor = findFloor(floorID);
  if (!floor)
  {
    return kFloorNoExist;
  }
  cancelFloorRequestsFromPendingByFloorID(floorID);
  cancelFloorRequestsFromAcceptedByFloorID(floorID);
  cancelFloorRequestsFromGrantedByFloorID(floorID);
  
  floors_.erase(floorID);

  tryToGrantFloorRequestsWithAllFloors();
  return kNoError;
}

void Conference::cancelFloorRequestsFromPendingByFloorID(uint16_t floorID)
{
  for (auto it = pending_.begin(); it != pending_.end();)
  {
    if ((*it)->findFloor(floorID))
    {
      auto floorRequest = *it;
      loop_->cancel(floorRequest->getTimerForChairAction());
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

void Conference::cancelFloorRequestsFromGrantedByFloorID(uint16_t floorID)
{
  for (auto it = granted_.begin(); it != granted_.end();)
  {
    if ((*it)->findFloor(floorID))
    {
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

Conference::ControlError
Conference::addChair( uint16_t floorID, uint16_t userID )
{
  ControlError err = kNoError;
  do 
  {
    auto user = findUser(userID);
    if (!user)
    {
      err = kUserNoExist;
      break;
    }
    auto floor = findFloor(floorID);
    if (!floor)
    {
      err = kFloorNoExist;
      break;
    }
    if (floor->isAssigned())
    {
      err = kChairAlreadyExist;
      break;
    }
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

Conference::ControlError Conference::removeChair( uint16_t floorID )
{
  ControlError err = kNoError;
  do 
  {
    auto floor = findFloor(floorID);
    if (!floor)
    {
      err = kFloorNoExist;
      break;
    }

    if (!floor->isAssigned())
    {
      err = kChairNoExist;
      break;
    }

    assert(findUser(floor->getChairID()));
    if (acceptPolicy_ == kAutoDeny)
    {
      cancelFloorRequestsFromPendingByFloorID(floorID);
    }
    else if (acceptPolicy_ == kAutoAccept)
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
    auto &floorRequest = *it;
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
        loop_->cancel(floorRequest->getTimerForChairAction());
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
  assert(floorRequest->isAnyFloorStatus(BFCP_PENDING));
  floorRequest->setOverallStatus(BFCP_PENDING);
  floorRequest->setQueuePosition(0);
  insertFloorRequestToQueue(pending_, floorRequest);
  notifyFloorAndRequestInfo(floorRequest);
}

void Conference::insertFloorRequestToAcceptedQueue( 
  FloorRequestNodePtr &floorRequest)
{
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
  assert(floorRequest->isAllFloorStatus(BFCP_GRANTED));
  floorRequest->setOverallStatus(BFCP_GRANTED);
  floorRequest->setQueuePosition(0);
  floorRequest->setPrioriy(BFCP_PRIO_LOWEST);
  insertFloorRequestToQueue(granted_, floorRequest);
  notifyFloorAndRequestInfo(floorRequest);
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
      it = (++rit).base();
    }
    else
    {
      it = queue.begin();
    }
    floorRequest->setPrioriy(BFCP_PRIO_LOWEST);
  }
  floorRequest->setQueuePosition(0);

  if (it == queue.end())
  {
    queue.push_front(floorRequest);
  }
  else if (floorRequest->getPriority() <= (*it)->getPriority())
  {
    queue.insert(it, floorRequest);
  }
  else
  {
    ++it;
    while (it != queue.end() && (*it)->getPriority() < floorRequest->getPriority()) {
      ++it;
    }
    --it;
    queue.insert(it, floorRequest);
  }
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
  for (auto userID : floor->getQueryUsers())
  {
    auto user = findUser(userID);
    if (user && user->isAvailable())
    {
      basicParam.userID = userID;
      basicParam.dst = user->getAddr();
      basicParam.cb = boost::bind(clientReponseCallback_,
        BFCP_FLOOR_STATUS_ACK, userID, _1, _2);
      user->trySendMessageAction(
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
    if (user && user->isAvailable())
    {
      param.userID = userID;
      param.dst = user->getAddr();
      param.cb = boost::bind(clientReponseCallback_, 
        BFCP_FLOOR_REQ_STATUS_ACK, userID, _1, _2);
      user->trySendMessageAction(
        boost::bind(&BfcpConnection::notifyFloorRequestStatus, 
        connection_, param, frqInfo));
    }
  }
}

void Conference::onNewRequest( const BfcpMsg &msg )
{
  LOG_TRACE << "Conference received new request " << msg.toString();
  assert(msg.valid());
  assert(!msg.isResponse());
  assert(msg.getConferenceID() == conferenceID_);

  if (checkUnknownAttrs(msg) && checkUserID(msg, msg.getUserID()))
  {
    auto user = findUser(msg.getUserID());
    assert(user);
    if (!user->isAvailable())
    {
      user->setAvailable(true);
      user->setAddr(msg.getSrc());
    }

    bfcp_prim prim = msg.primitive();
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

bool Conference::checkUnknownAttrs( const BfcpMsg &msg )
{
  auto &unknownAttrs = msg.getUnknownAttrs();
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

bool Conference::checkUserID( const BfcpMsg &msg, uint16_t userID )
{
  if (!findUser(userID))
  {
    char errorInfo[128];
    snprintf(errorInfo, sizeof errorInfo, 
      "User %hu does not exist in Conference %lu",
      userID, msg.getConferenceID());
    replyWithError(msg, BFCP_USER_NOT_EXIST, errorInfo);
    return false;
  }
  return true;
}

void Conference::replyWithError(const BfcpMsg &msg, 
                                bfcp_err err, 
                                const char *errInfo)
{
  ErrorParam param;
  param.errorCode.code = err;
  param.setErrorInfo(errInfo);
  connection_->replyWithError(msg, param);
}

void Conference::handleHello( const BfcpMsg &msg )
{
  assert(msg.primitive() == BFCP_HELLO);

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

void Conference::handleGoodBye( const BfcpMsg &msg )
{
  assert(msg.primitive() == BFCP_GOODBYE);
  
  connection_->replyWithGoodByeAck(msg);

  auto user = findUser(msg.getUserID());
  assert(user);
  user->setAvailable(false);
}

void Conference::handleFloorRequest( const BfcpMsg &msg )
{
  assert(msg.primitive() == BFCP_FLOOR_REQUEST);

  FloorRequestParam param;
  if (!parseFloorRequestParam(param, msg)) return;

  auto user = findUser(msg.getUserID());
  assert(user);
  auto beneficiaryUser = user;
  if (param.hasBeneficiaryID)
  {
    // NOTE: Third-party FloorRequests only allowed for chairs
    if (!isChair(msg.getUserID()))
    {
      char errorInfo[128];
      snprintf(errorInfo, sizeof errorInfo,
        "Third-party FloorRequests only allowed for chairs "
        "(User %hu is not chair of any floor)",
        msg.getUserID());
      replyWithError(msg, BFCP_UNAUTH_OPERATION, errorInfo);
      return;
    }

    if (!checkUserID(msg, param.beneficiaryID)) return;
    beneficiaryUser = findUser(param.beneficiaryID);
    assert(beneficiaryUser);
  }

  if (!checkFloorIDs(msg, param.floorIDs)) return;

  FloorRequestNodePtr newFloorRequest = 
    boost::make_shared<FloorRequestNode>(
      nextFloorRequestID_++, msg.getUserID(), param);

  for (auto &floorNode : newFloorRequest->getFloorNodeList())
  {
    uint16_t floorID = floorNode.getFloorID();
    auto floor = findFloor(floorID);
    assert(floor);
    if (!floor->isAssigned() && acceptPolicy_ == kAutoDeny)
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
        "for the same floor in Conference %lu",
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
  newFloorRequest->addQueryUser(msg.getUserID());

  // handle chair part
  for (auto &floorNode : newFloorRequest->getFloorNodeList())
  {
    auto floor = findFloor(floorNode.getFloorID());
    assert(floor);
    if (!floor->isAssigned())
    {
      assert(acceptPolicy_ == kAutoAccept);
      floorNode.setStatus(BFCP_ACCEPTED);
    }
    else
    {
      uint16_t chairID = floor->getChairID();
      assert(findUser(chairID));
      // start timer for chair action
      muduo::net::TimerId timerId = loop_->runAfter(
        timeForChairAction_, 
        boost::bind(
          chairActionTimeoutCallback_, 
          newFloorRequest->getFloorRequestID()));
      newFloorRequest->setTimerForChairAction(timerId);
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

bool Conference::parseFloorRequestParam(FloorRequestParam &param, const BfcpMsg &msg)
{
  param.floorIDs = msg.getFloorIDs();
  if (param.floorIDs.empty())
  {
    replyWithError(msg, BFCP_PARSE_ERROR, 
      "Missing FLOOR-ID attribute");
    return false;
  }

  auto attr = msg.findAttribute(BFCP_BENEFICIARY_ID);
  if (attr)
  {
    BfcpAttr beneficiaryIDAttr(*attr);
    param.hasBeneficiaryID = true;
    param.beneficiaryID = beneficiaryIDAttr.getBeneficiaryID();
  }

  attr = msg.findAttribute(BFCP_PART_PROV_INFO);
  if (attr)
  {
    BfcpAttr partProvInfo(*attr);
    detail::setString(param.pInfo, partProvInfo.getParticipantProvidedInfo());
  }

  attr = msg.findAttribute(BFCP_PRIORITY);
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

bool Conference::checkFloorIDs( const BfcpMsg &msg, const bfcp_floor_id_list &floorIDs )
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

bool Conference::checkFloorID( const BfcpMsg &msg, uint16_t floorID )
{
  if (!findFloor(floorID))
  {
    char errorInfo[128];
    snprintf(errorInfo, sizeof errorInfo, 
      "Floor %hu does not exist in Conference %lu",
      floorID, conferenceID_);
    replyWithError(msg, BFCP_INVALID_FLOOR_ID, errorInfo);
    return false;
  }
  return true;
}

void Conference::replyWithFloorRequestStatus(
  const BfcpMsg &msg, FloorRequestNodePtr &floorRequest)
{
  FloorRequestInfoParam param = 
    floorRequest->toFloorRequestInfoParam(users_);
  connection_->replyWithFloorRequestStatus(msg, param);
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
  auto floorRequest = findFloorRequest(pending_, floorRequestID);
  if (!floorRequest) return;

  LOG_INFO << "FloorRequest " << floorRequestID << " is cancelled for timeout";
  for (auto &floorNode : floorRequest->getFloorNodeList())
  {
    if (floorNode.getStatus() == BFCP_PENDING)
      floorNode.setStatus(BFCP_CANCELLED);
  }

  pending_.remove(floorRequest);
  revokeFloorsFromFloorRequest(floorRequest);
  floorRequest->setOverallStatus(BFCP_CANCELLED);
  notifyFloorAndRequestInfo(floorRequest);
}

void Conference::handleFloorRelease( const BfcpMsg &msg )
{
  assert(msg.primitive() == BFCP_FLOOR_RELEASE);
  
  auto attr = msg.findAttribute(BFCP_FLOOR_REQUEST_ID);
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
    removeFloorRequest(floorRequestID, msg.getUserID());
  if (!floorRequest)
  {
    char errorInfo[128];
    snprintf(errorInfo, sizeof errorInfo, 
      "FloorRequest %hu does not exist in Conference %lu",
      floorRequestID, conferenceID_);
    replyWithError(msg, BFCP_FLOOR_REQ_ID_NOT_EXIST, errorInfo);
    return;
  }

  // reply the releaser about the floor request status of the release request
  replyWithFloorRequestStatus(msg, floorRequest);
  floorRequest->removeQueryUser(msg.getUserID());
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
    floorRequest->setOverallStatus(BFCP_CANCELLED);
    loop_->cancel(floorRequest->getTimerForChairAction());
    return floorRequest;
  }

  // check the accepted queue
  floorRequest = 
    extractFloorRequestFromQueue(accepted_, floorRequestID, userID);
  if (floorRequest)
  {
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
    floorRequest->setOverallStatus(BFCP_RELEASED);
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

void Conference::handleChairAction( const BfcpMsg &msg )
{
  assert(msg.primitive() == BFCP_CHAIR_ACTION);

  auto attr = msg.findAttribute(BFCP_FLOOR_REQ_INFO);
  if (!attr)
  {
    replyWithError(msg, BFCP_PARSE_ERROR, 
      "Missing FLOOR-REQUEST-INFORMATION attribute");
    return;
  }

  BfcpAttr floorReqInfoAttr(*attr);
  auto info = floorReqInfoAttr.getFloorRequestInfo();
  if (!checkFloorRequestInfo(msg, info)) return;
  
  uint16_t floorRequestID = info.floorRequestID;
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
  const BfcpMsg &msg, const bfcp_floor_request_info &info)
{
  if (!info.fRS.empty())
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
          checkFloorChair(msg, floorReqStatus.floorID, msg.getUserID())))
    {
      return false;
    }
  }
  return true;
}

bool Conference::checkFloorChair(
  const BfcpMsg &msg, uint16_t floorID, uint16_t userID)
{
  auto floor = findFloor(floorID);
  assert(floor);
  if (!floor->isAssigned() || floor->getChairID() != userID)
  {
    char errorInfo[128];
    snprintf(errorInfo, sizeof errorInfo,
      "User %hu is not chair of Floor %hu in Conference %lu",
      userID, floorID, conferenceID_);
    replyWithError(msg, BFCP_UNAUTH_OPERATION, errorInfo);
    return false;
  }
  return true;
}

void Conference::acceptFloorRequest(
  const BfcpMsg &msg, const bfcp_floor_request_info &info)
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
    loop_->cancel(floorRequest->getTimerForChairAction());
    insertFloorRequestToAcceptedQueue(floorRequest);
    tryToGrantFloorRequestWithAllFloors(floorRequest);
  }
}

void Conference::denyFloorRequest(
  const BfcpMsg &msg, const bfcp_floor_request_info &info)
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

  loop_->cancel(floorRequest->getTimerForChairAction());
  pending_.remove(floorRequest);

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
  const BfcpMsg &msg, uint16_t floorRequestID)
{
  auto floorRequest = findFloorRequest(pending_, floorRequestID);
  if (!floorRequest)
  {
    char errorInfo[128];
    snprintf(errorInfo, sizeof errorInfo,
      "Pending FloorRequest %hu does not exist in Conference %lu",
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
  const BfcpMsg &msg, 
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
  const BfcpMsg &msg, const bfcp_floor_request_info &info)
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

  for (auto &floorReqStatus : info.fRS)
  {
    auto floorNode = floorRequest->findFloor(floorReqStatus.floorID);
    assert(floorNode);
    floorNode->setStatus(BFCP_REVOKED);
    floorNode->setStatusInfo(floorReqStatus.statusInfo);
  }

  revokeFloorsFromFloorRequest(floorRequest);

  floorRequest->setStatusInfo(info.oRS.statusInfo);
  floorRequest->setOverallStatus(BFCP_REVOKED);
  notifyFloorAndRequestInfo(floorRequest);
  tryToGrantFloorRequestsWithAllFloors();
}

FloorRequestNodePtr Conference::checkFloorRequestInGrantedQueue( 
  const BfcpMsg &msg, uint16_t floorRequestID)
{
  auto floorRequest = findFloorRequest(granted_, floorRequestID);
  if (!floorRequest)
  {
    char errorInfo[128];
    snprintf(errorInfo, sizeof errorInfo,
      "Granted FloorRequest %hu does not exist in Conference %lu",
      floorRequestID, conferenceID_);
    replyWithError(msg, BFCP_FLOOR_REQ_ID_NOT_EXIST, errorInfo);
    return nullptr;
  }
  return floorRequest;
}

void Conference::handleFloorRequestQuery( const BfcpMsg &msg )
{
  assert(msg.primitive() == BFCP_FLOOR_REQUEST_QUERY);
  auto attr = msg.findAttribute(BFCP_FLOOR_REQUEST_ID);
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
  if (!floorRequestID)
  {
    floorRequest = findFloorRequest(granted_, floorRequestID);
  }

  if (!floorRequest) // floor request not found
  {
    char errorInfo[128];
    snprintf(errorInfo, sizeof errorInfo,
      "FloorRequest &hu does not exist in Conference %lu",
      floorRequestID, conferenceID_);
    replyWithError(msg, BFCP_FLOOR_REQ_ID_NOT_EXIST, errorInfo);
    return;
  }

  replyWithFloorRequestStatus(msg, floorRequest);
  floorRequest->addQueryUser(msg.getUserID()); 
}

void Conference::handleUserQuery( const BfcpMsg &msg )
{
  assert(msg.primitive() == BFCP_USER_QUERY);
  UserStatusParam param;

  UserPtr user;
  auto attr = msg.findAttribute(BFCP_BENEFICIARY_ID);
  if (!attr)
  {
    user = findUser(msg.getUserID());
  }
  else
  {
    BfcpAttr beneficiaryIDAttr(*attr);
    user = findUser(beneficiaryIDAttr.getBeneficiaryID());
    if (!user) // beneficiary user not found
    {
      char errorInfo[200];
      snprintf(errorInfo, sizeof errorInfo, 
        "User %hu (beneficiary of the query) does not exist in Conference %lu",
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

void Conference::handleFloorQuery( const BfcpMsg &msg )
{
  assert(msg.primitive() == BFCP_FLOOR_QUERY);
  uint16_t userID = msg.getUserID();
  auto floorIDs = msg.getFloorIDs();
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
      "Floor %hu does not exist in Conference %lu",
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

void Conference::replyWithFloorStatus(const BfcpMsg &msg, const uint16_t *floorID)
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
  if (user && user->isAvailable())
  {
    BasicRequestParam basicParam;
    basicParam.conferenceID = conferenceID_;
    basicParam.userID = user->getUserID();
    basicParam.dst = user->getAddr();
    basicParam.cb = boost::bind(
      clientReponseCallback_, BFCP_FLOOR_STATUS_ACK, user->getUserID(), _1, _2);
    user->trySendMessageAction(boost::bind(&BfcpConnection::notifyFloorStatus,
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
                            const BfcpMsg &msg)
{
  auto user = findUser(userID);
  if (!user)
  {
    LOG_WARN << "Conference::onReponse: User " << userID 
             << " doesn't exist in Conference " << conferenceID_;
    return;
  }

  if (err != kNoError)
  {
    LOG_TRACE << "Conference received response with error " 
              << response_error_name(err);
    LOG_INFO << "Set User " << userID << " in Conference " << conferenceID_ 
             <<" to unavailable";
    user->setAvailable(false);
    user->clearAllSendMessageAction();
  }
  else
  {
    assert(msg.valid());
    assert(msg.getUserID() == userID);
    // FIXME: currently treat unexpected primitive as ACK
    if (msg.primitive() != expectedPrimitive)
    {
      LOG_ERROR << "Expected BFCP " << bfcp_prim_name(expectedPrimitive) 
                << " but get " << bfcp_prim_name(msg.primitive());
    }
    user->callNextSendMessageAction();
  }
}



} // namespace bfcp