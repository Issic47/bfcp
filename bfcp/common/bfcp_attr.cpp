#include <bfcp/common/bfcp_attr.h>

namespace bfcp
{

bfcp_user_info BfcpAttr::getBeneficiaryInfo() const
{
  bfcp_user_info info;
  info.id = getBeneficiaryID();

  bfcp_attr_t *displayNameAttr = findSubAttr(BFCP_USER_DISP_NAME);
  info.username = displayNameAttr ? displayNameAttr->v.userdname : nullptr;

  bfcp_attr_t *userUriAttr = findSubAttr(BFCP_USER_URI);
  info.useruri = userUriAttr ? userUriAttr->v.useruri : nullptr;

  return info;
}

bfcp_user_info BfcpAttr::getRequestedByInfo() const
{
  bfcp_user_info info;
  info.id = getRequestByID();

  bfcp_attr_t *displayNameAttr = findSubAttr(BFCP_USER_DISP_NAME);
  info.username = displayNameAttr ? displayNameAttr->v.userdname : nullptr;

  bfcp_attr_t *userUriAttr = findSubAttr(BFCP_USER_URI);
  info.useruri = userUriAttr ? userUriAttr->v.useruri : nullptr;

  return info;
}

bfcp_floor_request_status BfcpAttr::getFloorRequestStatus() const
{
  bfcp_floor_request_status fRS;
  fRS.floorID = getFloorID();

  bfcp_attr_t *requestStatusAttr = findSubAttr(BFCP_REQUEST_STATUS);
  fRS.requestStatus = requestStatusAttr ? &requestStatusAttr->v.reqstatus : nullptr;

  bfcp_attr_t *statusInfoAttr = findSubAttr(BFCP_STATUS_INFO);
  fRS.statusInfo = statusInfoAttr ? statusInfoAttr->v.statusinfo : nullptr;

  return fRS;
}

bfcp_overall_request_status BfcpAttr::getOverallRequestStatus() const
{
  bfcp_overall_request_status oRS;
  oRS.floorRequestID = getFloorRequestID();

  bfcp_attr_t *requestStatusAttr = findSubAttr(BFCP_REQUEST_STATUS);
  oRS.requestStatus = requestStatusAttr ? &requestStatusAttr->v.reqstatus : nullptr;

  bfcp_attr_t *statusInfoAttr = findSubAttr(BFCP_STATUS_INFO);
  oRS.statusInfo = statusInfoAttr ? statusInfoAttr->v.statusinfo : nullptr;

  return oRS;
}

bfcp_floor_request_info BfcpAttr::getFloorRequestInfo() const
{
  bfcp_floor_request_info info;
  info.floorRequestID = getFloorRequestID();
  info.valueType = kHasNone;
  
  bfcp_attr_t *overallRequestStatus = findSubAttr(BFCP_OVERALL_REQ_STATUS);
  if (overallRequestStatus) 
  {
    info.valueType |= kHasOverallRequestStatus;
    info.oRS = BfcpAttr(*overallRequestStatus).getOverallRequestStatus();
  }

  auto attrs = findSubAttrs(BFCP_FLOOR_REQ_STATUS);
  for (auto attr : attrs)
  {
    info.fRS.push_back(BfcpAttr(*attr).getFloorRequestStatus());
  }

  bfcp_attr_t *beneficiaryInfo = findSubAttr(BFCP_BENEFICIARY_INFO);
  if (beneficiaryInfo)
  {
    info.valueType |= kHasBeneficiaryInfo;
    info.beneficiary = BfcpAttr(*beneficiaryInfo).getBeneficiaryInfo();
  }

  bfcp_attr_t *requestedByInfo = findSubAttr(BFCP_REQUESTED_BY_INFO);
  if (requestedByInfo)
  {
    info.valueType |= kHasRequestedByInfo;
    info.requestedBy = BfcpAttr(*requestedByInfo).getRequestedByInfo();
  }

  bfcp_attr_t *priority = findSubAttr(BFCP_PRIORITY);
  info.priority = priority ? &priority->v.priority : nullptr;

  bfcp_attr_t *partPrivodedInfo = findSubAttr(BFCP_PART_PROV_INFO);
  info.partPriovidedInfo = partPrivodedInfo ? partPrivodedInfo->v.partprovinfo : nullptr;

  return info;
}

std::list<bfcp_attr_t*> BfcpAttr::findSubAttrs( bfcp_attrib type ) const
{
  std::list<bfcp_attr_t*> subAttrs;
  struct le *le = list_head(&attr_.attrl);
  while (le)
  {
    bfcp_attr_t *attr = static_cast<bfcp_attr_t*>(le->data);
    le = le->next;

    if (attr->type == type)
    {
      subAttrs.push_back(attr);
    }
  }
  return subAttrs;
}

} // namespace bfcp
 


