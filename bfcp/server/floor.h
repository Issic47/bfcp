#ifndef BFCP_FLOOR_H
#define BFCP_FLOOR_H

#include <set>
#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <bfcp/common/bfcp_ex.h>


namespace bfcp
{

class Floor : boost::noncopyable
{
public:
  typedef std::set<uint16_t> QueryUserSet;

  Floor(uint16_t floorID, uint16_t maxGrantedCount, double maxHoldingTime)
      : floorID_(floorID), 
        maxGrantedCount_(maxGrantedCount),
        currentGrantedCount_(0),
        chairID_(0),
        maxHoldingTime_(maxHoldingTime),
        isAssigned_(false)
  {}

  ~Floor() {}

  uint16_t getFloorID() const { return floorID_; }
  
  void addQueryUser(uint16_t userID) { floorQueryUsers_.insert(userID); }
  void removeQueryUser(uint16_t userID) { floorQueryUsers_.erase(userID); }
  const QueryUserSet& getQueryUsers() const { return floorQueryUsers_; }

  bool isAssigned() const { return isAssigned_; }
  uint16_t getChairID() const { return chairID_; }
  void unassigned() { isAssigned_ = false; }
  void assignedToChair(uint16_t chairID) 
  { 
    isAssigned_ = true;
    chairID_ = chairID;
  }

  void setMaxGrantedCount(uint16_t maxGrantedCount)
  { maxGrantedCount_ = maxGrantedCount; }
  uint16_t getMaxGrantedCount() const { return maxGrantedCount_; }

  void setMaxHoldingTime(double maxHoldingTime)
  { maxHoldingTime_ = maxHoldingTime; }
  double getMaxHoldingTime() const { return maxHoldingTime_; }

  uint16_t getGrantedCount() const { return currentGrantedCount_; }
  bool isFreeToGrant() const 
  { 
    return maxGrantedCount_ == 0 || currentGrantedCount_ < maxGrantedCount_;
  }
  bool tryToGrant();
  void revoke();

private:
  QueryUserSet floorQueryUsers_;
  uint16_t floorID_;
  uint16_t maxGrantedCount_;
  uint16_t currentGrantedCount_;
  uint16_t chairID_;
  double maxHoldingTime_;
  bool isAssigned_;
};

typedef boost::shared_ptr<Floor> FloorPtr;

inline bool Floor::tryToGrant()
{
  if (maxGrantedCount_ > 0 && 
      currentGrantedCount_ + 1 > maxGrantedCount_)
  {
    return false;
  }
  ++currentGrantedCount_;
  return true;
}

inline void Floor::revoke()
{
  if (currentGrantedCount_ != 0)
  {
    --currentGrantedCount_;
  }
}

} // namespace bfcp


#endif // BFCP_FLOOR_H