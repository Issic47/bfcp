#ifndef BFCP_CONFERENCE_H
#define BFCP_CONFERENCE_H

#include <map>
#include <list>
#include <unordered_map>

#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>

#include <muduo/net/EventLoop.h>

#include <bfcp/common/bfcp_param.h>
#include <bfcp/common/bfcp_callbacks.h>
#include <bfcp/server/conference_define.h>

namespace tinyxml2
{
class XMLDocument;
class XMLNode;
}

namespace bfcp
{
class BfcpMsg;
class BfcpConnection;
class User;
class Floor;
class FloorRequestNode;
typedef boost::shared_ptr<BfcpConnection> BfcpConnectionPtr;
typedef boost::shared_ptr<User> UserPtr;
typedef boost::shared_ptr<Floor> FloorPtr;
typedef boost::shared_ptr<FloorRequestNode> FloorRequestNodePtr;

typedef std::map<uint16_t, UserPtr> UserDict;

class Conference
{
public:
  typedef boost::function<
    void (uint32_t, uint16_t)
  > FloorRequestExpiredCallback;
  
  typedef boost::function<
    void (uint32_t, bfcp_prim, uint16_t, ResponseError, const BfcpMsgPtr&)
  > ClientResponseCallback;

public:
  Conference(
    muduo::net::EventLoop *loop,
    const BfcpConnectionPtr &connection,
    uint32_t conferenceID,
    const ConferenceConfig &config);

  ~Conference();

  uint32_t getConferenceID() const { return conferenceID_; }

  // methods for control
  ControlError set(const ConferenceConfig &config);
  ControlError setMaxFloorRequest(uint16_t maxFloorRequest);
  ControlError setAcceptPolicy(AcceptPolicy policy, double timeForChairAction);

  ControlError addUser(const UserInfoParam &user);
  ControlError removeUser(uint16_t userID);
  
  ControlError addFloor(uint16_t floorID, const FloorConfig &config);
  ControlError removeFloor(uint16_t floorID);
  ControlError modifyFloor(uint16_t floorID, const FloorConfig &config);

  ControlError setChair(uint16_t floorID, uint16_t userID);
  ControlError removeChair(uint16_t floorID);

  string getConferenceInfo() const;
  
  // onTimeoutForChairAction should be called in cb.
  void setChairActionTimeoutCallback(const FloorRequestExpiredCallback &cb)
  { chairActionTimeoutCallback_ = cb; }
  void setChairActionTimeoutCallback(FloorRequestExpiredCallback &&cb)
  { chairActionTimeoutCallback_ = std::move(cb); }

  // onTimeoutForHoldingFloors should be called in cb.
  void setHoldingTimeoutCallback(const FloorRequestExpiredCallback &cb)
  { holdingTimeoutCallback_ = cb; }
  void setHoldingTimeoutCallback(FloorRequestExpiredCallback &&cb)
  { holdingTimeoutCallback_ = std::move(cb); }

  // onResponse should be callback in cb.
  void setClientReponseCallback(const ClientResponseCallback &cb)
  { clientReponseCallback_ = cb; }
  void setClientReponseCallback(ClientResponseCallback &&cb)
  { clientReponseCallback_ = std::move(cb); }

  void onNewRequest(const BfcpMsgPtr &msg);
  void onResponse(
    bfcp_prim expectedPrimitive,
    uint16_t userID,
    ResponseError err, 
    const BfcpMsgPtr &msg);
  void onTimeoutForChairAction(uint16_t floorRequestID);
  void onTimeoutForHoldingFloors(uint16_t floorRequestID);

private:
  typedef std::list<FloorRequestNodePtr> FloorRequestQueue;

  void initRequestHandlers();
  void handleFloorRequest(const BfcpMsgPtr &msg);
  void handleFloorRelease(const BfcpMsgPtr &msg);
  void handleFloorRequestQuery(const BfcpMsgPtr &msg);
  void handleUserQuery(const BfcpMsgPtr &msg);
  void handleFloorQuery(const BfcpMsgPtr &msg);
  void handleChairAction(const BfcpMsgPtr &msg);
  void handleHello(const BfcpMsgPtr &msg); 
  void handleGoodbye(const BfcpMsgPtr &msg);

  UserPtr findUser(uint16_t userID);
  FloorPtr findFloor(uint16_t floorID);

  bool isChair(uint16_t userID) const;
  bool checkUserID(const BfcpMsgPtr &msg, uint16_t userID);
  bool checkFloorID(const BfcpMsgPtr &msg, uint16_t floorID);
  bool checkFloorIDs(const BfcpMsgPtr &msg, const bfcp_floor_id_list &floorIDs);
  bool checkFloorChair(const BfcpMsgPtr &msg, uint16_t floorID, uint16_t userID);
  bool checkUnknownAttrs(const BfcpMsgPtr &msg);

  bool checkFloorRequestInfo(
    const BfcpMsgPtr &msg,
    const bfcp_floor_request_info &info);
  bool checkFloorsInFloorRequest(
    const BfcpMsgPtr &msg, 
    FloorRequestNodePtr &floorRequest, 
    const bfcp_floor_request_status_list &fRS);

  FloorRequestNodePtr checkFloorRequestInPendingQueue(
    const BfcpMsgPtr &msg, uint16_t floorRequestID);
  FloorRequestNodePtr checkFloorRequestInGrantedQueue(
    const BfcpMsgPtr &msg, uint16_t floorRequestID);

