/*
	calc.h

	This is a gSOAP header file with web service definitions.

	The service operations and type definitions use familiar C/C++ syntax.

	The following methods are defined by this gSOAP service definition:

	add(a,b)
	sub(a,b)
	mul(a,b)
	div(a,b)
	pow(a,b)

	Compilation in C (see samples/calc):
	$ soapcpp2 -c calc.h
	$ cc -o calcclient calcclient.c stdsoap2.c soapC.c soapClient.c
	$ cc -o calcserver calcserver.c stdsoap2.c soapC.c soapServer.c

	Compilation in C++ (see samples/calc++):
	$ soapcpp2 -i calc.h
	$ cc -o calcclient++ calcclient.cpp stdsoap2.cpp soapC.cpp soapcalcProxy.cpp
	$ cc -o calcserver++ calcserver.cpp stdsoap2.cpp soapC.cpp soapcalcService.cpp

	Note that soapcpp2 option -i generates proxy and service classes, which
	encapsulate the method operations in the class instead of defining them
	as global functions as in C. 

	The //gsoap directives are used to bind XML namespaces and to define
	Web service properties:

	//gsoap <ns> service name: <WSDLserviceName> <documentationText>
	//gsoap <ns> service style: [rpc|document]
	//gsoap <ns> service encoding: [literal|encoded]
	//gsoap <ns> service namespace: <WSDLnamespaceURI>
	//gsoap <ns> service location: <WSDLserviceAddressLocationURI>

	Web service operation properties:

	//gsoap <ns> service method-style: <methodName> [rpc|document]
	//gsoap <ns> service method-encoding: <methodName> [literal|encoded]
	//gsoap <ns> service method-action: <methodName> <actionString>
	//gsoap <ns> service method-documentation: <methodName> <documentation>

	and type schema properties:

	//gsoap <ns> schema namespace: <schemaNamespaceURI>
	//gsoap <ns> schema elementForm: [qualified|unqualified]
	//gsoap <ns> schema attributeForm: [qualified|unqualified]
	//gsoap <ns> schema documentation: <documentationText>
	//gsoap <ns> schema type-documentation: <typeName> <documentationText>

	where <ns> is an XML namespace prefix, which is used in C/C++ operation
	names, e.g. ns__add(), and type names, e.g. xsd__int.

	See the documentation for the full list of //gsoap directives.

--------------------------------------------------------------------------------
gSOAP XML Web services tools
Copyright (C) 2001-2008, Robert van Engelen, Genivia, Inc. All Rights Reserved.
This software is released under one of the following two licenses:
GPL or Genivia's license for commercial use.
--------------------------------------------------------------------------------
GPL license.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

Author contact information:
engelen@genivia.com / engelen@acm.org
--------------------------------------------------------------------------------
A commercial use license is available from Genivia, Inc., contact@genivia.com
--------------------------------------------------------------------------------
*/

//gsoap ns service name:  BFCPService
//gsoap ns service protocol:  SOAP
//gsoap ns service style: rpc
//gsoap ns service encoding:  encoded
//gsoap ns service namespace: http://localhost:8011/BFCPService.wsdl
//gsoap ns service location:  http://localhost:8011

//gsoap ns schema namespace:	urn:BFCPService

#import "stlvector.h"

typedef std::string xsd__string;

enum ns__ErrorCode 
{
  kNoError,
  kUserNotExist,
  kUserAlreadyExist,
  kFloorNotExist,
  kFloorAlreadyExist,
  kChairNotExost,
  kChairAlreadyExist,
  kConferenceNotExist,
  kConferenceAlreadyExist,
  kServerNotStart,
  kServerAlreadyStart,
};

enum ns__Policy 
{
  kAutoAccept,
  kAutoDeny,
};

enum ns__AddrFamily 
{
  kIPv4,
  kIPv6
};

class ns__ConferenceListResult
{
public:
  enum ns__ErrorCode errorCode;
  std::vector<unsigned int> conferenceIDs;
};

class ns__ConferenceInfoResult
{
public:
  enum ns__ErrorCode errorCode;
  xsd__string conferenceInfo;
};

//gsoap ns service method-documentation: start the bfcp server
int ns__start(enum ns__AddrFamily af,
              unsigned short port,
              bool enbaleConnectionThread,
              int workThreadNum,
              double userObsoletedTime,
              enum ns__ErrorCode *errorCode);

//gsoap ns service method-documentation: stop the bfcp server
int ns__stop(enum ns__ErrorCode *errorCode);

//gsoap ns service method-documentation: quit the bfcp server and web service
int ns__quit(void);

//gsoap ns service method-documentation: add conference
int ns__addConference(unsigned int conferenceID,
                      unsigned short maxFloorRequest,
                      enum ns__Policy policy,
                      double timeForChairAction,
                      enum ns__ErrorCode *errorCode);

//gsoap ns service method-documentation: remove conference by ID
int ns__removeConference(unsigned int conferenceID, enum ns__ErrorCode *errorCode);

//gsoap ns service method-documentation: modify conference by ID
int ns__modifyConference(unsigned int conferenceID,
                         unsigned short maxFloorRequest,
                         enum ns__Policy policy,
                         double timeForChairAction,
                         enum ns__ErrorCode *errorCode);

//gsoap ns service method-documentation: add a floor to the conference
int ns__addFloor(unsigned int conferenceID, 
                 unsigned short floorID,
                 unsigned short maxGrantedNum,
                 double maxHoldingTime,
                 enum ns__ErrorCode *errorCode);

//gsoap ns service method-documentation: remove a floor from the conference
int ns__removeFloor(unsigned int conferenceID, 
                    unsigned short floorID,
                    enum ns__ErrorCode *errorCode);

//gsoap ns service method-documentation: change the max granted num of a floor
int ns__modifyFloor(unsigned int conferenceID, 
                    unsigned short floorID,
                    unsigned short maxGrantedNum,
                    double maxHoldingTime,
                    enum ns__ErrorCode *errorCode);

//gsoap ns service method-documentation: add a user to the conference
int ns__addUser(unsigned int conferenceID, 
                unsigned short userID,
                xsd__string userName,
                xsd__string userURI,
                enum ns__ErrorCode *errorCode);

//gsoap ns service method-documentation: remove a user from the conference
int ns__removeUser(unsigned int conferenceID, 
                   unsigned short userID,
                   enum ns__ErrorCode *errorCode);


//gsoap ns service method-documentation: remove a user from the conference
int ns__setChair(unsigned int conferenceID,
                 unsigned short floorID,
                 unsigned short userID,
                 enum ns__ErrorCode *errorCode);

//gsoap ns service method-documentation: remove a user from the conference
int ns__removeChair(unsigned int conferenceID,
                    unsigned short floorID,
                    enum ns__ErrorCode *errorCode);

//gsoap ns service method-documentation: get the conference list
int ns__getConferenceIDs(ns__ConferenceListResult *result);

//gsoap ns service method-documentation: get the conference information
int ns__getConferenceInfo(unsigned int conferenceID,
                          ns__ConferenceInfoResult *result);
