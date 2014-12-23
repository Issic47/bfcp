#ifndef BFCP_MSG_H
#define BFCP_MSG_H

#include <cstdint>
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

typedef struct bfcp_user_information
{
  uint16_t id;
  char *display;
  char *uri;
} bfcp_user_information;

typedef struct bfcp_overall_request_status
{
  uint16_t floorRequestID;
  bfcp_reqstatus_t *requestStatus;
  char *statusInfo;
};

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
  bfcp_user_information *beneficiary;
  bfcp_user_information *requested_by;
  bfcp_priority priority;
  char *partPriovidedInfo;
} bfcp_floor_request_info;

typedef std::vector<bfcp_floor_request_info> bfcp_floor_request_info_list;

#endif // BFCP_MSG_H