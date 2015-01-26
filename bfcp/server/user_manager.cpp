#include <bfcp/server/user_manager.h>

#include <boost/make_shared.hpp>
#include <bfcp/server/user.h>

namespace bfcp
{
 
BfcpUserPtr UserManager::getUser( uint16_t userID )
{
  auto it = users_.find(userID);
  if (it != users_.end()) 
  {
    return (*it).second;
  }
  return nullptr;
}

BfcpUserPtr UserManager::addUser( uint16_t userID, const string &displayName, const string &uri )
{
  auto it = users_.find(userID);
  if (it != users_.end())
  {
    (*it).second->setDisplayName(displayName);
    (*it).second->setURI(uri);
    return (*it).second;
  }
  else
  {
    BfcpUserPtr user = 
      boost::make_shared<User>(userID, displayName, uri);
    users_.insert(std::make_pair(userID, user));
    return user;
  }
}

void UserManager::clearAllUserFloorRequestCountOfAFloor( uint16_t floorID )
{
  for (auto it = users_.begin(); it != users_.end(); ++it)
  {
    (*it).second->clearFloorRequestCountOfAFloor(floorID);
  }
}

} // namespace bfcp