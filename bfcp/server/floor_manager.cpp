#include <bfcp/server/floor_manager.h>

#include <boost/make_shared.hpp>
#include <bfcp/server/floor.h>

namespace bfcp
{

FloorPtr FloorManager::addFloor(uint16_t floorID)
{
  auto lb = floors_.lower_bound(floorID);
  if (lb != floors_.end() && (*lb).first == floorID)
  {
    return (*lb).second;
  }
  FloorPtr floor = boost::make_shared<Floor>(floorID);
  floors_.insert(lb, std::make_pair(floorID, floor));
  return floor;
}

void FloorManager::removeFloor( uint16_t floorID )
{
  floors_.erase(floorID);
}

FloorPtr FloorManager::getFloor( uint16_t floorID )
{
  auto it = floors_.find(floorID);
  if (it != floors_.end())
  {
    return (*it).second;
  }
  return nullptr;
}

bool FloorManager::isUserAChair( uint16_t userID ) const
{
  for (auto it = floors_.begin(); it != floors_.end(); ++it)
  {
    if ((*it).second->isAssinedToChair() && 
        (*it).second->getChairID() == userID)
    {
      return true;
    }
  }
  return false;
}

} // namespace bfcp