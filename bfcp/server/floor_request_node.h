#ifndef BFCP_FLOOR_REQUEST_NODE_H
#define BFCP_FLOOR_REQUEST_NODE_H

#include <vector>
#include <list>
#include <set>
#include <map>

#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <muduo/net/TimerId.h>
#include <bfcp/common/bfcp_param.h>

namespace bfcp
{

class User;
typedef boost::shared_ptr<User> UserPtr;
typedef std::map<uint16_t, UserPtr> UserDict;

class FloorNode
{
public:
  FloorNode(uint16_t floorID)
    : floorID_(floorID)
  {
    requestStatus_.status = BFCP_PENDING;
    requestStatus_.qpos = 0;
  }

  uint16_t getFloorID() const { return floorID_; }

  void setStatusInfo(const char *statusInfo)
  { if (statusInfo) statusInfo_ = statusInfo; }
  const string& getStatusInfo() const { return statusInfo_; }

  void setStatus(bfcp_reqstat status) { requestStatus_.status = status; }
  bfcp_reqstat getStatus() const { return requestStatus_.status; }

  void setQueuePosition(uint8_t qpos) { requestStatus_.qpos = qpos; }
  uint8_t getQueuePosition() const { return requestStatus_.qpos; }

  FloorRequestStatusParam toFloorRequestStatusParam() const;

private:
  uint16_t floorID_;
  bfcp_reqstatus requestStatus_;
  string statusInfo_;
  // TODO: add timer for waiting chair action
};

typedef std::vector<FloorNode> FloorNodeList;

class FloorRequestNode : boost::noncopyable
{
public:
  typedef std::set<uint16_t> QueryUserSet;

public:
  FloorRequestNode(uint16_t floorRequestID,
                   uint16_t userID,
                   const FloorRequestParam &param);

  FloorNode* findFloor(uint16_t floorID);
  const FloorNode* findFloor(uint16_t floorID) const;
  bool isAllFloorStatus(bfcp_reqstat status) const;
  bool isAnyFloorStatus(bfcp_reqstat status) const;
  FloorNodeList& getFloorNodeList() { return floors_; }
  const FloorNodeList& getFloorNodeList() const { return floors_; }

  uint16_t getFloorRequestID() const { return floorRequestID_; }
  uint16_t getUserID() const { return userID_; }
  uint16_t getBeneficiaryID() const { return beneficiaryID_; }
  bool hasBeneficiary() const { return hasBeneficiary_; }

  void setOverallStatus(bfcp_reqstat status) { requestStatus_.status = status; }
  bfcp_reqstat getOverallStatus() const { return requestStatus_.status; }

  void setQueuePosition(uint8_t qpos) { requestStatus_.qpos = qpos; }
  uint8_t getQueuePosition() const { return requestStatus_.qpos; }

  void setPrioriy(bfcp_priority priority) { priority_ = priority; }
  bfcp_priority getPriority() const { return priority_; }

  const string& getParticipantInfo() const { return participantInfo_; }

  void setStatusInfo(const char *statusInfo)
  { if (statusInfo) statusInfo_ = statusInfo; }
  const string& getStatusInfo() const { return statusInfo_; }

  void addQueryUser(uint16_t userID);
  void removeQueryUser(uint16_t userID);
  const QueryUserSet& getFloorRequestQueryUsers() const 
  { return queryUsers_; }

  FloorRequestInfoParam toFloorRequestInfoParam(const UserDict &users) const;

  void setExpiredTimer(muduo::net::TimerId timerId) 
  { expiredTimer_ = timerId; }
  muduo::net::TimerId getExpiredTimer() const
  { return expiredTimer_; }

private:
  void setFloors(const bfcp_floor_id_list &floorIDs);

  uint16_t floorRequestID_;
  uint16_t userID_;
  bool hasBeneficiary_;
  uint16_t beneficiaryID_;
  bfcp_priority priority_;
  bfcp_reqstatus requestStatus_;
  int queuePosition_;
  string participantInfo_;
  string statusInfo_;
  QueryUserSet queryUsers_;
  FloorNodeList floors_;
  muduo::net::TimerId expiredTimer_; // for chair action or holding timeout
};

typedef boost::shared_ptr<FloorRequestNode> FloorRequestNodePtr;

inline FloorRequestStatusParam FloorNode::toFloorRequestStatusParam() const
{
  FloorRequestStatusParam param;
  param.floorID = floorID_;
  param.hasRequestStatus = true;
  param.requestStatus = requestStatus_;
  param.statusInfo = statusInfo_;
  return param;
}

} // namespace bfcp

#endif // BFCP_FLOOR_REQUEST_NODE_H