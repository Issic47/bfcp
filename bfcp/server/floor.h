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

  Floor(uint16_t floorID)
      : floorID_(floorID), 
        maxGrantedCount_(1),
        currentGrantedCount_(0),
        chairID_(0),
        isAssignedToChair_(false)
  {}

  ~Floor() {}

  uint16_t getFloorID() const { return floorID_; }
  
  void addFloorQueryUser(uint16_t userID) { floorQueryUsers_.insert(userID); }
  void removeFloorQueryUser(uint16_t userID) { floorQueryUsers_.erase(userID); }
  const QueryUserSet& getFloorQueryUsers() const { return floorQueryUsers_; }

  bool isAssinedToChair() const { return isAssignedToChair_; }
  uint16_t getChairID() const { return chairID_; }
  void unassigned() { isAssignedToChair_ = false; }
  void assignedToChair(uint16_t chairID) 
  { 
    isAssignedToChair_ = true;
    chairID_ = chairID;
  }

  void setMaxGrantedCount(uint16_t maxGrantedCount)
  { maxGrantedCount_ = maxGrantedCount; }
  uint16_t getMaxGrantedCount() const { return maxGrantedCount_; }

  uint16_t getGrantedCount() const { return currentGrantedCount_; }
  bool increaseGrantedCount() 
  {
    if (currentGrantedCount_ + 1 > maxGrantedCount_)
    {
      return false;
    }
    ++currentGrantedCount_;
    return true;
  }

  void decreaseGrantedCount()
  {
    --currentGrantedCount_;
    if (currentGrantedCount_ < 0)
    {
      currentGrantedCount_ = 0;
    }
  }

private:
  QueryUserSet floorQueryUsers_;
  uint16_t floorID_;
  uint16_t maxGrantedCount_;
  uint16_t currentGrantedCount_;
  uint16_t chairID_;
  bool isAssignedToChair_;
};

typedef boost::shared_ptr<Floor> FloorPtr;

} // namespace


#endif // BFCP_FLOOR_H