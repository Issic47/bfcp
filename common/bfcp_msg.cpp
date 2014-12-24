#include "bfcp_msg.h"

#include <cassert>

namespace bfcp
{
namespace detail
{

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

inline int build_attr_ERROR_INFO( mbuf_t *buf, char *eInfo )
{
  return bfcp_attrs_encode(buf, 1, BFCP_ERROR_INFO, 0, eInfo);
}

inline int build_attr_PARTICIPANT_PROVIDED_INFO( mbuf_t *buf, char *pInfo )
{
  return bfcp_attrs_encode(buf, 1, BFCP_PART_PROV_INFO, 0, pInfo);
}

inline int build_attr_STATUS_INFO( mbuf_t *buf, char *sInfo )
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

inline int build_attr_USER_DISPLAY_NAME( mbuf_t *buf, char *username )
{
  return bfcp_attrs_encode(buf, 1, BFCP_USER_DISP_NAME, 0, username);
}

inline int bfcp_build_attr_USER_URI( mbuf_t *buf, char *useruri )
{
  return bfcp_attrs_encode(buf, 1, BFCP_USER_URI, 0, useruri);
}

inline int build_attr_BENEFICIARY_INFORMATION( mbuf_t *buf, const bfcp_user_info &beneficiary )
{
  return bfcp_attrs_encode(
    buf, 1, BFCP_BENEFICIARY_INFO | BFCP_MANDATORY, 2, &beneficiary.id,
    BFCP_USER_DISP_NAME, 0, beneficiary.username,
    BFCP_USER_URI, 0, beneficiary.useruri);
}

inline int build_attr_REQUESTED_BY_INFORMATION( mbuf_t *buf, const bfcp_user_info &requested_by )
{
  return bfcp_attrs_encode(
    buf, 1, BFCP_BENEFICIARY_INFO | BFCP_MANDATORY, 2, &requested_by.id,
    BFCP_USER_DISP_NAME, 0, requested_by.username,
    BFCP_USER_URI, 0, requested_by.useruri);
}

inline int build_attr_FLOOR_REQUEST_STATUS( mbuf_t *buf, const bfcp_floor_request_status &fRS )
{
  return bfcp_attrs_encode(
    buf, 1, BFCP_FLOOR_REQ_STATUS | BFCP_MANDATORY, 2, &fRS.floorID,
    BFCP_REQUEST_STATUS, 0, fRS.requestStatus,
    BFCP_STATUS_INFO, 0, fRS.statusInfo);
}

inline int build_attr_OVERALL_REQUEST_STATUS( mbuf_t *buf, const bfcp_overall_request_status &oRS )
{
  return bfcp_attrs_encode(
    buf, 1, BFCP_OVERALL_REQ_STATUS | BFCP_MANDATORY, 2, &oRS.floorRequestID,
    BFCP_REQUEST_STATUS, 0, oRS.requestStatus,
    BFCP_STATUS_INFO, 0, oRS.statusInfo);
}

int build_attr_FLOOR_REQUEST_INFORMATION( mbuf_t *buf, const bfcp_floor_request_info &frqInfo )
{
  size_t start = buf->pos;
  int err = 0;
  do 
  {
    err = bfcp_attrs_encode(buf, 1, BFCP_FLOOR_REQ_INFO | BFCP_MANDATORY, 0, &frqInfo.floorRequestID);
    if (err) break;

    if (frqInfo.oRS)
    {
      err = build_attr_OVERALL_REQUEST_STATUS(buf, *frqInfo.oRS);
      if (err) break;
    }

    for (auto &floorRequestStatus : frqInfo.fRS)
    {
      err = build_attr_FLOOR_REQUEST_STATUS(buf, floorRequestStatus);
      if (err) break;
    }

    if (frqInfo.beneficiary)
    {
      err = build_attr_BENEFICIARY_INFORMATION(buf, *frqInfo.beneficiary);
      if (err) break;
    }

    if (frqInfo.requested_by)
    {
      err = build_attr_REQUESTED_BY_INFORMATION(buf, *frqInfo.requested_by);
      if (err) break;
    }

    if (frqInfo.priority)
    {
      err = build_attr_PRIORITY(buf, *frqInfo.priority);
      if (err) break;
    }

    if (frqInfo.partPriovidedInfo)
    {
      err = build_attr_PARTICIPANT_PROVIDED_INFO(buf, frqInfo.partPriovidedInfo);
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

int build_msg_FloorRequest(mbuf_t *buf, uint8_t version, const bfcp_entity &entity, 
                           const bfcp_floor_id_list &fID, uint16_t *bID, char *pInfo, bfcp_priority *priority)
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

    for (auto floorID : fID)
    {
      err = build_attr_FLOOR_ID(buf, floorID);
      if (err) break;
    }

    if (bID)
    {
      err = build_attr_BENEFICIARY_ID(buf, *bID);
      if (err) break;
    }

    if (pInfo)
    {
      err = build_attr_PARTICIPANT_PROVIDED_INFO(buf, pInfo);
      if (err) break;
    }

    if (priority)
    {
      err = build_attr_PRIORITY(buf, *priority);
      if (err) break;
    }

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
    1, BFCP_FLOOR_REQUEST_ID | BFCP_MANDATORY, 0, frqID);
}

int build_msg_FloorRequestQuery( mbuf_t *buf, uint8_t version, const bfcp_entity &entity, uint16_t frqID )
{
  assert(buf);
  return bfcp_msg_encode(buf, version,
    false, BFCP_FLOOR_REQUEST_QUERY,
    entity.conferenceID, entity.transactionID, entity.userID,
    1, BFCP_FLOOR_REQUEST_ID | BFCP_MANDATORY, 0, frqID);
}

int build_msg_FloorRequestStatus(mbuf_t *buf, uint8_t version, const bfcp_entity &entity, 
                                 const bfcp_floor_request_info &frqInfo)
{
  assert(buf);
  size_t start = buf->pos;
  int err = 0;
  do 
  {
    err = bfcp_msg_encode(
      buf, 
      version, 
      true, BFCP_FLOOR_REQUEST_STATUS, 
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

int build_msg_UserQuery( mbuf_t *buf, uint8_t version, const bfcp_entity &entity, uint16_t bID )
{
  assert(buf);
  return bfcp_msg_encode(
    buf, version, 
    false, BFCP_USER_QUERY,
    entity.conferenceID, entity.transactionID, entity.userID,
    1, BFCP_BENEFICIARY_ID | BFCP_MANDATORY, 0, bID);
}

int build_msg_UserStatus(mbuf_t *buf, uint8_t version, const bfcp_entity &entity, 
                         bfcp_user_info *beneficiary, const bfcp_floor_request_info_list &frqInfo)
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

    if (beneficiary)
    {
      err = build_attr_BENEFICIARY_INFORMATION(buf, *beneficiary);
      if (err) break;
    }

    for (auto & floorRequestInfo : frqInfo)
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

int build_msg_FloorStatus(mbuf_t *buf, uint8_t version, const bfcp_entity &entity, 
                          uint16_t floorID, const bfcp_floor_request_info_list &frqInfo)
{
  assert(buf);
  size_t start = buf->pos;
  int err = 0;
  do 
  {
    err = bfcp_msg_encode(
      buf, version, 
      true, BFCP_FLOOR_STATUS, 
      entity.conferenceID, entity.transactionID, entity.userID, 
      0);
    if (err) break;

    err = build_attr_FLOOR_ID(buf, floorID);
    if (err) break;

    for (auto &floorRequestInfo : frqInfo)
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
                          const bfcp_floor_request_info &frqInfo)
{
  assert(buf);
  size_t start = buf->pos;
  int err = 0;
  do 
  {
    err = bfcp_msg_encode(
      buf, version, 
      true, BFCP_CHAIR_ACTION, 
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

int build_msg_HelloAck( mbuf_t *buf, uint8_t version, const bfcp_entity &entity, const bfcp_supprim_t &primitives, const bfcp_supattr_t &attributes )
{
  assert(buf);
  return bfcp_msg_encode(
    buf, version, 
    true, BFCP_HELLO_ACK, 
    entity.conferenceID, entity.transactionID, entity.userID, 
    2, 
    BFCP_SUPPORTED_PRIMS | BFCP_MANDATORY, 0, &primitives, 
    BFCP_SUPPORTED_ATTRS | BFCP_MANDATORY, 0, &attributes);
}

int build_msg_Error( mbuf_t *buf, uint8_t version, const bfcp_entity &entity, const bfcp_errcode_t &errcode, char *eInfo )
{
  assert(buf);
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

int build_msg_GoodBye( mbuf_t *buf, uint8_t version, const bfcp_entity &entity )
{
  assert(buf);
  return bfcp_msg_encode(
    buf, version, 
    false, BFCP_GOODBYE, 
    entity.conferenceID, entity.transactionID, entity.userID, 
    0);
}

int build_msg_GoodByeAck( mbuf_t *buf, uint8_t version, const bfcp_entity &entity )
{
  assert(buf);
  return bfcp_msg_encode(
    buf, version, 
    true, BFCP_GOODBYE_ACK, 
    entity.conferenceID, entity.transactionID, entity.userID, 
    0);
}

} // namespace bfcp