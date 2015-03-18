#ifndef BFCP_CONFERENCE_DEFINE_H
#define BFCP_CONFERENCE_DEFINE_H

namespace bfcp
{

enum class ControlError
{
  kNoError = 0,
  kUserNotExist,
  kUserAlreadyExist,
  kFloorNotExist,
  kFloorAlreadyExist,
  kChairNotExist,
  kChairAlreadyExist,
  kConferenceNotExist,
  kConferenceAlreadyExist,
};

enum class AcceptPolicy
{
  kAutoAccept = 0,
  kAutoDeny = 1,
};

struct ConferenceConfig
{
  uint16_t maxFloorRequest;
  AcceptPolicy acceptPolicy;
  double timeForChairAction;
};

const char* toString(AcceptPolicy policy);

} // namespace bfcp

#endif // BFCP_CONFERENCE_DEFINE_H