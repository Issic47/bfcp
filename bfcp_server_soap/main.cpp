#include <utility>
#include <stdio.h>
//#include <unistd.h>
#include <iostream>

#include <muduo/base/Logging.h>
#include <muduo/base/Thread.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/EventLoopThread.h>

#include <bfcp/server/base_server.h>

#include "soapBFCPServiceService.h"
#include "BFCPService.nsmap"

using namespace muduo;
using namespace muduo::net;
using namespace bfcp;

void controlFunc(BaseServer *server);
int http_get(struct soap *soap);

#define CHECK_CIN_RESULT(res) do {\
  if (!(res)) {\
  std::cin.clear(); \
  std::cin.ignore(); \
  break; \
  }\
} while (false)

int main(int argc, char* argv[])
{
  Logger::setLogLevel(Logger::kTRACE);
  LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid();

  BFCPServiceService service(SOAP_C_UTFSTRING);
  if (argc < 2)
    service.serve();	/* serve as CGI application */
  else
  { 
    int port = atoi(argv[1]);
    if (!port)
    { 
      fprintf(stderr, "Usage: bfcp_service_soap <port>\n");
      exit(0);
    }
    /* run iterative server on port until fatal error */
    service.fget = http_get;
    if (service.run(port))
    { 
      service.soap_stream_fault(std::cerr);
      exit(-1);
    }
  }
  getchar();

  return 0;
}


int http_get(struct soap * soap)
{ 
  FILE *fd = NULL;
  char *s = strchr(soap->path, '?'); 
  if (!s || strcmp(s, "?wsdl")) 
    return SOAP_GET_METHOD; 
  fd = fopen("calc.wsdl", "rb"); // open WSDL file to copy 
  if (!fd) 
    return 404; // return HTTP not found error 
  soap->http_content = "text/xml"; // HTTP header with text/xml content 
  soap_response(soap, SOAP_FILE); 
  for (;;) 
  { 
    size_t r = fread(soap->tmpbuf, 1, sizeof(soap->tmpbuf), fd); 
    if (!r) 
      break; 
    if (soap_send_raw(soap, soap->tmpbuf, r)) 
      break; // can't send, but little we can do about that 
  } 
  fclose(fd); 
  soap_end_send(soap); 
  return SOAP_OK; 
}

int BFCPServiceService::start(unsigned short port,
                              bool enbaleConnectionThread, 
                              int workThreadNum)
{
  return SOAP_OK;
}

int BFCPServiceService::stop()
{
  return SOAP_OK;
}

int BFCPServiceService::addConference(unsigned int conferenceID, 
                                      unsigned short maxFloorRequest, 
                                      char *policy, 
                                      double timeForChairAction)
{
  return SOAP_OK;
}

int BFCPServiceService::removeConference(unsigned int conferenceID)
{
  return SOAP_OK;
}

int BFCPServiceService::changeMaxFloorRequest(unsigned int conferenceID, 
                                              unsigned short maxFloorRequest)
{
  return SOAP_OK;
}

int BFCPServiceService::changeAcceptPolicy(unsigned int conferenceID, 
                                           char *policy,
                                           double timeForChairActoin)
{
  return SOAP_OK;
}

int BFCPServiceService::addFloor(unsigned int conferenceID, 
                                 unsigned short floorID, 
                                 unsigned short maxGrantedNum)
{
  return SOAP_OK;
}

int BFCPServiceService::removeFloor(unsigned int conferenceID,
                                    unsigned short floorID)
{
  return SOAP_OK;
}

int BFCPServiceService::changeMaxGrantedNum(unsigned int conferenceID, 
                                            unsigned short floorID, 
                                            unsigned short maxGrantedNum)
{
  return SOAP_OK;
}

int BFCPServiceService::addUser(unsigned int conferenceID, 
                                unsigned short userID, 
                                char *userName, 
                                char *userURI)
{
  return SOAP_OK;
}

int BFCPServiceService::removeUser(unsigned int conferenceID, 
                                   unsigned short userID)
{
  return SOAP_OK;
}

int BFCPServiceService::addChair(unsigned int conferenceID, 
                                 unsigned short floorID, 
                                 unsigned short userID)
{
  return SOAP_OK;
}

int BFCPServiceService::removeChair(unsigned int conferenceID, 
                                    unsigned short floorID)
{
  return SOAP_OK;
}

int BFCPServiceService::getConferenceIDs(ns__ConferenceList *result)
{
  return SOAP_OK;
}

int BFCPServiceService::getConferenceInfo( unsigned int conferenceID, char *&conferenceInfo )
{
  return SOAP_OK;
}