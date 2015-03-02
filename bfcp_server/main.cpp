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

using namespace muduo;
using namespace muduo::net;
using namespace bfcp;

void controlFunc(BaseServer *server);

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
  InetAddress listenAddr(AF_INET, 7890);
  printf("hostport: %s\n", listenAddr.toIpPort().c_str());
  EventLoop loop;
  BaseServer server(&loop, listenAddr);
  Thread thread(boost::bind(&controlFunc, &server));
  thread.start();
  loop.loop();
  thread.join();
  
  return 0;
}

void handleCallResult(ControlError err)
{
  printf("call result: %d\n", err);
}

void printMenu()
{
  printf(
    "\n--------CONFERENCE SERVER-----------------------------------\n"
    " ?      - Show the menu\n"
    " c      - Start the FCS\n"
    " y      - Stop the FCS\n"
    " i      - Insert a new conference\n"
    " d      - Delete a conference\n"
    " a      - Add a new floor\n"
    " f      - Delete a floor\n"
    " r      - Add a floor chair\n"
    " g      - Delete a floor chair\n"
    " j      - Add a new user\n"
    " k      - Delete a user\n"
    " w      - Change number of users that can be granted the same floor at the same time\n"
    " b      - Change the chair policy\n"
    " u      - Change maximum number of requests a user can make for the same floor\n"
    " s      - Show the conferences in the BFCP server\n"
    " q      - Quit\n"
    " p      - Preset Conference\n"
    "--------------------------------------------------------------\n\n");
}

