#ifndef BFCP_EX_H
#define BFCP_EX_H

#include <cstdint>
#include <vector>

extern "C" 
{
/* Basic types */
#include <re_types.h>
#include <re_fmt.h>
#include <re_mbuf.h>
#include <re_msg.h>
#include <re_list.h>
#include <re_sa.h>
#include <re_sys.h>

#include <re_bfcp.h>
#include <re_mem.h>
}

#include <muduo/base/Types.h>

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
  const char *username;
  const char *useruri;
} bfcp_user_info;

typedef struct bfcp_overall_request_status
{
  uint16_t floorRequestID;
  const bfcp_reqstatus_t *requestStatus;
  const char *statusInfo;
} bfcp_overall_request_status;

typedef struct bfcp_floor_request_status 
{
  uint16_t floorID;
  const bfcp_reqstatus_t *requestStatus;
  const char *statusInfo;
} bfcp_floor_request_status;

typedef std::vector<bfcp_floor_request_status> bfcp_floor_request_status_list;

enum AttrValueType
{
  kHasNone = 0,
  kHasOverallRequestStatus = 0x1,
  kHasBeneficiaryInfo = 0x2,
  kHasRequestedByInfo = 0x4,
};

typedef struct bfcp_floor_request_info
{
  uint16_t floorRequestID;
  uint32_t valueType;
  bfcp_overall_request_status oRS;
  bfcp_floor_request_status_list fRS;
  bfcp_user_info beneficiary;
  bfcp_user_info requestedBy;
  const bfcp_priority *priority;
  const char *partPriovidedInfo;
} bfcp_floor_request_info;

typedef std::vector<bfcp_floor_request_info> bfcp_floor_request_info_list;

inline int compare(const bfcp_entity &lhs, const bfcp_entity &rhs)
{
  uint64_t lvalue = uint64_t(lhs.conferenceID) << 32;
  lvalue |= uint64_t(lhs.userID) << 16;
  lvalue |= uint64_t(lhs.transactionID);

  uint64_t rvalue = uint64_t(rhs.conferenceID) << 32;
  rvalue |= uint64_t(rhs.userID) << 16;
  rvalue |= uint64_t(rhs.transactionID);

  return lvalue < rvalue ? -1 : ((lvalue == rvalue) ? 0 : 1);
}

inline bool operator<(const bfcp_entity &lhs, const bfcp_entity &rhs)
{
  return compare(lhs, rhs) < 0;
}

inline bool operator==(const bfcp_entity &lhs, const bfcp_entity &rhs)
{
  return compare(lhs, rhs) == 0;
}

namespace bfcp 
{
  typedef muduo::string string;
}

inline bfcp::string toString(const bfcp_entity &entity)
{
  char buf[64];
  snprintf(buf, sizeof buf, "{cid=%u,tid=%u,uid=%u}", 
    entity.conferenceID, entity.transactionID, entity.userID);
  return buf;
}

inline bfcp::string toString(const bfcp_entity &entity, bfcp_prim primitive)
{
  char buf[64];
  snprintf(buf, sizeof buf, "{cid=%u,tid=%u,uid=%u,prim=%s}", 
    entity.conferenceID, entity.transactionID, entity.userID,
    bfcp_prim_name(primitive));
  return buf;
}

#endif // BFCP_EX_H