#ifndef BFCP_PARAM_H
#define BFCP_PARAM_H

#include <muduo/base/StringPiece.h>

#include <bfcp/common/bfcp_ex.h>

namespace bfcp
{
namespace detail
{
inline void setString(string &str, const char *cstr)
{
  if (cstr)
    str = cstr;
  else
    str.clear();
}
} // namespace detail

class UserQueryParam
{
public:
  UserQueryParam(): hasBeneficiaryID(false) {}

  uint16_t beneficiaryID;
  bool hasBeneficiaryID;
};

class FloorRequestParam
{
public:
  FloorRequestParam(): 
    hasBeneficiaryID(false),
    priority(BFCP_PRIO_NORMAL)
  {}

  FloorRequestParam(FloorRequestParam &&param)
      : floorIDs(std::move(param.floorIDs)),
        beneficiaryID(param.beneficiaryID),
        pInfo(std::move(param.pInfo)),
        priority(param.priority)
  {}

  bfcp_floor_id_list floorIDs;
  uint16_t beneficiaryID;
  string pInfo;
  bfcp_priority priority;
  bool hasBeneficiaryID;
};

class UserInfoParam 
{
public:
  UserInfoParam() {}
  UserInfoParam(UserInfoParam &&param)
    : id(param.id),
    username(std::move(param.username)),
    useruri(std::move(param.useruri))
  {}

  void set(const bfcp_user_info &info);

  uint16_t id;
  string username;
  string useruri;
};

class OverallRequestStatusParam
{
public:
  OverallRequestStatusParam(): hasRequestStatus(false) {}
  OverallRequestStatusParam(OverallRequestStatusParam &&param)
      : floorRequestID(param.floorRequestID),
        requestStatus(param.requestStatus),
        statusInfo(std::move(param.statusInfo)),
        hasRequestStatus(param.hasRequestStatus)
  {}

  void set(const bfcp_overall_request_status &oRS);

  uint16_t floorRequestID;
  bfcp_reqstatus_t requestStatus;
  string statusInfo;
  bool hasRequestStatus;
};

class FloorRequestStatusParam
{
public:
  FloorRequestStatusParam(): hasRequestStatus(false) {}
  FloorRequestStatusParam(FloorRequestStatusParam &&param)
      : floorID(param.floorID),
        requestStatus(param.requestStatus),
        statusInfo(std::move(param.statusInfo)),
        hasRequestStatus(param.hasRequestStatus)

  {}

  void set(const bfcp_floor_request_status &frqStatus);

  uint16_t floorID;
  bfcp_reqstatus_t requestStatus;
  string statusInfo;
  bool hasRequestStatus;
};

typedef std::vector<FloorRequestStatusParam> FloorRequestStatusParamList;

class FloorRequestInfoParam
{
public:
  FloorRequestInfoParam()
      : valueType(kHasNone)
  {}

  FloorRequestInfoParam(FloorRequestInfoParam &&param)
      : floorRequestID(param.floorRequestID),
        valueType(param.valueType),
        oRS(std::move(param.oRS)),
        fRS(std::move(param.fRS)),
        beneficiary(std::move(param.beneficiary)),
        requestedBy(std::move(param.requestedBy)),
        priority(param.priority),
        partPriovidedInfo(std::move(param.partPriovidedInfo))
  {}

  void set(const bfcp_floor_request_info &info);

  uint16_t floorRequestID;
  uint32_t valueType;
  OverallRequestStatusParam oRS;
  FloorRequestStatusParamList fRS;
  UserInfoParam beneficiary;
  UserInfoParam requestedBy;
  bfcp_priority priority;
  string partPriovidedInfo;
};

typedef std::vector<FloorRequestInfoParam> FloorRequestInfoParamList;

class UserStatusParam
{
public:
  UserStatusParam() : hasBeneficiary(false) {}
  UserStatusParam(UserStatusParam &&param)
      : beneficiary(std::move(param.beneficiary)),
        frqInfoList(std::move(param.frqInfoList)),
        hasBeneficiary(param.hasBeneficiary)
  {}

  void setBeneficiary(const bfcp_user_info &info)
  {
    hasBeneficiary = true;
    beneficiary.set(info);
  }

  void addFloorRequestInfo(const bfcp_floor_request_info &info)
  {
    FloorRequestInfoParam param;
    param.set(info);
    frqInfoList.push_back(std::move(param));
  }

  UserInfoParam beneficiary;
  FloorRequestInfoParamList frqInfoList;
  bool hasBeneficiary;
};

class FloorStatusParam
{
public:
  FloorStatusParam() : hasFloorID(false) {}
  FloorStatusParam(FloorStatusParam &&param)
      : floorID(param.floorID),
        hasFloorID(param.hasFloorID),
        frqInfoList(std::move(param.frqInfoList))
  {}

  void addFloorRequestInfo(const bfcp_floor_request_info &info)
  {
    FloorRequestInfoParam param;
    param.set(info);
    frqInfoList.push_back(std::move(param));
  }

  void setFloorID(uint16_t fID)
  {
    floorID = fID;
    hasFloorID = true;
  }

  uint16_t floorID;
  bool hasFloorID;
  FloorRequestInfoParamList frqInfoList;
};

class HelloAckParam
{
public:
  HelloAckParam() {}
  HelloAckParam(HelloAckParam &&param)
      : primitives(std::move(param.primitives)),
        attributes(std::move(param.attributes))
  {}

  std::vector<bfcp_prim> primitives;
  std::vector<bfcp_attrib> attributes;
};

class ErrorCodeParam
{
public:
  ErrorCodeParam() {}
  ErrorCodeParam(ErrorCodeParam &&param)
      : code(param.code),
        details(std::move(param.details))
  {}

  void set(const bfcp_errcode_t &errcode);

  bfcp_err code;
  std::vector<uint8_t> details;
};

class ErrorParam
{
public:
  ErrorParam() {}
  ErrorParam(ErrorParam &&param)
      : errorCode(std::move(param.errorCode)),
        errorInfo(std::move(param.errorInfo))
  {}
  
  void setErrorCode(const bfcp_errcode_t &errcode)
  { errorCode.set(errcode); }

  void setErrorInfo(const char *errInfo)
  { detail::setString(errorInfo, errInfo); }

  ErrorCodeParam errorCode;
  string errorInfo;
};


} // namespace bfcp

#endif // BFCP_PARAM_H