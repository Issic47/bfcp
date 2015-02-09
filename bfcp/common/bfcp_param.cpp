#include <bfcp/common/bfcp_param.h>

namespace bfcp
{

using detail::setString;

void UserInfoParam::set( const bfcp_user_info &info )
{
  id = info.id;
  setString(username, info.username);
  setString(useruri, info.useruri);
}

void OverallRequestStatusParam::set( const bfcp_overall_request_status &oRS )
{
  floorRequestID = oRS.floorRequestID;
  if (oRS.requestStatus)
  {
    hasRequestStatus = true;
    requestStatus = *oRS.requestStatus;
  }
  else
  {
    hasRequestStatus = false;
  }
  setString(statusInfo, oRS.statusInfo);
}

void FloorRequestStatusParam::set( const bfcp_floor_request_status &frqStatus )
{
  floorID = frqStatus.floorID;
  if (frqStatus.requestStatus)
  {
    hasRequestStatus = true;
    requestStatus = *frqStatus.requestStatus;
  }
  else
  {
    hasRequestStatus = false;
  }
  setString(statusInfo, frqStatus.statusInfo);
}

void FloorRequestInfoParam::set( const bfcp_floor_request_info &info )
{
  floorRequestID = info.floorRequestID;
  valueType = info.valueType;
  if (valueType & kHasOverallRequestStatus)
  {
    oRS.set(info.oRS);
  }

  for (auto &status : info.fRS)
  {
    FloorRequestStatusParam floorRequestStatus;
    floorRequestStatus.set(status);
    fRS.push_back(std::move(floorRequestStatus));
  }

  if (valueType & kHasBeneficiaryInfo)
  {
    beneficiary.set(info.beneficiary);
  }

  if (valueType & kHasRequestedByInfo)
  {
    requestedBy.set(info.requestedBy);
  }
  priority = info.priority ? *info.priority : BFCP_PRIO_NORMAL;
  setString(partPriovidedInfo, info.partPriovidedInfo);
}

void ErrorCodeParam::set( const bfcp_errcode_t &errcode )
{
  code = errcode.code;
  details.insert(details.end(), errcode.details, errcode.details + errcode.len);
}

} // namespace