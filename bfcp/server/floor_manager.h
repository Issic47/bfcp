#ifndef BFCP_FLOOR_MANAGER_H
#define BFCP_FLOOR_MANAGER_H

#include <map>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

namespace bfcp
{

class Floor;
typedef boost::shared_ptr<Floor> FloorPtr;

class FloorManager : boost::noncopyable
{
public:
  FloorPtr addFloor(uint16_t floorID);
  void removeFloor(uint16_t floorID);
  FloorPtr getFloor(uint16_t floorID);

  bool isUserAChair(uint16_t userID) const;
  void removeAllFloors() { floors_.clear(); }

private:
  std::map<uint16_t, FloorPtr> floors_;
};

} // namespace bfcp

#endif // BFCP_FLOOR_MANAGER_H