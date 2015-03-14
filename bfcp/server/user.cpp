#include <bfcp/server/user.h>

namespace bfcp
{

void User::addOneRequestOfFloor(uint16_t floorID)
{
  auto it = floorRequestCounter_.lower_bound(floorID);
  if (it != floorRequestCounter_.end() && (*it).first == floorID)
  {
    ++(*it).second;
  }
  else
  {
    floorRequestCounter_.insert(
      it, std::pair<uint16_t, uint16_t>(floorID, 1));
  }
}

void User::removeOneRequestOfFloor( uint16_t floorID )
{
  auto it = floorRequestCounter_.find(floorID);
  if (it != floorRequestCounter_.end())
  {
    if ((*it).second > 0)
    {
      --(*it).second;
    }
  }
}

uint16_t User::getRequestCountOfFloor( uint16_t floorID ) const
{
  auto it = floorRequestCounter_.find(floorID);
  if (it != floorRequestCounter_.end())
  {
    return (*it).second;
  }
  return 0;
}

void User::resetRequestCountOfFloor( uint16_t floorID )
{
  auto it = floorRequestCounter_.find(floorID);
  if (it != floorRequestCounter_.end())
  {
    (*it).second = 0;
  }
}

} // namespace bfcp