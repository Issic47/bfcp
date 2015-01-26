#include <bfcp/server/user.h>

namespace bfcp
{

bool User::increaseFloorRequestCountOfAFloor(
  uint16_t floorID, uint16_t maxFloorRequestCount)
{
  auto it = floorRequestCounter_.lower_bound(floorID);
  if (it != floorRequestCounter_.end() && (*it).first == floorID)
  {
    if ((*it).second + 1 > maxFloorRequestCount)
    {
      return false;
    }
    ++(*it).second;
  }
  else
  {
    floorRequestCounter_.insert(
      it, std::pair<uint16_t, uint16_t>(floorID, 1));
  }
  return true;
}

void User::decreaseFloorRequestCountOfAFloor( uint16_t floorID )
{
  auto it = floorRequestCounter_.find(floorID);
  if (it != floorRequestCounter_.end())
  {
    (*it).second = (std::max)(0, (*it).second - 1);
  }
}

uint16_t User::getFloorRequestCountOfAFloor( uint16_t floorID ) const
{
  auto it = floorRequestCounter_.find(floorID);
  if (it != floorRequestCounter_.end())
  {
    return (*it).second;
  }
  return 0;
}

void User::clearFloorRequestCountOfAFloor( uint16_t floorID )
{
  auto it = floorRequestCounter_.find(floorID);
  if (it != floorRequestCounter_.end())
  {
    (*it).second = 0;
  }
}





} // namespace bfcp