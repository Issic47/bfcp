#ifndef BFCP_MSG_BUILDER_H
#define BFCP_MSG_BUILDER_H

#include "common/bfcp_ex.h"

namespace bfcp
{

int build_msg_FloorRequest(mbuf_t *buf, uint8_t version, const bfcp_entity &entity, 
                           const bfcp_floor_id_list &fID, const uint16_t *bID, 
                           const char *pInfo, const bfcp_priority *priority);

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
                    const bfcp_errcode_t &errcode, const char *eInfo);

int build_msg_FloorRequestStatusAck(mbuf_t *buf, uint8_t version, const bfcp_entity &entity);
int build_msg_FloorStatusAck(mbuf_t *buf, uint8_t version, const bfcp_entity &entity);
int build_msg_GoodBye(mbuf_t *buf, uint8_t version, const bfcp_entity &entity);
int build_msg_GoodByeAck(mbuf_t *buf, uint8_t version, const bfcp_entity &entity);

} // namespace bfcp

#endif // BFCP_MSG_BUILDER_H