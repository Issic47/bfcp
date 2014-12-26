#ifndef BFCP_EX_H
#define BFCP_EX_H

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
  bfcp_user_info requested_by;
  const bfcp_priority *priority;
  const char *partPriovidedInfo;
} bfcp_floor_request_info;

typedef std::vector<bfcp_floor_request_info> bfcp_floor_request_info_list;


#endif // BFCP_EX_H