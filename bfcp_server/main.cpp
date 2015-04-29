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
  printf("call result: %d\n", static_cast<int>(err));
}

void handleGetConferenceInfoResult(ControlError err, void *data)
{
  printf("call result: %d\n", static_cast<int>(err));
  if (err == ControlError::kNoError)
  {
    bfcp::string *res = static_cast<bfcp::string*>(data);
    printf("Conference Info: \n%s\n", res->c_str());
  }
}

void handleGetConferenceIDsResult(ControlError err, void *data)
{
  printf("call result: %d\n", static_cast<int>(err));
  if (err == ControlError::kNoError)
  {
    BaseServer::ConferenceIDList *res = static_cast<BaseServer::ConferenceIDList*>(data);
    printf("Conference Info: ");
    for (auto conferenceID : *res)
    {
      printf("%u ", conferenceID);
    }
    printf("\n");
  }
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
    " w      - Modify a floor\n"
    " f      - Delete a floor\n"
    " r      - Add a floor chair\n"
    " g      - Delete a floor chair\n"
    " j      - Add a new user\n"
    " k      - Delete a user\n"
    " b      - Change the chair policy\n"
    " m      - modify the conference\n"
    " s      - Show the conferences in the BFCP server\n"
    " e      - Get all conference IDs in FCS\n"
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
        printf("Enter the user obsoleted time:\n");
        double userObsoletedTime = 0.0;
        CHECK_CIN_RESULT(std::cin >> userObsoletedTime);
        server->setUserObsoleteTime(userObsoletedTime);
        server->start(); 
      } break;
    case 'y': 
      server->stop(); break;
    case 'i':
      {
        printf("Enter the desired ConferenceID for the conference:\n");
        uint32_t conferenceID = 0;
        CHECK_CIN_RESULT(std::cin >> conferenceID);
        bfcp::ConferenceConfig config;
        printf("Enter the maximum number of requests a user can make for the same floor:\n");
        CHECK_CIN_RESULT(std::cin >> config.maxFloorRequest);
        
        printf("Automated policy when chair is missing:\n\t0 = accept the request / 1 = don't\n");
        bool autoReject = 0;
        CHECK_CIN_RESULT(std::cin >> autoReject);
        config.acceptPolicy = autoReject ? AcceptPolicy::kAutoDeny : AcceptPolicy::kAutoAccept;

        printf("Time in seconds the system will wait for a ChairAction: \n");
        CHECK_CIN_RESULT(std::cin >> config.timeForChairAction);
        server->addConference(conferenceID, config, &handleCallResult);
      } break;
    case 'd':
      {
        printf("Enter the conference you want to remove:\n");
        uint32_t conferenceID = 0;
        CHECK_CIN_RESULT(std::cin >> conferenceID);
        server->removeConference(conferenceID, &handleCallResult);
      } break;
    case 'm':
      {
        printf("Enter the conference you want to modify:\n");
        uint32_t conferenceID = 0;
        CHECK_CIN_RESULT(std::cin >> conferenceID);
        bfcp::ConferenceConfig config;
        printf("Enter the maximum number of requests a user can make for the same floor:\n");
        CHECK_CIN_RESULT(std::cin >> config.maxFloorRequest);

        printf("Automated policy when chair is missing:\n\t0 = accept the request / 1 = don't\n");
        bool autoReject = 0;
        CHECK_CIN_RESULT(std::cin >> autoReject);
        config.acceptPolicy = autoReject ? AcceptPolicy::kAutoDeny : AcceptPolicy::kAutoAccept;

        printf("Time in seconds the system will wait for a ChairAction: \n");
        CHECK_CIN_RESULT(std::cin >> config.timeForChairAction);
        server->modifyConference(conferenceID, config, &handleCallResult);
      } break;
    case 'a':
      {
        printf("Enter the conference you want to add a new floor to:\n");
        uint32_t conferenceID = 0;
        CHECK_CIN_RESULT(std::cin >> conferenceID);
        printf("Enter the desired FloorID:\n");
        uint16_t floorID = 0;
        CHECK_CIN_RESULT(std::cin >> floorID);
        bfcp::FloorConfig config;
        printf("Enter the maximum number of users that can be granted this floor at the same time\n\t(0 for unlimited users):\n");
        CHECK_CIN_RESULT(std::cin >> config.maxGrantedNum);
        printf("Enter the maximum holding time\n\t(< 0 for unlimited time):\n");
        CHECK_CIN_RESULT(std::cin >> config.maxHoldingTime);
        server->addFloor(conferenceID, floorID, config, &handleCallResult);
      } break;
    case 'w':
      {
        printf("Enter the conference you want to modify the floor in:\n");
        uint32_t conferenceID = 0;
        CHECK_CIN_RESULT(std::cin >> conferenceID);
        printf("Enter the desired FloorID:\n");
        uint16_t floorID = 0;
        CHECK_CIN_RESULT(std::cin >> floorID);
        bfcp::FloorConfig config;
        printf("Enter the maximum number of users that can be granted this floor at the same time\n\t(0 for unlimited users):\n");
        CHECK_CIN_RESULT(std::cin >> config.maxGrantedNum);
        printf("Enter the maximum holding time\n\t(< 0 for unlimited time):\n");
        CHECK_CIN_RESULT(std::cin >> config.maxHoldingTime);
        server->modifyFloor(conferenceID, floorID, config, &handleCallResult);
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
        printf("Enter the conference you want to get info from: \n");
        uint32_t conferenceID = 0;
        CHECK_CIN_RESULT(std::cin >> conferenceID);
        server->getConferenceInfo(conferenceID, &handleGetConferenceInfoResult);
      } break;
    case 'e':
      {
        server->getConferenceIDs(&handleGetConferenceIDsResult);
      } break;
    case 'q':
      printf("Quit\n");
      server->stop();
      server->getLoop()->quit();
      return;
    case 'p':
      {
        printf("Preset Conference\n");
        ConferenceConfig config;
        config.maxFloorRequest = 3;
        config.acceptPolicy = AcceptPolicy::kAutoAccept;
        config.timeForChairAction = 60.0;
        server->addConference(100, config, &handleCallResult);

        FloorConfig floorConfig;
        floorConfig.maxGrantedNum = 1;
        floorConfig.maxHoldingTime = -1.0;
        server->addFloor(100, 10, floorConfig, &handleCallResult);
        floorConfig.maxGrantedNum = 2;
        server->addFloor(100, 11, floorConfig, &handleCallResult);
        floorConfig.maxGrantedNum = 1;
        floorConfig.maxHoldingTime = 20.0;
        server->addFloor(100, 12, floorConfig, &handleCallResult);
        floorConfig.maxGrantedNum = 1;
        floorConfig.maxHoldingTime = -1.0;
        server->addFloor(100, 13, floorConfig, &handleCallResult);

        UserInfoParam user;
        user.id = 1;
        server->addUser(100, user, &handleCallResult);

        // user 2 is chair of floor 10 and floor 11
        user.id = 2;
        server->addUser(100, user, &handleCallResult);
        server->setChair(100, 10, 2, &handleCallResult);
        server->setChair(100, 11, 2, &handleCallResult);

        // user 3 is chair of floor 13
        user.id = 3;
        server->addUser(100, user, &handleCallResult);
        server->setChair(100, 13, 3, &handleCallResult);

      } break;
    default:
      printf("Invalid menu choice - try again\n");
      break;
    }
  }
}
