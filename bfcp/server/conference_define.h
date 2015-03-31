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
  double timeForChairAction; // when < 0.0, unlimited
  double userObsoletedTime; // when < 0.0, unlimited
};

struct FloorConfig
{
  uint16_t maxGrantedNum; // when == 0, unlimited
  double maxHoldingTime;  // when < 0.0, unlimited
};

const char* toString(AcceptPolicy policy);

} // namespace bfcp

#endif // BFCP_CONFERENCE_DEFINE_H