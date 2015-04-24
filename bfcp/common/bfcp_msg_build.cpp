#include <bfcp/common/bfcp_msg_build.h>

#include <cassert>
#include <algorithm>

namespace bfcp
{
namespace detail
{

const size_t kHeaderSize = 12;
const size_t kHeaderWithFragSize = 16;

inline int build_attr_BENEFICIARY_ID( mbuf_t *buf, uint16_t bID )
{
  return bfcp_attrs_encode(buf, 1, BFCP_BENEFICIARY_ID | BFCP_MANDATORY, 0, &bID);
}

inline int build_attr_FLOOR_ID( mbuf_t *buf, uint16_t fID )
{
  return bfcp_attrs_encode(buf, 1, BFCP_FLOOR_ID | BFCP_MANDATORY, 0, &fID);
}

inline int build_attr_FLOOR_REQUEST_ID( mbuf_t *buf, uint16_t frqID )
{
  return bfcp_attrs_encode(buf, 1, BFCP_FLOOR_ID | BFCP_MANDATORY, 0, &frqID);
}

inline int build_attr_PRIORITY( mbuf_t *buf, bfcp_priority priority )
{
  return bfcp_attrs_encode(buf, 1, BFCP_PRIORITY | BFCP_MANDATORY, 0, &priority);
}

inline int build_attr_REQUEST_STATUS( mbuf_t *buf, const bfcp_reqstatus_t &rs )
{
  return bfcp_attrs_encode(buf, 1, BFCP_REQUEST_STATUS | BFCP_MANDATORY, 0, &rs);
}

inline int build_attr_ERROR_CODE( mbuf_t *buf, const bfcp_errcode_t &error )
{
  return bfcp_attrs_encode(buf, 1, BFCP_ERROR_CODE | BFCP_MANDATORY, 0, &error);
}

inline int build_attr_ERROR_INFO( mbuf_t *buf, const char *eInfo )
{
  return bfcp_attrs_encode(buf, 1, BFCP_ERROR_INFO, 0, eInfo);
}

inline int build_attr_PARTICIPANT_PROVIDED_INFO( mbuf_t *buf, const char *pInfo )
{
  return bfcp_attrs_encode(buf, 1, BFCP_PART_PROV_INFO, 0, pInfo);
}

inline int build_attr_STATUS_INFO( mbuf_t *buf, const char *sInfo )
{
  return bfcp_attrs_encode(buf, 1, BFCP_STATUS_INFO, 0, sInfo);
}

inline int build_attribute_SUPPORTED_ATTRIBUTES( mbuf_t *buf, const bfcp_supattr_t &attributes )
{
  return bfcp_attrs_encode(buf, 1, BFCP_SUPPORTED_ATTRS | BFCP_MANDATORY, 0, &attributes);
}

inline int build_attr_SUPPORTED_PRIMITIVES( mbuf_t *buf, const bfcp_supprim_t &primitives )
{
  return bfcp_attrs_encode(buf, 1, BFCP_SUPPORTED_PRIMS | BFCP_MANDATORY, 0, &primitives);
}

inline int build_attr_USER_DISPLAY_NAME( mbuf_t *buf, const char *username )
{
  return bfcp_attrs_encode(buf, 1, BFCP_USER_DISP_NAME, 0, username);
}

inline int bfcp_build_attr_USER_URI( mbuf_t *buf, const char *useruri )
{
  return bfcp_attrs_encode(buf, 1, BFCP_USER_URI, 0, useruri);
}

inline int build_attr_BENEFICIARY_INFORMATION( mbuf_t *buf, const UserInfoParam &beneficiary )
{
  const char *username = 
    beneficiary.username.empty() ? nullptr : beneficiary.username.c_str();
  const char *useruri = 
    beneficiary.useruri.empty() ? nullptr : beneficiary.useruri.c_str();
  return bfcp_attrs_encode(
    buf, 1, BFCP_BENEFICIARY_INFO | BFCP_MANDATORY, 2, &beneficiary.id,
    BFCP_USER_DISP_NAME, 0, username,
    BFCP_USER_URI, 0, useruri);
}

inline int build_attr_REQUESTED_BY_INFORMATION( mbuf_t *buf, const UserInfoParam &requested_by )
{
  const char *username = 
    requested_by.username.empty() ? nullptr : requested_by.username.c_str();
  const char *useruri = 
    requested_by.useruri.empty() ? nullptr : requested_by.useruri.c_str();
  return bfcp_attrs_encode(
    buf, 1, BFCP_REQUESTED_BY_INFO, 2, &requested_by.id,
    BFCP_USER_DISP_NAME, 0, username,
    BFCP_USER_URI, 0, useruri);
}

inline int build_attr_FLOOR_REQUEST_STATUS( mbuf_t *buf, const FloorRequestStatusParam &fRS )
{
  const char *statusInfo = 
    fRS.statusInfo.empty() ? nullptr : fRS.statusInfo.c_str();
  const bfcp_reqstatus_t *requestStatus = 
    fRS.hasRequestStatus ? &fRS.requestStatus : nullptr;
  return bfcp_attrs_encode(
    buf, 1, BFCP_FLOOR_REQ_STATUS | BFCP_MANDATORY, 2, &fRS.floorID,
    BFCP_REQUEST_STATUS, 0, requestStatus,
    BFCP_STATUS_INFO, 0, statusInfo);
}

inline int build_attr_OVERALL_REQUEST_STATUS( mbuf_t *buf, const OverallRequestStatusParam &oRS )
{
  const char *statusInfo = 
    oRS.statusInfo.empty() ? nullptr : oRS.statusInfo.c_str();
  const bfcp_reqstatus_t *requestStatus = 
    oRS.hasRequestStatus ? &oRS.requestStatus : nullptr;
  return bfcp_attrs_encode(
    buf, 1, BFCP_OVERALL_REQ_STATUS | BFCP_MANDATORY, 2, &oRS.floorRequestID,
    BFCP_REQUEST_STATUS, 0, requestStatus,
    BFCP_STATUS_INFO, 0, statusInfo);
}

int build_attr_FLOOR_REQUEST_INFORMATION( mbuf_t *buf, const FloorRequestInfoParam &frqInfo )
{
  size_t start = buf->pos;
  int err = 0;
  do 
  {
    err = bfcp_attrs_encode(buf, 1, BFCP_FLOOR_REQ_INFO | BFCP_MANDATORY, 0, &frqInfo.floorRequestID);
    if (err) break;

    if (frqInfo.valueType & kHasOverallRequestStatus)
    {
      err = build_attr_OVERALL_REQUEST_STATUS(buf, frqInfo.oRS);
      if (err) break;
    }

    for (auto &floorRequestStatus : frqInfo.fRS)
    {
      err = build_attr_FLOOR_REQUEST_STATUS(buf, floorRequestStatus);
      if (err) break;
    }

    if (frqInfo.valueType & kHasBeneficiaryInfo)
    {
      err = build_attr_BENEFICIARY_INFORMATION(buf, frqInfo.beneficiary);
      if (err) break;
    }

    if (frqInfo.valueType & kHasRequestedByInfo)
    {
      err = build_attr_REQUESTED_BY_INFORMATION(buf, frqInfo.requestedBy);
      if (err) break;
    }

    // NOTE: priority is optional
    err = build_attr_PRIORITY(buf, frqInfo.priority);
    if (err) break;

    if (!frqInfo.partPriovidedInfo.empty()) 
    {
      err = build_attr_PARTICIPANT_PROVIDED_INFO(buf, frqInfo.partPriovidedInfo.c_str());
      if (err) break;
    }

    err = bfcp_attr_update_len(buf, start);
    if (err) break;

  } while (false);

  return err;
}

} // namespace detail
} // namespace bfcp

