#ifndef BFCP_PARAM_H
#define BFCP_PARAM_H

#include <list>
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

  void setBeneficiaryID(uint16_t bID)
  {
    beneficiaryID = bID;
    hasBeneficiaryID = true;
  }

  uint16_t beneficiaryID;
  bool hasBeneficiaryID;
};

class FloorRequestParam
{
public:
  FloorRequestParam(): 
    priority(BFCP_PRIO_NORMAL),
    hasBeneficiaryID(false)
  {}

  FloorRequestParam(const FloorRequestParam &param)
    : floorIDs(param.floorIDs),
    beneficiaryID(param.beneficiaryID),
    pInfo(param.pInfo),
    priority(param.priority),
    hasBeneficiaryID(param.hasBeneficiaryID)
  {}

  FloorRequestParam(FloorRequestParam &&param)
      : floorIDs(std::move(param.floorIDs)),
        beneficiaryID(param.beneficiaryID),
        pInfo(std::move(param.pInfo)),
        priority(param.priority),
        hasBeneficiaryID(param.hasBeneficiaryID)
  {}

  FloorRequestParam& operator=(const FloorRequestParam &param)
  {
    floorIDs = param.floorIDs;
    beneficiaryID = param.beneficiaryID;
    pInfo = param.pInfo;
    priority = param.priority;
    hasBeneficiaryID = param.hasBeneficiaryID;
    return *this;
  }

  void setBeneficiaryID(uint16_t bID)
  {
    beneficiaryID = bID;
    hasBeneficiaryID = true;
  }

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

  UserInfoParam(const UserInfoParam &param)
    : id(param.id),
    username(param.username),
    useruri(param.useruri)
  {}

  UserInfoParam(UserInfoParam &&param)
    : id(param.id),
    username(std::move(param.username)),
    useruri(std::move(param.useruri))
  {}
  
  UserInfoParam& operator=(const UserInfoParam &param)
  {
    id = param.id;
    username = param.username;
    useruri = param.useruri;
    return *this;
  }

  void set(const bfcp_user_info &info);

  uint16_t id;
  string username;
  string useruri;
};

class OverallRequestStatusParam
{
public:
  OverallRequestStatusParam(): hasRequestStatus(false) {}

  OverallRequestStatusParam(const OverallRequestStatusParam &param)
    : floorRequestID(param.floorRequestID),
    requestStatus(param.requestStatus),
    statusInfo(param.statusInfo),
    hasRequestStatus(param.hasRequestStatus)
  {}

  OverallRequestStatusParam(OverallRequestStatusParam &&param)
      : floorRequestID(param.floorRequestID),
        requestStatus(param.requestStatus),
        statusInfo(std::move(param.statusInfo)),
        hasRequestStatus(param.hasRequestStatus)
  {}

  OverallRequestStatusParam& operator=(const OverallRequestStatusParam &param)
  {
    floorRequestID = param.floorRequestID;
    requestStatus = param.requestStatus;
    statusInfo = param.statusInfo;
    hasRequestStatus = param.hasRequestStatus;
    return *this;
  }

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

  FloorRequestStatusParam(const FloorRequestStatusParam &param)
    : floorID(param.floorID),
    requestStatus(param.requestStatus),
    statusInfo(param.statusInfo),
    hasRequestStatus(param.hasRequestStatus)
  {}

  FloorRequestStatusParam(FloorRequestStatusParam &&param)
      : floorID(param.floorID),
        requestStatus(param.requestStatus),
        statusInfo(std::move(param.statusInfo)),
        hasRequestStatus(param.hasRequestStatus)
  {}

  FloorRequestStatusParam& operator=(const FloorRequestStatusParam &param)
  {
    floorID = param.floorID;
    requestStatus = param.requestStatus;
    statusInfo = param.statusInfo;
    hasRequestStatus = param.hasRequestStatus;
    return *this;
  }

  void set(const bfcp_floor_request_status &frqStatus);

  uint16_t floorID;
  bfcp_reqstatus_t requestStatus;
  string statusInfo;
  bool hasRequestStatus;
};

typedef std::list<FloorRequestStatusParam> FloorRequestStatusParamList;

class FloorRequestInfoParam
{
public:
  FloorRequestInfoParam()
      : valueType(kHasNone)
  {}

