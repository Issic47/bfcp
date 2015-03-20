#include <utility>
#include <stdio.h>
#include <iostream>

#include <muduo/base/Logging.h>
#include <muduo/base/TimeZone.h>
#include <muduo/base/AsyncLogging.h>

#include "BfcpService.h"
#include "BFCPService.nsmap"

extern char *basename( char *path );

using namespace muduo;

int kRollSize = 500 * 1000 * 1000;

muduo::AsyncLogging* g_asyncLog = NULL;
void asyncOutput(const char* msg, int len)
{
  g_asyncLog->append(msg, len);
}

int http_get(struct soap *soap);

int main(int argc, char* argv[])
{
  int port = 0;
  if (argc < 2)
  {
    fprintf(stderr, "Usage: bfcp_service_soap <port>\n");
    return 0;
  }
  else
  {
    port = atoi(argv[1]);
    if (!port)
    {
      fprintf(stderr, "Usage: bfcp_service_soap <port>\n");
      return 0;
    }
  }

  sys_coredump_set(true);

  Logger::setLogLevel(Logger::kTRACE);
  Logger::setTimeZone(muduo::TimeZone(8*3600, "CST"));
  
  char name[256];
  strncpy(name, argv[0], 256);
  muduo::AsyncLogging log(::basename(name), kRollSize);
  log.start();
  g_asyncLog = &log;
  muduo::Logger::setOutput(asyncOutput);

  LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid();

  BfcpService service(SOAP_C_UTFSTRING | SOAP_IO_KEEPALIVE);
  service.send_timeout = 20;
  service.recv_timeout = 20;
  service.connect_timeout = 5;
  service.accept_timeout = 5;
  
  /* run iterative server on port until fatal error */
  service.fget = http_get;
  if (service.run(port))
  { 
    char buf[1024];
    service.soap_sprint_fault(buf, sizeof buf);
    buf[sizeof(buf)-1] = '\0';
    LOG_ERROR << buf;
  }

  return 0;
}

int http_get(struct soap * soap)
{ 
  FILE *fd = NULL;
  char *s = strchr(soap->path, '?'); 
  if (!s || strcmp(s, "?wsdl")) 
    return SOAP_GET_METHOD; 
  fd = fopen("BFCPService.wsdl", "rb"); // open WSDL file to copy 
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

