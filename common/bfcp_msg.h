#ifndef BFCP_MSG_H
#define BFCP_MSG_H

#include <vector>

#include <re.h>

typedef struct mbuf mbuf_t;

typedef struct bfcp_entity
{
  uint32_t conferenceID;
  uint16_t transactionID;
  uint16_t userID;
} bfcp_entity;

typedef std::vector<uint16_t> bfcp_floor_id_list;

typedef struct bfcp_user_info
{
  uint16_t id;
  char *username;
  char *useruri;
} bfcp_user_info;

typedef struct bfcp_overall_request_status
{
  uint16_t floorRequestID;
  bfcp_reqstatus_t *requestStatus;
  char *statusInfo;
} bfcp_overall_request_status;

typedef struct bfcp_floor_request_status 
{
  uint16_t floorID;
  bfcp_reqstatus_t *requestStatus;
  char *statusInfo;
} bfcp_floor_request_status;

typedef std::vector<bfcp_floor_request_status> bfcp_floor_request_status_list;

typedef struct bfcp_floor_request_info
{
  uint16_t floorRequestID;
  bfcp_overall_request_status *oRS;
  bfcp_floor_request_status_list fRS;
  bfcp_user_info *beneficiary;
  bfcp_user_info *requested_by;
  bfcp_priority *priority;
  char *partPriovidedInfo;
} bfcp_floor_request_info;

typedef std::vector<bfcp_floor_request_info> bfcp_floor_request_info_list;

namespace bfcp
{

int build_msg_FloorRequest(mbuf_t *buf, uint8_t version, const bfcp_entity &entity, 
                           const bfcp_floor_id_list &fID, uint16_t *bID, 
                           char *pInfo, bfcp_priority *priority);

int build_msg_FloorRelease(mbuf_t *buf, uint8_t version, const bfcp_entity &entity, uint16_t frqID);
int build_msg_FloorRequestQuery(mbuf_t *buf, uint8_t version, const bfcp_entity &entity, uint16_t frqID);

int build_msg_FloorRequestStatus(mbuf_t *buf, uint8_t version, const bfcp_entity &entity, 
                                 const bfcp_floor_request_info &frqInfo);
int build_msg_UserQuery(mbuf_t *buf, uint8_t version, const bfcp_entity &entity, uint16_t bID);

int build_msg_UserStatus(mbuf_t *buf, uint8_t version, const bfcp_entity &entity, 
                         bfcp_user_info *beneficiary, const bfcp_floor_request_info_list &frqInfo);

int build_msg_FloorQuery(mbuf_t *buf, uint8_t version, const bfcp_entity &entity, 
                         const bfcp_floor_id_list &fID);

int build_msg_FloorStatus(mbuf_t *buf, uint8_t version, const bfcp_entity &entity,
                          uint16_t floorID, const bfcp_floor_request_info_list &frqInfo);

int build_msg_ChairAction(mbuf_t *buf, uint8_t version, const bfcp_entity &entity,
                          const bfcp_floor_request_info &frqInfo);

int build_msg_ChairActionAck(mbuf_t *buf, uint8_t version, const bfcp_entity &entity);
int build_msg_Hello(mbuf_t *buf, uint8_t version, const bfcp_entity &entity);

int build_msg_HelloAck(mbuf_t *buf, uint8_t version, const bfcp_entity &entity,
                       const bfcp_supprim_t &primitives, const bfcp_supattr_t &attributes);

int build_msg_Error(mbuf_t *buf, uint8_t version, const bfcp_entity &entity,
                    const bfcp_errcode_t &errcode, char *eInfo);

int build_msg_FloorRequestStatusAck(mbuf_t *buf, uint8_t version, const bfcp_entity &entity);
int build_msg_FloorStatusAck(mbuf_t *buf, uint8_t version, const bfcp_entity &entity);
int build_msg_GoodBye(mbuf_t *buf, uint8_t version, const bfcp_entity &entity);
int build_msg_GoodByeAck(mbuf_t *buf, uint8_t version, const bfcp_entity &entity);

} // namespace bfcp

#endif // BFCP_MSG_H