  FloorRequestInfoParam(const FloorRequestInfoParam &param)
    : floorRequestID(param.floorRequestID),
    valueType(param.valueType),
    oRS(param.oRS),
    fRS(param.fRS),
    beneficiary(param.beneficiary),
    requestedBy(param.requestedBy),
    priority(param.priority),
    partPriovidedInfo(param.partPriovidedInfo)
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

  FloorRequestInfoParam& operator=(const FloorRequestInfoParam &param)
  {
    floorRequestID = param.floorRequestID;
    valueType = param.valueType;
    oRS = param.oRS;
    fRS = param.fRS;
    beneficiary = param.beneficiary;
    requestedBy = param.requestedBy;
    priority = param.priority;
    partPriovidedInfo = param.partPriovidedInfo;
    return *this;
  }

  void set(const bfcp_floor_request_info &info);

  uint16_t floorRequestID;
  uint32_t valueType; // AttrValueType flags
  OverallRequestStatusParam oRS;
  FloorRequestStatusParamList fRS;
  UserInfoParam beneficiary;
  UserInfoParam requestedBy;
  bfcp_priority priority;
  string partPriovidedInfo;
};

typedef std::list<FloorRequestInfoParam> FloorRequestInfoParamList;

class UserStatusParam
{
public:
  UserStatusParam() : hasBeneficiary(false) {}

  UserStatusParam(const UserStatusParam &param)
      : beneficiary(param.beneficiary),
        frqInfoList(param.frqInfoList),
        hasBeneficiary(param.hasBeneficiary)
  {}

  UserStatusParam(UserStatusParam &&param)
      : beneficiary(std::move(param.beneficiary)),
        frqInfoList(std::move(param.frqInfoList)),
        hasBeneficiary(param.hasBeneficiary)
  {}

  
  UserStatusParam& operator=(const UserStatusParam &param)
  {
    beneficiary = param.beneficiary;
    frqInfoList = param.frqInfoList;
    hasBeneficiary = param.hasBeneficiary;
    return *this;
  }

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

  FloorStatusParam(const FloorStatusParam &param)
    : floorID(param.floorID),
    hasFloorID(param.hasFloorID),
    frqInfoList(param.frqInfoList)
  {}

  FloorStatusParam(FloorStatusParam &&param)
      : floorID(param.floorID),
        hasFloorID(param.hasFloorID),
        frqInfoList(std::move(param.frqInfoList))
  {}

  FloorStatusParam& operator=(const FloorStatusParam &param)
  {
    floorID = param.floorID;
    hasFloorID = param.hasFloorID;
    frqInfoList = param.frqInfoList;
    return *this;
  }

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

  HelloAckParam(const HelloAckParam &param)
    : primitives(param.primitives),
    attributes(param.attributes)
  {}

  HelloAckParam(HelloAckParam &&param)
      : primitives(std::move(param.primitives)),
        attributes(std::move(param.attributes))
  {}

  HelloAckParam& operator=(const HelloAckParam &param)
  {
    primitives = param.primitives;
    attributes = param.attributes;
    return *this;
  }

  std::vector<bfcp_prim> primitives;
  std::vector<bfcp_attrib> attributes;
};

class ErrorCodeParam
{
public:
  ErrorCodeParam() {}

  ErrorCodeParam(const ErrorCodeParam &param)
      : code(param.code),
        details(param.details)
  {}

  ErrorCodeParam(ErrorCodeParam &&param)
      : code(param.code),
        details(std::move(param.details))
  {}

  ErrorCodeParam& operator=(const ErrorCodeParam &param)
  {
    code = param.code;
    details = param.details;
    return *this;
  }

  void set(const bfcp_errcode_t &errcode);

  bfcp_err code;
  std::vector<uint8_t> details;
};

class ErrorParam
{
public:
  ErrorParam() {}

  ErrorParam(const ErrorParam &param)
      : errorCode(param.errorCode),
        errorInfo(param.errorInfo)
  {}

  ErrorParam(ErrorParam &&param)
      : errorCode(std::move(param.errorCode)),
        errorInfo(std::move(param.errorInfo))
  {}

  ErrorParam& operator=(const ErrorParam &param)
  {
    errorCode = param.errorCode;
    errorInfo = param.errorInfo;
    return *this;
  }
  
  void setErrorCode(const bfcp_errcode_t &errcode)
  { errorCode.set(errcode); }

  void setErrorInfo(const char *errInfo)
  { detail::setString(errorInfo, errInfo); }

  ErrorCodeParam errorCode;
  string errorInfo;
};


} // namespace bfcp

#endif // BFCP_PARAM_H