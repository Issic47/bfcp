#ifndef BFCP_SERVICE_H
#define BFCP_SERVICE_H

// NOTE: the following definition should be put in the soapBfcpServiceService.h
//#ifndef WITH_PURE_VIRTUAL
//#define WITH_PURE_VIRTUAL
//#endif

#include <boost/shared_ptr.hpp>
#include <muduo/base/Mutex.h>
#include <muduo/base/Condition.h>
#include <muduo/net/EventLoopThread.h>
#include <bfcp/server/base_server.h>

#include "soapBFCPServiceService.h"

class BfcpService : public BFCPServiceService
{
public:
  BfcpService();
  /// Construct from another engine state
  BfcpService(const struct soap &soap);
  /// Constructor with engine input+output mode control
  BfcpService(soap_mode iomode);
  /// Constructor with engine input and output mode control
  BfcpService(soap_mode imode, soap_mode omode);
  /// Destructor, also frees all deserialized data
  virtual ~BfcpService();

  int run(int port) override;

  BFCPServiceService * copy() override;

  int start(
    enum ns__AddrFamily af,
    unsigned short port, bool enbaleConnectionThread, 
    int workThreadNum, 
    double userObsoletedTime, 
    enum ns__ErrorCode *errorCode) override;
  
  int stop(enum ns__ErrorCode *errorCode) override;

  int quit() override;
  
  int addConference(
    unsigned int conferenceID, 
    unsigned short maxFloorRequest, 
    enum ns__Policy policy, 
    double timeForChairAction, 
    enum ns__ErrorCode *errorCode) override;

  int removeConference(
    unsigned int conferenceID, 
    enum ns__ErrorCode *errorCode) override;

  int modifyConference(
    unsigned int conferenceID, 
    unsigned short maxFloorRequest, 
    enum ns__Policy policy, 
    double timeForChairAction, 
    enum ns__ErrorCode *errorCode) override;

  int addFloor(
    unsigned int conferenceID, 
    unsigned short floorID, 
    unsigned short maxGrantedNum, 
    double maxHoldingTime,
    enum ns__ErrorCode *errorCode) override;

  int removeFloor(
    unsigned int conferenceID, 
    unsigned short floorID, 
    enum ns__ErrorCode *errorCode) override;

  int modifyFloor(
    unsigned int conferenceID, 
    unsigned short floorID, 
    unsigned short maxGrantedNum, 
    double maxHoldingTime,
    enum ns__ErrorCode *errorCode) override;
  
  int addUser(
    unsigned int conferenceID, 
    unsigned short userID, 
    std::string userName, 
    std::string userURI, 
    enum ns__ErrorCode *errorCode) override;

  int removeUser(
    unsigned int conferenceID, 
    unsigned short userID, 
    enum ns__ErrorCode *errorCode) override;

  int setChair(
    unsigned int conferenceID, 
    unsigned short floorID, 
    unsigned short userID, 
    enum ns__ErrorCode *errorCode) override;

  int removeChair(
    unsigned int conferenceID, 
    unsigned short floorID, 
    enum ns__ErrorCode *errorCode) override;

  int getConferenceIDs(ns__ConferenceListResult *result) override;

  int getConferenceInfo(
    unsigned int conferenceID, 
    ns__ConferenceInfoResult *result) override;

private:
  typedef boost::shared_ptr<bfcp::BaseServer> BaseServerPtr;
  typedef std::vector<unsigned int> ConferenceIDList;
  static void resetServer(BaseServerPtr server);
  void handleCallResult(bfcp::ControlError error);
  void handleGetCoferenceIDsResult(
    ConferenceIDList *ids, bfcp::ControlError error, void *data);
  void handleGetConferenceInfoResult(
    std::string &info, bfcp::ControlError error, void *data);

  BaseServerPtr server_;
  muduo::MutexLock mutex_;
  muduo::Condition cond_;
  muduo::net::EventLoop *loop_;
  muduo::net::EventLoopThread thread_;
  bool callFinished_;
  bfcp::ControlError error_;
  bool isRunning_;
};

#endif // BFCP_SERVICE_H