  void notifyFloorAndRequestInfo(const FloorRequestNodePtr &floorRequest);
  void notifyWithFloorRequestStatus(const FloorRequestNodePtr &floorRequest);
  void notifyWithFloorStatus(uint16_t floorID);
  void notifyWithFloorStatus(uint16_t userID, uint16_t floorID);

  void replyWithError(const BfcpMsgPtr &msg, bfcp_err err, const char *errInfo);
  void replyWithFloorStatus(const BfcpMsgPtr &msg, const uint16_t *floorID);
  void replyWithFloorRequestStatus(
    const BfcpMsgPtr &msg, 
    FloorRequestNodePtr &floorRequest);

  void insertFloorRequestToPendingQueue(FloorRequestNodePtr &floorRequest);
  void insertFloorRequestToAcceptedQueue(FloorRequestNodePtr &floorRequest);
  void insertFloorRequestToGrantedQueue(FloorRequestNodePtr &floorRequest);
  void insertFloorRequestToQueue(
    FloorRequestQueue &queue, FloorRequestNodePtr &floorRequest);

  FloorRequestNodePtr findFloorRequest(FloorRequestQueue &queue, uint16_t floorRequestID);
  FloorRequestNodePtr removeFloorRequest(uint16_t floorRequestID, uint16_t userID);
  FloorRequestNodePtr extractFloorRequestFromQueue(
    FloorRequestQueue &queue, uint16_t floorRequestID, uint16_t userID);
  bool revokeFloorsFromFloorRequest(FloorRequestNodePtr &floorRequest);

  void updateQueuePosition(FloorRequestQueue &queue);

  void cancelFloorRequestsFromPendingByFloorID(uint16_t floorID);
  void cancelFloorRequestsFromAcceptedByFloorID(uint16_t floorID);
  void releaseFloorRequestsFromGrantedByFloorID(uint16_t floorID);
  
  void cancelFloorRequestsFromPendingByUserID(uint16_t userID);
  void cancelFloorRequestsFromAcceptedByUserID(uint16_t userID);
  void releaseFloorRequestsFromGrantedByUserID(uint16_t userID);

  void acceptFloorRequest(
    const BfcpMsgPtr &msg,
    const bfcp_floor_request_info &info);
  void denyFloorRequest(
    const BfcpMsgPtr &msg,
    const bfcp_floor_request_info &info);
  void revokeFloorRequest(
    const BfcpMsgPtr &msg,
    const bfcp_floor_request_info &info);

  bool tryToAcceptFloorRequestsWithFloor(FloorPtr &floor);
  bool tryToGrantFloorRequestsWithAllFloors();
  bool tryToGrantFloorRequestsWithFloor(FloorPtr &floor);
  bool tryToGrantFloor(FloorRequestNodePtr &floorRequest, FloorPtr &floor);
  bool tryToGrantFloorRequestWithAllFloors(FloorRequestNodePtr &floorRequest);

  bool parseFloorRequestParam(FloorRequestParam &param, const BfcpMsgPtr &msg);

  FloorStatusParam getFloorStatusParam(uint16_t floorID) const;

  void getFloorRequestInfoParamsByUserID(
    FloorRequestInfoParamList &frqInfoList, 
    uint16_t userID,
    const FloorRequestQueue &queue) const;

  void getFloorRequestInfoParamsByFloorID(
    FloorRequestInfoParamList &frqInfoList,
    uint16_t floorID,
    const FloorRequestQueue &queue) const;

  void addUserInfoToXMLNode(
    tinyxml2::XMLDocument *doc,
    tinyxml2::XMLNode *node) const;

  void addFloorInfoToXMLNode(
    tinyxml2::XMLDocument *doc,
    tinyxml2::XMLNode *node) const;

  void addQueueInfoToXMLNode(
    tinyxml2::XMLDocument *doc,
    tinyxml2::XMLNode *node, 
    const FloorRequestQueue &queue, 
    const string &queueName) const;

  void addFloorRequestInfoToXMLNode(
    tinyxml2::XMLDocument *doc,
    tinyxml2::XMLNode *node,
    const FloorRequestNodePtr &floorRequest) const;

  bool isUserAvailable(const UserPtr &user) const;

  void setFloorRequestExpired(
    FloorRequestNodePtr &floorRequest, 
    double expiredTime,
    const FloorRequestExpiredCallback &cb);

private:
  typedef boost::function<void (const BfcpMsgPtr&)> Handler;
  typedef std::unordered_map<int, Handler> HandlerDict;

  muduo::net::EventLoop *loop_;
  BfcpConnectionPtr connection_;
  uint32_t conferenceID_;
  uint16_t nextFloorRequestID_;
  uint16_t maxFloorRequest_;
  double timeForChairAction_;
  AcceptPolicy acceptPolicy_;

  FloorRequestQueue pending_;
  FloorRequestQueue accepted_;
  FloorRequestQueue granted_;

  UserDict users_;
  std::map<uint16_t, FloorPtr> floors_;
  HandlerDict requestHandler_;

  FloorRequestExpiredCallback chairActionTimeoutCallback_;
  FloorRequestExpiredCallback holdingTimeoutCallback_;
  ClientResponseCallback clientReponseCallback_;

  double userObsoletedTime_;
};

} // namespace bfcp

#endif // BFCP_CONFERENCE_H