/* soapStub.h
   Generated by gSOAP 2.8.21 from server.h

Copyright(C) 2000-2014, Robert van Engelen, Genivia Inc. All Rights Reserved.
The generated code is released under one of the following licenses:
GPL or Genivia's license for commercial use.
This program is released under the GPL with the additional exemption that
compiling, linking, and/or using OpenSSL is allowed.
*/

#ifndef soapStub_H
#define soapStub_H
#include <vector>
#include "stdsoap2.h"
#if GSOAP_VERSION != 20821
# error "GSOAP VERSION 20821 MISMATCH IN GENERATED CODE VERSUS LIBRARY CODE: PLEASE REINSTALL PACKAGE"
#endif


/******************************************************************************\
 *                                                                            *
 * Enumerations                                                               *
 *                                                                            *
\******************************************************************************/


/******************************************************************************\
 *                                                                            *
 * Types with Custom Serializers                                              *
 *                                                                            *
\******************************************************************************/


/******************************************************************************\
 *                                                                            *
 * Classes and Structs                                                        *
 *                                                                            *
\******************************************************************************/


#if 0 /* volatile type: do not declare here, declared elsewhere */

#endif

#ifndef SOAP_TYPE_ns__ConferenceList
#define SOAP_TYPE_ns__ConferenceList (8)
/* ns:ConferenceList */
class SOAP_CMAC ns__ConferenceList
{
public:
	std::vector<unsigned int >conferenceIDs;	/* SOAP 1.2 RPC return element (when namespace qualified) */	/* optional element of type xsd:unsignedInt */
public:
	virtual int soap_type() const { return 8; } /* = unique type id SOAP_TYPE_ns__ConferenceList */
	virtual void soap_default(struct soap*);
	virtual void soap_serialize(struct soap*) const;
	virtual int soap_put(struct soap*, const char*, const char*) const;
	virtual int soap_out(struct soap*, const char*, int, const char*) const;
	virtual void *soap_get(struct soap*, const char*, const char*);
	virtual void *soap_in(struct soap*, const char*, const char*);
	         ns__ConferenceList() { ns__ConferenceList::soap_default(NULL); }
	virtual ~ns__ConferenceList() { }
};
#endif

#ifndef SOAP_TYPE_ns__start
#define SOAP_TYPE_ns__start (16)
/* ns:start */
struct ns__start
{
public:
	unsigned short port;	/* required element of type xsd:unsignedShort */
	bool enbaleConnectionThread;	/* required element of type xsd:boolean */
	int workThreadNum;	/* required element of type xsd:int */
public:
	int soap_type() const { return 16; } /* = unique type id SOAP_TYPE_ns__start */
};
#endif

#ifndef SOAP_TYPE_ns__stop
#define SOAP_TYPE_ns__stop (18)
/* ns:stop */
struct ns__stop
{
public:
	int soap_type() const { return 18; } /* = unique type id SOAP_TYPE_ns__stop */
};
#endif

#ifndef SOAP_TYPE_ns__addConference
#define SOAP_TYPE_ns__addConference (21)
/* ns:addConference */
struct ns__addConference
{
public:
	unsigned int conferenceID;	/* required element of type xsd:unsignedInt */
	unsigned short maxFloorRequest;	/* required element of type xsd:unsignedShort */
	char *policy;	/* optional element of type xsd:string */
	double timeForChairAction;	/* required element of type xsd:double */
public:
	int soap_type() const { return 21; } /* = unique type id SOAP_TYPE_ns__addConference */
};
#endif

#ifndef SOAP_TYPE_ns__removeConference
#define SOAP_TYPE_ns__removeConference (23)
/* ns:removeConference */
struct ns__removeConference
{
public:
	unsigned int conferenceID;	/* required element of type xsd:unsignedInt */
public:
	int soap_type() const { return 23; } /* = unique type id SOAP_TYPE_ns__removeConference */
};
#endif

#ifndef SOAP_TYPE_ns__changeMaxFloorRequest
#define SOAP_TYPE_ns__changeMaxFloorRequest (25)
/* ns:changeMaxFloorRequest */
struct ns__changeMaxFloorRequest
{
public:
	unsigned int conferenceID;	/* required element of type xsd:unsignedInt */
	unsigned short maxFloorRequest;	/* required element of type xsd:unsignedShort */
public:
	int soap_type() const { return 25; } /* = unique type id SOAP_TYPE_ns__changeMaxFloorRequest */
};
#endif

