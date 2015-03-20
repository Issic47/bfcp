#include <stdio.h>
//#include <unistd.h>

#include <utility>
#include <iostream>

#include <boost/bind.hpp>
#include <boost/scoped_ptr.hpp>

#include <muduo/base/Logging.h>
#include <muduo/base/Thread.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>

#include <bfcp/client/base_client.h>

using namespace muduo;
using namespace muduo::net;
using namespace bfcp;

void controlFunc(EventLoop *loop);
void onClientStateChanged(BaseClient::State state);
void onReceivedResponse(BaseClient::Error error, bfcp_prim prim, void *data);

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
  
  EventLoop loop;
  Thread thread(boost::bind(&controlFunc, &loop));
  thread.start();

  loop.loop();
  thread.join();
}

void printMenu()
{
  printf(
    "\n--------PARTICIPANT LIST-----------------------------\n"
    " ?      - Show the menu\n"
    " c      - Create the Participant\n"
    " h      - Destroy the Participant\n"
    " s      - Show the information of the conference\n"
    "BFCP Messages:\n"
    " f      - Hello\n"
    " r      - FloorRequest\n"
    " l      - FloorRelease\n"
    " o      - FloorRequestQuery\n"
    " u      - UserQuery\n"
    " e      - FloorQuery\n"
    " a      - ChairAction\n"
    " q      - quit\n"
    "------------------------------------------------------\n\n");
}

void resetClient(boost::shared_ptr<BaseClient> client)
{
  client = nullptr;
}

