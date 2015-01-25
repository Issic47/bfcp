#include <utility>
#include <stdio.h>
//#include <unistd.h>

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>

#include <bfcp/common/bfcp_buf_pool.h>
#include <bfcp/server/bfcp_server.h>

using namespace muduo;
using namespace muduo::net;
using namespace bfcp;

int main(int argc, char* argv[])
{
  Logger::setLogLevel(Logger::kTRACE);
  LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid();
  EventLoop loop;
  InetAddress listenAddr(AF_INET, 7890);
  printf("hostport: %s\n", listenAddr.toIpPort().c_str());
  BfcpServer server(&loop, listenAddr);

  server.start();

  loop.loop();
}