#ifndef SOAP_TYPE_ns__changeAcceptPolicy
#define SOAP_TYPE_ns__changeAcceptPolicy (27)
/* ns:changeAcceptPolicy */
struct ns__changeAcceptPolicy
{
public:
	unsigned int conferenceID;	/* required element of type xsd:unsignedInt */
	char *policy;	/* optional element of type xsd:string */
	double timeForChairActoin;	/* required element of type xsd:double */
public:
	int soap_type() const { return 27; } /* = unique type id SOAP_TYPE_ns__changeAcceptPolicy */
};
#endif

#ifndef SOAP_TYPE_ns__addFloor
#define SOAP_TYPE_ns__addFloor (29)
/* ns:addFloor */
struct ns__addFloor
{
public:
	unsigned int conferenceID;	/* required element of type xsd:unsignedInt */
	unsigned short floorID;	/* required element of type xsd:unsignedShort */
	unsigned short maxGrantedNum;	/* required element of type xsd:unsignedShort */
public:
	int soap_type() const { return 29; } /* = unique type id SOAP_TYPE_ns__addFloor */
};
#endif

#ifndef SOAP_TYPE_ns__removeFloor
#define SOAP_TYPE_ns__removeFloor (31)
/* ns:removeFloor */
struct ns__removeFloor
{
public:
	unsigned int conferenceID;	/* required element of type xsd:unsignedInt */
	unsigned short floorID;	/* required element of type xsd:unsignedShort */
public:
	int soap_type() const { return 31; } /* = unique type id SOAP_TYPE_ns__removeFloor */
};
#endif

#ifndef SOAP_TYPE_ns__changeMaxGrantedNum
#define SOAP_TYPE_ns__changeMaxGrantedNum (33)
/* ns:changeMaxGrantedNum */
struct ns__changeMaxGrantedNum
{
public:
	unsigned int conferenceID;	/* required element of type xsd:unsignedInt */
	unsigned short floorID;	/* required element of type xsd:unsignedShort */
	unsigned short maxGrantedNum;	/* required element of type xsd:unsignedShort */
public:
	int soap_type() const { return 33; } /* = unique type id SOAP_TYPE_ns__changeMaxGrantedNum */
};
#endif

#ifndef SOAP_TYPE_ns__addUser
#define SOAP_TYPE_ns__addUser (35)
/* ns:addUser */
struct ns__addUser
{
public:
	unsigned int conferenceID;	/* required element of type xsd:unsignedInt */
	unsigned short userID;	/* required element of type xsd:unsignedShort */
	char *userName;	/* optional element of type xsd:string */
	char *userURI;	/* optional element of type xsd:string */
public:
	int soap_type() const { return 35; } /* = unique type id SOAP_TYPE_ns__addUser */
};
#endif

#ifndef SOAP_TYPE_ns__removeUser
#define SOAP_TYPE_ns__removeUser (37)
/* ns:removeUser */
struct ns__removeUser
{
public:
	unsigned int conferenceID;	/* required element of type xsd:unsignedInt */
	unsigned short userID;	/* required element of type xsd:unsignedShort */
public:
	int soap_type() const { return 37; } /* = unique type id SOAP_TYPE_ns__removeUser */
};
#endif

#ifndef SOAP_TYPE_ns__addChair
#define SOAP_TYPE_ns__addChair (39)
/* ns:addChair */
struct ns__addChair
{
public:
	unsigned int conferenceID;	/* required element of type xsd:unsignedInt */
	unsigned short floorID;	/* required element of type xsd:unsignedShort */
	unsigned short userID;	/* required element of type xsd:unsignedShort */
public:
	int soap_type() const { return 39; } /* = unique type id SOAP_TYPE_ns__addChair */
};
#endif

#ifndef SOAP_TYPE_ns__removeChair
#define SOAP_TYPE_ns__removeChair (41)
/* ns:removeChair */
struct ns__removeChair
{
public:
	unsigned int conferenceID;	/* required element of type xsd:unsignedInt */
	unsigned short floorID;	/* required element of type xsd:unsignedShort */
public:
	int soap_type() const { return 41; } /* = unique type id SOAP_TYPE_ns__removeChair */
};
#endif

#ifndef SOAP_TYPE_ns__getConferenceIDs
#define SOAP_TYPE_ns__getConferenceIDs (44)
/* ns:getConferenceIDs */
struct ns__getConferenceIDs
{
public:
	int soap_type() const { return 44; } /* = unique type id SOAP_TYPE_ns__getConferenceIDs */
};
#endif

