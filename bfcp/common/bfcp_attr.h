#ifndef BFCP_ATTR_H
#define BFCP_ATTR_H

#include <bfcp/common/bfcp_ex.h>

#include <list>

namespace bfcp
{

class BfcpAttr
{
public:
  BfcpAttr(const bfcp_attr_t &attr) : attr_(attr) {}

  bfcp_attrib getType() const { return attr_.type; }
  bool isMandatory() const { return attr_.mand; }

  uint16_t getBeneficiaryID() const { return attr_.v.beneficiaryid; }
  uint16_t getFloorID() const { return attr_.v.floorid; }
  uint16_t getFloorRequestID() const { return attr_.v.floorreqid; }
  bfcp_priority getPriority() const { return attr_.v.priority; }
  const bfcp_reqstatus_t& getRequestStatus() const { return attr_.v.reqstatus; }
  const bfcp_errcode& getErrorCode() const { return attr_.v.errcode; }
  const char* getErrorInfo() const { return attr_.v.errinfo; }
  const char* getParticipantProvidedInfo() const { return attr_.v.partprovinfo; }
  const char* getStatusInfo() const { return attr_.v.statusinfo; }
  const bfcp_supattr_t& getSupportedAttrs() const { return attr_.v.supattr; }
  const bfcp_supprim_t& getSupportedPrims() const { return attr_.v.supprim; }
  const char* getUsername() const { return attr_.v.userdname; }
  const char* getUserURI() const { return attr_.v.useruri; }
  uint16_t getRequestByID() const { return attr_.v.reqbyid; }

  /* for group attrs */
  bfcp_user_info getBeneficiaryInfo() const;
  bfcp_user_info getRequestedByInfo() const;
  bfcp_floor_request_status getFloorRequestStatus() const;
  bfcp_overall_request_status getOverallRequestStatus() const;
  bfcp_floor_request_info getFloorRequestInfo() const;

private:
  bfcp_attr_t* findSubAttr(bfcp_attrib type) const
  { return bfcp_attr_subattr(&attr_, type); }

  std::list<bfcp_attr_t*> findSubAttrs(bfcp_attrib type) const;

  const bfcp_attr_t &attr_;
};

} // namespace bfcp


#endif // BFCP_ATTR_H