using namespace bfcp::detail;

namespace bfcp
{

int build_msg_FloorRequest(mbuf_t *buf, 
                           uint8_t version, 
                           const bfcp_entity &entity, 
                           const FloorRequestParam &floorRequest)
{
  assert(buf);

  size_t start = buf->pos;
  int err = 0;
  do 
  {
    err = bfcp_msg_encode(
      buf, 
      version, 
      false, BFCP_FLOOR_REQUEST, 
      entity.conferenceID, entity.transactionID, entity.userID, 
      0);
    if (err) break;

    for (auto floorID : floorRequest.floorIDs)
    {
      err = build_attr_FLOOR_ID(buf, floorID);
      if (err) break;
    }

    if (floorRequest.hasBeneficiaryID)
    {
      err = build_attr_BENEFICIARY_ID(buf, floorRequest.beneficiaryID);
      if (err) break;
    }

    if (!floorRequest.pInfo.empty())
    {
      err = build_attr_PARTICIPANT_PROVIDED_INFO(buf, floorRequest.pInfo.c_str());
      if (err) break;
    }

    // NOTE: priority is optional
    err = build_attr_PRIORITY(buf, floorRequest.priority);
    if (err) break;

    err = bfcp_msg_update_len(buf, start);
    if (err) break;

  } while (false);

  return err;
}

int build_msg_FloorRelease( mbuf_t *buf, uint8_t version, const bfcp_entity &entity, uint16_t frqID )
{
  assert(buf);
  return bfcp_msg_encode(buf, version, 
    false, BFCP_FLOOR_RELEASE, 
    entity.conferenceID, entity.transactionID, entity.userID,
    1, BFCP_FLOOR_REQUEST_ID | BFCP_MANDATORY, 0, &frqID);
}

int build_msg_FloorRequestQuery( mbuf_t *buf, uint8_t version, const bfcp_entity &entity, uint16_t frqID )
{
  assert(buf);
  return bfcp_msg_encode(buf, version,
    false, BFCP_FLOOR_REQUEST_QUERY,
    entity.conferenceID, entity.transactionID, entity.userID,
    1, BFCP_FLOOR_REQUEST_ID | BFCP_MANDATORY, 0, &frqID);
}

int build_msg_FloorRequestStatus(mbuf_t *buf, bool response,
                                 uint8_t version, const bfcp_entity &entity, 
                                 const FloorRequestInfoParam &frqInfo)
{
  assert(buf);
  size_t start = buf->pos;
  int err = 0;
  do 
  {
    err = bfcp_msg_encode(
      buf, 
      version, 
      response, BFCP_FLOOR_REQUEST_STATUS, 
      entity.conferenceID, entity.transactionID, entity.userID, 
      0);
    if (err) break;

    err = build_attr_FLOOR_REQUEST_INFORMATION(buf, frqInfo);
    if (err) break;

    err = bfcp_msg_update_len(buf, start);
    if (err) break;

  } while (false);

  return err;
}

int build_msg_UserQuery( mbuf_t *buf, uint8_t version, const bfcp_entity &entity, const UserQueryParam &userQuery )
{
  assert(buf);
  return bfcp_msg_encode(
    buf, version, 
    false, BFCP_USER_QUERY,
    entity.conferenceID, entity.transactionID, entity.userID,
    (userQuery.hasBeneficiaryID ? 1 : 0), 
    BFCP_BENEFICIARY_ID | BFCP_MANDATORY, 0, &userQuery.beneficiaryID);
}

int build_msg_UserStatus(mbuf_t *buf, uint8_t version, 
                         const bfcp_entity &entity, 
                         const UserStatusParam &userStatus)
{
  assert(buf);
  size_t start = buf->pos;
  int err = 0;
  do 
  {
    err = bfcp_msg_encode(
      buf, version, 
      true, BFCP_USER_STATUS, 
      entity.conferenceID, entity.transactionID, entity.userID, 
      0);
    if (err) break;

    if (userStatus.hasBeneficiary)
    {
      err = build_attr_BENEFICIARY_INFORMATION(buf, userStatus.beneficiary);
      if (err) break;
    }

    for (auto & floorRequestInfo : userStatus.frqInfoList)
    {
      err = build_attr_FLOOR_REQUEST_INFORMATION(buf, floorRequestInfo);
      if (err) break;
    }

    err = bfcp_msg_update_len(buf, start);
    if (err) break;

  } while (false);

  return err;
}

int build_msg_FloorQuery(mbuf_t *buf, uint8_t version, const bfcp_entity &entity,
                         const bfcp_floor_id_list &fID)
{
  assert(buf);
  size_t start = buf->pos;
  int err = 0;
  do 
  {
    err = bfcp_msg_encode(
      buf, version, 
      false, BFCP_FLOOR_QUERY, 
      entity.conferenceID, entity.transactionID, entity.userID, 
      0);
    if (err) break;

    for (auto floorID : fID)
    {
      err = build_attr_FLOOR_ID(buf, floorID);
      if (err) break;
    }

    err = bfcp_msg_update_len(buf, start);
    if (err) break;

  } while (false);

  return err;
}

int build_msg_FloorStatus(mbuf_t *buf, bool response,
                          uint8_t version, const bfcp_entity &entity, 
                          const FloorStatusParam &floorStatus)
{
  assert(buf);
  size_t start = buf->pos;
  int err = 0;
  do 
  {
    err = bfcp_msg_encode(
      buf, version, 
      response, BFCP_FLOOR_STATUS, 
      entity.conferenceID, entity.transactionID, entity.userID, 
      0);
    if (err) break;

    if (floorStatus.hasFloorID)
    {
      err = build_attr_FLOOR_ID(buf, floorStatus.floorID);
      if (err) break;
    }

    for (auto &floorRequestInfo : floorStatus.frqInfoList)
    {
      err = build_attr_FLOOR_REQUEST_INFORMATION(buf, floorRequestInfo);
      if (err) break;
    }

    err = bfcp_msg_update_len(buf, start);
    if (err) break;

  } while (false);

  return err;
}

int build_msg_ChairAction(mbuf_t *buf, uint8_t version, const bfcp_entity &entity, 
                          const FloorRequestInfoParam &frqInfo)
{
  assert(buf);
  size_t start = buf->pos;
  int err = 0;
  do 
  {
    err = bfcp_msg_encode(
      buf, version, 
      false, BFCP_CHAIR_ACTION, 
      entity.conferenceID, entity.transactionID, entity.userID, 
      0);
    if (err) break;

    err = build_attr_FLOOR_REQUEST_INFORMATION(buf, frqInfo);
    if (err) break;

    err = bfcp_msg_update_len(buf, start);
    if (err) break;

  } while (false);

  return err;
}

int build_msg_ChairActionAck( mbuf_t *buf, uint8_t version, const bfcp_entity &entity )
{
  assert(buf);
  return bfcp_msg_encode(
    buf, version, 
    true, BFCP_CHAIR_ACTION_ACK, 
    entity.conferenceID, entity.transactionID, entity.userID,
    0);
}

int build_msg_Hello( mbuf_t *buf, uint8_t version, const bfcp_entity &entity )
{
  assert(buf);
  return bfcp_msg_encode(
    buf, version, 
    false, BFCP_HELLO, 
    entity.conferenceID, entity.transactionID, entity.userID, 
    0);
}

int build_msg_HelloAck(mbuf_t *buf, uint8_t version, const bfcp_entity &entity, const HelloAckParam &helloAck)
{
  assert(buf);
  bfcp_supprim_t primitives;
  primitives.primc = helloAck.primitives.size();
  primitives.primv = helloAck.primitives.empty() ? 
    nullptr : const_cast<bfcp_prim*>(&helloAck.primitives[0]);

  bfcp_supattr_t attributes;
  attributes.attrc = helloAck.attributes.size();
  attributes.attrv = helloAck.attributes.empty() ?
    nullptr : const_cast<bfcp_attrib*>(&helloAck.attributes[0]);

  return bfcp_msg_encode(
    buf, version, 
    true, BFCP_HELLO_ACK, 
    entity.conferenceID, entity.transactionID, entity.userID, 
    2, 
    BFCP_SUPPORTED_PRIMS | BFCP_MANDATORY, 0, &primitives, 
    BFCP_SUPPORTED_ATTRS | BFCP_MANDATORY, 0, &attributes);
}

int build_msg_Error(mbuf_t *buf, uint8_t version, const bfcp_entity &entity, const ErrorParam &error)
{
  assert(buf);
  
  bfcp_errcode errcode;
  auto &errorCode = error.errorCode;
  errcode.code = errorCode.code;
  errcode.len = errorCode.details.size();
  errcode.details = errorCode.details.empty() ? 
    nullptr : const_cast<uint8_t*>(&errorCode.details[0]);

  const char *eInfo = error.errorInfo.empty() ? 
    nullptr : error.errorInfo.c_str();

  return bfcp_msg_encode(
    buf, version, 
    true, BFCP_ERROR,
    entity.conferenceID, entity.transactionID, entity.userID,
    2,
    BFCP_ERROR_CODE | BFCP_MANDATORY, 0, &errcode,
    BFCP_ERROR_INFO, 0, eInfo);
}

int build_msg_FloorRequestStatusAck( mbuf_t *buf, uint8_t version, const bfcp_entity &entity )
{
  assert(buf);
  return bfcp_msg_encode(
    buf, version, 
    true, BFCP_FLOOR_REQ_STATUS_ACK, 
    entity.conferenceID, entity.transactionID, entity.userID, 
    0);
}

int build_msg_FloorStatusAck( mbuf_t *buf, uint8_t version, const bfcp_entity &entity )
{
  assert(buf);
  return bfcp_msg_encode(
    buf, version, 
    true, BFCP_FLOOR_STATUS_ACK, 
    entity.conferenceID, entity.transactionID, entity.userID, 
    0);
}

int build_msg_Goodbye( mbuf_t *buf, uint8_t version, const bfcp_entity &entity )
{
  assert(buf);
  return bfcp_msg_encode(
    buf, version, 
    false, BFCP_GOODBYE, 
    entity.conferenceID, entity.transactionID, entity.userID, 
    0);
}

int build_msg_GoodbyeAck( mbuf_t *buf, uint8_t version, const bfcp_entity &entity )
{
  assert(buf);
  return bfcp_msg_encode(
    buf, version, 
    true, BFCP_GOODBYE_ACK, 
    entity.conferenceID, entity.transactionID, entity.userID, 
    0);
}

int build_msg_fragments( std::vector<mbuf_t*> &fragBufs, mbuf_t *msgBuf, size_t maxMsgSize )
{
  assert(kHeaderWithFragSize < maxMsgSize);

  int err = 0;
  size_t bufSize = msgBuf->end;
  if (bufSize < maxMsgSize)
  {
    fragBufs.push_back(msgBuf);
    mem_ref(msgBuf);
  }
  else
  {
    size_t start = msgBuf->pos;

    size_t payloadSize = bufSize - kHeaderSize;
    size_t maxFragPayloadSize = (maxMsgSize - kHeaderWithFragSize) & ~0x3;
    size_t fragmentCount = 
      (payloadSize + maxFragPayloadSize - 1) / maxFragPayloadSize;

    std::vector<mbuf_t*> bufs;
    bufs.reserve(fragmentCount);
    
    bfcp_hdr_t header;
    err = bfcp_hdr_decode(&header, msgBuf);
    assert(!err);
    assert(!header.f);

    size_t remainPayloadSize = payloadSize;
    for (size_t i = 0; i < fragmentCount; ++i)
    {
      mbuf_t *buf = mbuf_alloc(maxMsgSize);
      if (!buf)
      {
        err = ENOMEM;
        break;
      }
      bufs.push_back(buf);

      header.f = 1;
      header.fragoffset = i * maxFragPayloadSize / 4;
      size_t actualPayloadSize = (std::min)(maxFragPayloadSize, remainPayloadSize);
      header.fraglen = actualPayloadSize / 4;
      remainPayloadSize -= actualPayloadSize;
      err = bfcp_hdr_encode(buf, &header);
      if (err) break;

      mbuf_write_mem(buf, msgBuf->buf + msgBuf->pos, actualPayloadSize);
      msgBuf->pos += actualPayloadSize;
    }
    if (err)
    {
      for (auto buf : bufs)
      {
        mem_deref(buf);
      }
    }
    else
    {
      assert(msgBuf->pos == msgBuf->end);
      msgBuf->pos = start;
      fragBufs.insert(fragBufs.end(), bufs.begin(), bufs.end());
    }
  }
  return err;
}

} // namespace bfcp