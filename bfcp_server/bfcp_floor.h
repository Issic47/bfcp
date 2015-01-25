#ifndef BFCP_FLOOR_H
#define BFCP_FLOOR_H

#include "common/bfcp_ex.h"

namespace bfcp
{

class BfcpFloor
{
public:
  enum State
  {
    kWaiting = 0,
    kAccepted = 1,
    kGranted = 2,
  };

  typedef std::vector<uint16_t> FloorQueryList;

  BfcpFloor(uint16_t floorID, uint16_t maxGrantedCount)
      : floorID_(floorID), 
        chairID_(0),
        maxGrantedCount_(maxGrantedCount),
        state_(kWaiting)
  {}

  ~BfcpFloor() {}

  void setMaxGrantedCount(uint16_t maxGrantedCount)
  { maxGrantedCount_ = maxGrantedCount; }
  void setChairID(uint16_t chairID) { chairID_ = chairID;}

  uint16_t getFloorID() const { return floorID_; }
  State getState() const { return state_; }
  const FloorQueryList& getFloorQueryList() const { return floorQueryList_; }
  
private:
  uint16_t maxGrantedCount_;
  uint16_t floorID_;
  uint16_t chairID_;
  State state_;
  FloorQueryList floorQueryList_;
};

} // namespace


#endif // BFCP_FLOOR_H