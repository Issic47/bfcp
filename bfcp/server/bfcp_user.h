#ifndef BFCP_USER_H
#define BFCP_USER_H

#include <bfcp/common/bfcp_ex.h>

namespace bfcp
{

class BfcpUser
{
public:
  typedef std::vector<uint16_t> FloorRequestList;

  BfcpUser(uint16_t userID)
      : userID_(userID)
  {}
  
  void setDisplayName(const string &displayName)
  { displayName_ = displayName; }

  void setURI(const string &uri)
  { uri_ = uri; }

  uint16_t getUserID() const { return userID_; }
  const string& getDisplayName() const { return displayName_; }
  const string& getURI() const { return uri_; }
  const FloorRequestList& getFloorRequestList() const 
  { return floorRequestList_; }

  

private:
  uint16_t userID_;
  string displayName_;
  string uri_;
  FloorRequestList floorRequestList_;
};

} // namespace bfcp

#endif // BFCP_USER_H