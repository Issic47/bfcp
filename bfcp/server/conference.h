#ifndef BFCP_CONFERENCE_H
#define BFCP_CONFERENCE_H

#include <bfcp/common/bfcp_param.h>
#include <bfcp/server/user.h>

namespace bfcp
{

class Conference
{
public:
  Conference();
  ~Conference();

  uint32_t getConferenceID() const { return conferenceID_; }

  void addUser(uint16_t userID, const string &userURI, const string &displayName);
  bool containsUser(uint16_t userID);
  int removeUser(uint16_t userID);
  User* getUser(uint16_t userID);
  const User* getUser(uint16_t userID) const;

private:
  uint32_t conferenceID_;
  // FIXME: user atomic
  uint16_t nextFloorRequestID_;
  
};

} // namespace bfcp

#endif // BFCP_CONFERENCE_H