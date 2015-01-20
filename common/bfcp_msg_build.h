#ifndef BFCP_MSG_BUILD_H
#define BFCP_MSG_BUILD_H

#include "common/bfcp_ex.h"
#include "common/bfcp_build_param.h"

namespace bfcp
{

int build_msg_FloorRequest(mbuf_t *buf, uint8_t version, 
                           const bfcp_entity &entity, 
                           const FloorRequestParam &floorRequest);

int build_msg_FloorRelease(mbuf_t *buf, uint8_t version, const bfcp_entity &entity, uint16_t frqID);
int build_msg_FloorRequestQuery(mbuf_t *buf, uint8_t version, const bfcp_entity &entity, uint16_t frqID);

int build_msg_FloorRequestStatus(mbuf_t *buf, bool response, 
                                 uint8_t version, const bfcp_entity &entity, 
                                 const FloorRequestInfoParam &frqInfo);

int build_msg_UserQuery(mbuf_t *buf, uint8_t version, const bfcp_entity &entity, uint16_t bID);

int build_msg_UserStatus(mbuf_t *buf, uint8_t version,
                         const bfcp_entity &entity, 
                         const UserStatusParam &userStatus);

int build_msg_FloorQuery(mbuf_t *buf, uint8_t version, 
                         const bfcp_entity &entity, 
                         const bfcp_floor_id_list &fID);

int build_msg_FloorStatus(mbuf_t *buf, bool response, 
                          uint8_t version, const bfcp_entity &entity,
                          const FloorStatusParam &floorStatus);

int build_msg_ChairAction(mbuf_t *buf, uint8_t version, const bfcp_entity &entity,
                          const FloorRequestInfoParam &frqInfo);

int build_msg_ChairActionAck(mbuf_t *buf, uint8_t version, const bfcp_entity &entity);
int build_msg_Hello(mbuf_t *buf, uint8_t version, const bfcp_entity &entity);

int build_msg_HelloAck(mbuf_t *buf, uint8_t version, const bfcp_entity &entity,
                       const HelloAckParam &helloAck);

int build_msg_Error(mbuf_t *buf, uint8_t version, const bfcp_entity &entity,
                    const ErrorParam &error);

int build_msg_FloorRequestStatusAck(mbuf_t *buf, uint8_t version, const bfcp_entity &entity);
int build_msg_FloorStatusAck(mbuf_t *buf, uint8_t version, const bfcp_entity &entity);
int build_msg_GoodBye(mbuf_t *buf, uint8_t version, const bfcp_entity &entity);
int build_msg_GoodByeAck(mbuf_t *buf, uint8_t version, const bfcp_entity &entity);

} // namespace bfcp

#endif // BFCP_MSG_BUILD_H