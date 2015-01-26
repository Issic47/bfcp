#ifndef BFCP_FLOOR_REQUEST_NODE_H
#define BFCP_FLOOR_REQUEST_NODE_H

#include <set>
#include <vector>
#include <bfcp/common/bfcp_param.h>

namespace bfcp
{

class FloorRequestNode
{
public:
  typedef std::set<uint16_t> QueryUserSet;
  
  enum FloorState
  {
    kWaiting = 0,
    kAccepted = 1,
    kGranted = 2,
  };

  typedef struct FloorNode
  {
    uint16_t floorID;
    FloorState state;
    string chairInfo;
  } FloorNode;

public:



private:
  uint16_t floorRequestID_;
  uint16_t userID_;
  bool hasBeneficiary_;
  uint16_t beneficiaryID_;
  bfcp_priority priority_;
  int queuePosition_;
  string participantInfo_;
  string chairInfo_;
  QueryUserSet floorRequestQueryUsers_;
  std::vector<FloorNode> floors_;
};

}

#endif // BFCP_FLOOR_REQUEST_H