void controlFunc(BaseServer *server)
{
  printMenu();
  char ch;
  while (std::cin >> ch)
  {
    switch (ch)
    {
    case '?': 
      printMenu(); break;
    case 'c':
      {
        printf("Enable extra thread for bfcp connection: 0 = disable, 1 = enable\n");
        bool enableConnectionThread = false;
        CHECK_CIN_RESULT(std::cin >> enableConnectionThread);
        if (enableConnectionThread)
        {
          server->enableConnectionThread();
        }
        printf("Enter the worker thread num:\n");
        int threadNum = 0;
        CHECK_CIN_RESULT(std::cin >> threadNum);
        server->setWorkerThreadNum(threadNum);
        server->start(); 
      } break;
    case 'y': 
      server->stop(); break;
    case 'i':
      {
        printf("Enter the desired ConferenceID for the conference:\n");
        uint32_t conferenceID = 0;
        CHECK_CIN_RESULT(std::cin >> conferenceID);
        printf("Enter the maximum number of requests a user can make for the same floor:\n");
        uint16_t maxFloorRequest = 0;
        CHECK_CIN_RESULT(std::cin >> maxFloorRequest);
        printf("Automated policy when chair is missing:\n\t0 = accept the request / 1 = don't\n");
        bool autoReject = 0;
        CHECK_CIN_RESULT(std::cin >> autoReject);
        printf("Time in seconds the system will wait for a ChairAction: (0 for default number)\n");
        double timeForChairActionInSec = 5.0;
        CHECK_CIN_RESULT(std::cin >> timeForChairActionInSec);
        server->addConference(
          conferenceID, 
          maxFloorRequest, 
          autoReject ? AcceptPolicy::kAutoDeny : AcceptPolicy::kAutoAccept,
          timeForChairActionInSec,
          &handleCallResult);
      } break;
    case 'd':
      {
        printf("Enter the conference you want to remove:\n");
        uint32_t conferenceID = 0;
        CHECK_CIN_RESULT(std::cin >> conferenceID);
        server->removeConference(conferenceID, &handleCallResult);
      } break;
    case 'u':
      {
        printf("Enter the conference you want to modify:\n");
        uint32_t conferenceID = 0;
        CHECK_CIN_RESULT(std::cin >> conferenceID);
        printf("Enter the maximum number of requests a user can make for the same floor:\n");
        uint16_t maxFloorRequest = 3;
        CHECK_CIN_RESULT(std::cin >> maxFloorRequest);
        server->changeMaxFloorRequest(conferenceID, maxFloorRequest, &handleCallResult);
      } break;
    case 'b':
      {
        printf("Enter the conference you want to modify:\n");
        uint32_t conferenceID = 0;
        CHECK_CIN_RESULT(std::cin >> conferenceID);
        printf("Automated policy when chair is missing:\n\t0 = accept the request / 1 = don't\n");
        bool autoReject = 0;
        CHECK_CIN_RESULT(std::cin >> autoReject);
        printf("Time in seconds the system will wait for a ChairAction: (0 for default number)\n");
        double timeForChairActionInSec = 5.0;
        CHECK_CIN_RESULT(std::cin >> timeForChairActionInSec);
        server->changeAcceptPolicy(
          conferenceID, 
          autoReject ? AcceptPolicy::kAutoDeny : AcceptPolicy::kAutoAccept,
          timeForChairActionInSec,
          &handleCallResult);
      } break;
    case 'a':
      {
        printf("Enter the conference you want to add a new floor to:\n");
        uint32_t conferenceID = 0;
        CHECK_CIN_RESULT(std::cin >> conferenceID);
        printf("Enter the desired FloorID:\n");
        uint16_t floorID = 0;
        CHECK_CIN_RESULT(std::cin >> floorID);
        printf("Enter the maximum number of users that can be granted this floor at the same time\n\t(0 for unlimited users):\n");
        uint16_t maxGrantedNum = 0;
        CHECK_CIN_RESULT(std::cin >> maxGrantedNum);
        server->addFloor(
          conferenceID, 
          floorID, 
          maxGrantedNum, 
          &handleCallResult);
      } break;
    case 'f':
      {
        printf("Enter the conference you want to remove the floor from:\n");
        uint32_t conferenceID = 0;
        CHECK_CIN_RESULT(std::cin >> conferenceID);
        printf("Enter the FloorID you want to remove:\n");
        uint16_t floorID = 0;
        CHECK_CIN_RESULT(std::cin >> floorID);
        server->removeFloor(conferenceID, floorID, &handleCallResult);
      } break;
    case 'j':
      {
        printf("Enter the conference you want to add a new user to:\n");
        uint32_t conferenceID = 0;
        CHECK_CIN_RESULT(std::cin >> conferenceID);
        printf("Enter the UserID:\n");
        UserInfoParam user;
        CHECK_CIN_RESULT(std::cin >> user.id);
        printf("Enter the user's URI (e.g. bob@example.com):\n");
        CHECK_CIN_RESULT(std::cin >> user.useruri);
        printf("Enter the user's display name:\n");
        CHECK_CIN_RESULT(std::cin >> user.username);
        server->addUser(conferenceID, user, &handleCallResult);
      } break;
    case 'k':
      {
        printf("Enter the conference you want to remove the user from:\n");
        uint32_t conferenceID = 0;
        CHECK_CIN_RESULT(std::cin >> conferenceID);
        printf("Enter the UserID:\n");
        uint16_t userID;
        CHECK_CIN_RESULT(std::cin >> userID);
        server->removeUser(conferenceID, userID, &handleCallResult);
      } break;
    case 'r':
      {
        printf("Enter the conference you want to add a new chair to:\n");
        uint32_t conferenceID = 0;
        CHECK_CIN_RESULT(std::cin >> conferenceID);
        printf("Enter the FloorID this new chair will manage:\n");
        uint16_t floorID = 0;
        CHECK_CIN_RESULT(std::cin >> floorID);
        printf("Enter the UserID of the chair (ChairID):\n");
        uint16_t userID = 0;
        CHECK_CIN_RESULT(std::cin >> userID);
        server->removeUser(conferenceID, userID, &handleCallResult);
      } break;
    case 'g':
      {
        printf("Enter the conference you want to remove a chair from:\n");
        uint32_t conferenceID = 0;
        CHECK_CIN_RESULT(std::cin >> conferenceID);
        printf("Enter the FloorID the chair is currently handling:\n");
        uint16_t floorID = 0;
        CHECK_CIN_RESULT(std::cin >> floorID);
        server->removeChair(conferenceID, floorID, &handleCallResult);
      } break;
    case 's':
      {
        // TODO:
        printf("TODO: \n");
      } break;
    case 'q':
      printf("Quit\n");
      server->stop();
      server->getLoop()->quit();
      return;
    case 'p':
      {
        printf("Preset Conference\n");
        server->addConference(100, 3, AcceptPolicy::kAutoAccept, 5.0, &handleCallResult);
        UserInfoParam user;
        user.id = 1;
        server->addUser(100, user, &handleCallResult);
        server->addFloor(100, 10, 1, &handleCallResult);
        user.id = 2;
        server->addUser(100, user, &handleCallResult);
        server->addChair(100, 10, 2, &handleCallResult);
      } break;
    default:
      printf("Invalid menu choice - try again\n");
      break;
    }
  }
}
