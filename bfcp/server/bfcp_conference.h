#ifndef BFCP_CONFERENCE_H
#define BFCP_CONFERENCE_H

#include <bfcp/common/bfcp_param.h>
#include <bfcp/server/bfcp_user.h>

namespace bfcp
{

typedef struct FloorRequest
{
  uint16_t floorRequestID;
  uint16_t userID;
  FloorRequestParam param;
  string chairProvidedInfo;
  std::vector<uint16_t> floorRequestQuery;
  

} FloorRequest;

class BfcpConference
{
public:
  BfcpConference();
  ~BfcpConference();

  uint32_t getConferenceID() const { return conferenceID_; }

  
  void addUser(uint16_t userID, const string &userURI, const string &displayName);
  bool containsUser(uint16_t userID);
  int removeUser(uint16_t userID);
  BfcpUser* getUser(uint16_t userID);
  const BfcpUser* getUser(uint16_t userID) const;





private:
  uint32_t conferenceID_;
  // FIXME: user atomic
  uint16_t nextFloorRequestID_;
  
};

} // namespace bfcp

#endif // BFCP_CONFERENCE_H