#ifndef SOAP_TYPE_ns__getConferenceInfoResponse
#define SOAP_TYPE_ns__getConferenceInfoResponse (47)
/* ns:getConferenceInfoResponse */
struct ns__getConferenceInfoResponse
{
public:
	char *conferenceInfo;	/* SOAP 1.2 RPC return element (when namespace qualified) */	/* required element of type xsd:string */
public:
	int soap_type() const { return 47; } /* = unique type id SOAP_TYPE_ns__getConferenceInfoResponse */
};
#endif

#ifndef SOAP_TYPE_ns__getConferenceInfo
#define SOAP_TYPE_ns__getConferenceInfo (48)
/* ns:getConferenceInfo */
struct ns__getConferenceInfo
{
public:
	unsigned int conferenceID;	/* required element of type xsd:unsignedInt */
public:
	int soap_type() const { return 48; } /* = unique type id SOAP_TYPE_ns__getConferenceInfo */
};
#endif

#ifndef WITH_NOGLOBAL

#ifndef SOAP_TYPE_SOAP_ENV__Header
#define SOAP_TYPE_SOAP_ENV__Header (49)
/* SOAP Header: */
struct SOAP_ENV__Header
{
public:
	int soap_type() const { return 49; } /* = unique type id SOAP_TYPE_SOAP_ENV__Header */
};
#endif

#endif

#ifndef WITH_NOGLOBAL

#ifndef SOAP_TYPE_SOAP_ENV__Code
#define SOAP_TYPE_SOAP_ENV__Code (50)
/* SOAP Fault Code: */
struct SOAP_ENV__Code
{
public:
	char *SOAP_ENV__Value;	/* optional element of type xsd:QName */
	struct SOAP_ENV__Code *SOAP_ENV__Subcode;	/* optional element of type SOAP-ENV:Code */
public:
	int soap_type() const { return 50; } /* = unique type id SOAP_TYPE_SOAP_ENV__Code */
};
#endif

#endif

#ifndef WITH_NOGLOBAL

#ifndef SOAP_TYPE_SOAP_ENV__Detail
#define SOAP_TYPE_SOAP_ENV__Detail (52)
/* SOAP-ENV:Detail */
struct SOAP_ENV__Detail
{
public:
	char *__any;
	int __type;	/* any type of element <fault> (defined below) */
	void *fault;	/* transient */
public:
	int soap_type() const { return 52; } /* = unique type id SOAP_TYPE_SOAP_ENV__Detail */
};
#endif

#endif

#ifndef WITH_NOGLOBAL

#ifndef SOAP_TYPE_SOAP_ENV__Reason
#define SOAP_TYPE_SOAP_ENV__Reason (54)
/* SOAP-ENV:Reason */
struct SOAP_ENV__Reason
{
public:
	char *SOAP_ENV__Text;	/* optional element of type xsd:string */
public:
	int soap_type() const { return 54; } /* = unique type id SOAP_TYPE_SOAP_ENV__Reason */
};
#endif

#endif

#ifndef WITH_NOGLOBAL

#ifndef SOAP_TYPE_SOAP_ENV__Fault
#define SOAP_TYPE_SOAP_ENV__Fault (55)
/* SOAP Fault: */
struct SOAP_ENV__Fault
{
public:
	char *faultcode;	/* optional element of type xsd:QName */
	char *faultstring;	/* optional element of type xsd:string */
	char *faultactor;	/* optional element of type xsd:string */
	struct SOAP_ENV__Detail *detail;	/* optional element of type SOAP-ENV:Detail */
	struct SOAP_ENV__Code *SOAP_ENV__Code;	/* optional element of type SOAP-ENV:Code */
	struct SOAP_ENV__Reason *SOAP_ENV__Reason;	/* optional element of type SOAP-ENV:Reason */
	char *SOAP_ENV__Node;	/* optional element of type xsd:string */
	char *SOAP_ENV__Role;	/* optional element of type xsd:string */
	struct SOAP_ENV__Detail *SOAP_ENV__Detail;	/* optional element of type SOAP-ENV:Detail */
public:
	int soap_type() const { return 55; } /* = unique type id SOAP_TYPE_SOAP_ENV__Fault */
};
#endif

#endif

/******************************************************************************\
 *                                                                            *
 * Typedefs                                                                   *
 *                                                                            *
\******************************************************************************/

#ifndef SOAP_TYPE__QName
#define SOAP_TYPE__QName (5)
typedef char *_QName;
#endif

#ifndef SOAP_TYPE__XML
#define SOAP_TYPE__XML (6)
typedef char *_XML;
#endif


/******************************************************************************\
 *                                                                            *
 * Externals                                                                  *
 *                                                                            *
\******************************************************************************/


#endif

/* End of soapStub.h */
