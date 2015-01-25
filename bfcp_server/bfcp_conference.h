#ifndef BFCP_CONFERENCE_H
#define BFCP_CONFERENCE_H

#include "common/bfcp_param.h"

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

private:
  uint32_t conferenceID_;
  
};

} // namespace bfcp

#endif // BFCP_CONFERENCE_H