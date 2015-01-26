#include <stdio.h>
//#include <unistd.h>

#include <utility>

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>

#include <bfcp/client/base_client.h>

using namespace muduo;
using namespace muduo::net;
using namespace bfcp;
using namespace bfcp::client;

int main(int argc, char* argv[])
{
  Logger::setLogLevel(Logger::kTRACE);
  LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid();
  
  EventLoop loop;
  InetAddress serverAddr(AF_INET, "127.0.0.1", 7890);
  printf("connect to hostport:%s\n", serverAddr.toIpPort().c_str());
  BaseClient client(&loop, serverAddr, 100, 1);
  client.connect();

  loop.loop();
}

