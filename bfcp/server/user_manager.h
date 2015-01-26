#ifndef BFCP_USER_MANAGER_H
#define BFCP_USER_MANAGER_H

#include <map>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <bfcp/common/bfcp_ex.h>

namespace bfcp
{

class User;
typedef boost::shared_ptr<User> BfcpUserPtr;

class UserManager : boost::noncopyable
{
public:
  UserManager(uint16_t maxFloorRequestCountOfAFloor)
    : maxFloorRequestCountOfAFloor_(maxFloorRequestCountOfAFloor)
  {}

  void setMaxFloorRequestCount(uint16_t maxFloorRequestCountOfAFloor) 
  { maxFloorRequestCountOfAFloor_ = maxFloorRequestCountOfAFloor; }

  uint16_t getMaxFloorRequestCount() const 
  { return maxFloorRequestCountOfAFloor_; }

  BfcpUserPtr getUser(uint16_t userID);
  BfcpUserPtr addUser(uint16_t userID, const string &displayName, const string &uri);
  void removeUser(uint16_t userID) { users_.erase(userID); }

  void clearAllUserFloorRequestCountOfAFloor(uint16_t floorID);
private:
  uint16_t maxFloorRequestCountOfAFloor_;
  std::map<uint16_t, BfcpUserPtr> users_;
};

} // namespace bfcp

#endif // BFCP_USER_MANAGER_H

