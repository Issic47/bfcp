#include <bfcp/server/floor_request_node.h>

#include <algorithm>
#include <bfcp/server/user.h>

namespace bfcp
{

namespace detail
{

class FloorNodeCmp
{
public:
  bool operator()(const FloorNode &lhs, uint16_t floorID)
  {
    return lhs.getFloorID() < floorID;
  }
};

} // namespace detail

FloorRequestNode::FloorRequestNode(uint16_t floorRequestID, 
                                   uint16_t userID, 
                                   const FloorRequestParam &param)
    : floorRequestID_(floorRequestID),
      userID_(userID),
      hasBeneficiary_(param.hasBeneficiaryID),
      beneficiaryID_(param.beneficiaryID),
      priority_(param.priority),
      participantInfo_(param.pInfo)
{
  requestStatus_.status = BFCP_PENDING;
  requestStatus_.qpos = 0;

  setFloors(param.floorIDs); 
}

void FloorRequestNode::setFloors( const bfcp_floor_id_list &floorIDs )
{
  bfcp_floor_id_list uniqueFloorIDs = floorIDs;
  std::sort(uniqueFloorIDs.begin(), uniqueFloorIDs.end());
  uniqueFloorIDs.erase(
    std::unique(uniqueFloorIDs.begin(), uniqueFloorIDs.end()), 
    uniqueFloorIDs.end());
  
  floors_.reserve(uniqueFloorIDs.size());
  floors_.clear();
  for (auto floorID : uniqueFloorIDs)
  {
    floors_.emplace_back(floorID);
  }
}

FloorNode* FloorRequestNode::findFloor( uint16_t floorID )
{
  auto it = std::lower_bound(
    floors_.begin(), floors_.end(), floorID, detail::FloorNodeCmp());
  if (it != floors_.end() && (*it).getFloorID() == floorID)
  {
    return &(*it);
  }
  return nullptr;
}

const FloorNode* FloorRequestNode::findFloor( uint16_t floorID ) const
{
  auto it = std::lower_bound(
    floors_.cbegin(), floors_.cend(), floorID, detail::FloorNodeCmp());
  if (it != floors_.cend() && (*it).getFloorID() == floorID)
  {
    return &(*it);
  }
  return nullptr;
}

bool FloorRequestNode::isAllFloorStatus( bfcp_reqstat status ) const
{
  for (auto &floor : floors_)
  {
    if (floor.getStatus() != status)
      return false;
  }
  return true;
}

bool FloorRequestNode::isAnyFloorStatus( bfcp_reqstat status ) const
{
  for (auto &floor : floors_)
  {
    if (floor.getStatus() == status)
      return true;
  }
  return false;
}

void FloorRequestNode::addQueryUser( uint16_t userID )
{
  queryUsers_.insert(userID);
}

void FloorRequestNode::removeQueryUser( uint16_t userID )
{
  queryUsers_.erase(userID);
}

FloorRequestInfoParam 
FloorRequestNode::toFloorRequestInfoParam(const UserDict &users) const
{
  FloorRequestInfoParam param;
  param.floorRequestID = floorRequestID_;
  param.priority = priority_;
  param.partPriovidedInfo = participantInfo_;

  if (hasBeneficiary_)
  {
    param.valueType |= kHasBeneficiaryInfo;
    param.beneficiary.id = beneficiaryID_;
    auto it = users.find(beneficiaryID_);
    if (it != users.end())
    {
      param.beneficiary = (*it).second->toUserInfoParam();
    }
  }

  {
    param.valueType |= kHasRequestedByInfo;
    param.requestedBy.id = userID_;
    auto it = users.find(userID_);
    if (it != users.end())
    {
      param.requestedBy = (*it).second->toUserInfoParam();
    }
  }

  {
    FloorRequestStatusParam floorStatusParam;
    for (auto &floorNode : floors_)
    {
      param.fRS.emplace_back(floorNode.toFloorRequestStatusParam());
    }
  }

  {
    param.valueType |= kHasOverallRequestStatus;
    param.oRS.floorRequestID = floorRequestID_;
    param.oRS.hasRequestStatus = true;
    param.oRS.requestStatus = requestStatus_;
    param.oRS.statusInfo = statusInfo_;
  }
  return param;
}



} // namespace bfcp