void controlFunc(EventLoop *loop)
{
  boost::shared_ptr<BaseClient> client;
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
          if (client)
          {
            printf("Error: A participant already exist\n");
            break;
          }
          printf("Enter the conferenceID of the conference:\n");
          uint32_t conferenceID;
          CHECK_CIN_RESULT(std::cin >> conferenceID);
          printf("Enter the userID of the participant:\n");
          uint16_t userID;
          CHECK_CIN_RESULT(std::cin >> userID);
          InetAddress serverAddr(AF_INET, "127.0.0.1", 7890);
          printf("connect to hostport:%s\n", serverAddr.toIpPort().c_str());
          client.reset(new BaseClient(loop, serverAddr, conferenceID, userID, 10.0));
          client->setStateChangedCallback(&onClientStateChanged);
          client->setResponseReceivedCallback(&onReceivedResponse);
          client->connect();
          printf("BFCP Participant created.\n");
        } break;
      case 'h':
        {
          if (!client)
          {
            printf("Error: No participant exist\n");
            break;
          }
          client->forceDisconnect();
          // client must destruct in event loop
          //loop->runInLoop(boost::bind(&boost::shared_ptr<BaseClient>::reset, client));
          loop->runInLoop(boost::bind(resetClient, client));
          client = nullptr;
          printf("BFCP participant destroyed.\n");
        } break;
      case 'f':
        if (!client)
        {
          printf("Error: No participant exist\n");
          break;
        }
        client->sendHello();
        break;
      
      case 'r':
        {
          if (!client)
          {
            printf("Error: No participant exist\n");
            break;
          }
          FloorRequestParam param;
          printf("Request for beneficiary user?: 0=false, 1=true\n");
          CHECK_CIN_RESULT(std::cin >> param.hasBeneficiaryID);
          if (param.hasBeneficiaryID)
          {
            printf("Enter the BeneficiaryID of the new request:\n");
            CHECK_CIN_RESULT(std::cin >> param.beneficiaryID);
          }
          printf("Enter the priority for this request (0=lowest --> 4=highest):\n");
          int priority = 0;
          CHECK_CIN_RESULT(std::cin >> priority);
          param.priority = static_cast<bfcp_priority>(priority);
          printf("Enter the FloorID:\n");
          uint16_t floorID = 0;
          CHECK_CIN_RESULT(std::cin >> floorID);
          param.floorIDs.push_back(floorID);
          printf("Enter the rest floors count:\n");
          size_t restCount = 0;
          CHECK_CIN_RESULT(std::cin >> restCount);
          printf("Enter the rest floorIDs:\n");
          for (size_t i = 0; i < restCount; ++i)
          {
            CHECK_CIN_RESULT(std::cin >> floorID);
            param.floorIDs.push_back(floorID);
          }
          client->sendFloorRequest(param);
          printf("Message sent!\n");
        } break;

      case 'l':
        {
          if (!client)
          {
            printf("Error: No participant exist\n");
            break;
          }
          printf("Enter the FloorRequestID of the request you want to release:\n");
          uint16_t floorRequestID;
          CHECK_CIN_RESULT(std::cin >> floorRequestID);
          client->sendFloorRelease(floorRequestID);
          printf("Message send!\n");
        } break;

      case 'o':
        {
          if (!client)
          {
            printf("Error: No participant exist\n");
            break;
          }
          printf("Enter the FloorRequestID of the request you're interested in:\n");
          uint16_t floorRequestID = 0;
          CHECK_CIN_RESULT(std::cin >> floorRequestID);
          client->sendFloorRequestQuery(floorRequestID);
          printf("Message send!\n");
        } break;

      case 'u':
        {
          if (!client)
          {
            printf("Error: No participant exist\n");
            break;
          }
          UserQueryParam param;
          printf("Query the beneficiary user?: 0=false, 1=true\n");
          CHECK_CIN_RESULT(std::cin >> param.hasBeneficiaryID);
          if (param.hasBeneficiaryID)
          {
            printf("Enter the BeneficiaryID if you want information about a specific user:\n");
            CHECK_CIN_RESULT(std::cin >> param.beneficiaryID);
          }
          client->sendUserQuery(param);
          printf("Message send!\n");
        } break;

      case 'e':
        {
          if (!client)
          {
            printf("Error: No participant exist\n");
            break;
          }
          bfcp_floor_id_list floorIDs;
          printf("Enter the floorID count:\n");
          size_t floorIDCount = 0;
          CHECK_CIN_RESULT(std::cin >> floorIDCount);
          printf("Enter the FloorIDs:\n");
          uint16_t floorID = 0;
          for (size_t i = 0; i < floorIDCount; ++i)
          {
            CHECK_CIN_RESULT(std::cin >> floorID);
            floorIDs.push_back(floorID);
          }
          client->sendFloorQuery(floorIDs);
          printf("Message send!\n");
        } break;

      case 'a':
        {
          if (!client)
          {
            printf("Error: No participant exist\n");
            break;
          }
          FloorRequestInfoParam param;
          printf("Enter the FloorRequestID:\n");
          CHECK_CIN_RESULT(std::cin >> param.floorRequestID);

          param.valueType |= AttrValueType::kHasOverallRequestStatus;
          param.oRS.floorRequestID = param.floorRequestID;
          param.oRS.hasRequestStatus = true;
          printf("What to do with this request? (2=Accept / 4=Deny / 7=Revoke):\n");
          int status;
          CHECK_CIN_RESULT(std::cin >> status);
          param.oRS.requestStatus.status = static_cast<bfcp_reqstat>(status);

          printf("Enter the desired position in queue for this request (0 if you are not interested in that):\n");
          int pos = 0;
          CHECK_CIN_RESULT(std::cin >> pos);
          param.oRS.requestStatus.qpos = static_cast<uint8_t>(pos);

          FloorRequestStatusParam floorStatus;
          printf("Enter the FloorID:\n");
          CHECK_CIN_RESULT(std::cin >> floorStatus.floorID);
          param.fRS.push_back(floorStatus);

          printf("Enter the rest floors count:\n");
          size_t restCount = 0;
          CHECK_CIN_RESULT(std::cin >> restCount);
          printf("Enter the rest floorIDs:\n");
          for (size_t i = 0; i < restCount; ++i)
          {
            CHECK_CIN_RESULT(std::cin >> floorStatus.floorID);
            param.fRS.push_back(floorStatus);
          }
          
          client->sendChairAction(param);
          printf("Message send!\n");
        } break;

      case 's':
        // TODO: 
        printf("TODO:\n");
        break;

      case 'q':
        printf("Quit\n");
        if (client)
        {
          client->forceDisconnect();
          //loop->runInLoop(boost::bind(&boost::shared_ptr<BaseClient>::reset, client));
          loop->runInLoop(boost::bind(resetClient, client));
          client = nullptr;
        }
        loop->quit();
        return;

      default:
        printf("Invalid menu choice - try again\n");
        break;
    }
  }
}

void onClientStateChanged( BaseClient::State state )
{

}

void onReceivedResponse( BaseClient::Error error, bfcp_prim prim, void *data )
{

}
