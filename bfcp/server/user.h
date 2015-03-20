#ifndef BFCP_USER_H
#define BFCP_USER_H

#include <map>
#include <list>
#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <boost/function.hpp>
#include <muduo/base/Timestamp.h>
#include <muduo/net/InetAddress.h>
#include <bfcp/common/bfcp_param.h>

namespace bfcp
{

class User : boost::noncopyable
{
public:
  typedef boost::function<void ()> SendMessageTask;

  User(uint16_t userID, const string &displayName, const string &uri)
      : userID_(userID),
        displayName_(displayName),
        uri_(uri),
        isAvailable_(false)
  {}

  uint16_t getUserID() const { return userID_; }
  
  void setDisplayName(const string &displayName) { displayName_ = displayName; }
  const string& getDisplayName() const { return displayName_; }
  
  void setURI(const string &uri) { uri_ = uri; }
  const string& getURI() const { return uri_; }

  void setActiveTime(const muduo::Timestamp &timestamp)
  { activeTime_ = timestamp; }
  const muduo::Timestamp& getActiveTime() const 
  { return activeTime_; }

  void setAvailable(bool available) { isAvailable_ = available; }
  bool isAvailable() const { return isAvailable_; }

  void setAddr(const muduo::net::InetAddress &addr) { addr_ = addr; }
  const muduo::net::InetAddress& getAddr() const { return addr_; }

  uint16_t getRequestCountOfFloor(uint16_t floorID) const;
  void addOneRequestOfFloor(uint16_t floorID);
  void removeOneRequestOfFloor(uint16_t floorID);
  void resetRequestCountOfFloor(uint16_t floorID);
  void clearAllRequestCount() { floorRequestCounter_.clear(); }

  UserInfoParam toUserInfoParam() const 
  {
    UserInfoParam param;
    param.id = userID_;
    param.username = displayName_;
    param.useruri = uri_;
    return param;
  }

  void runSendMessageTask(SendMessageTask &&task)
  {
    if (tasks_.empty())
    {
      task();
    }
    tasks_.emplace_back(std::move(task));
  }

  void runSendMessageTask(const SendMessageTask &task)
  {
    if (tasks_.empty())
    {
      task();
    }
    tasks_.emplace_back(task);
  }

  void clearAllSendMessageTasks()
  { tasks_.clear(); }

  void runNextSendMessageTask()
  {
    if (!tasks_.empty())
    {
      tasks_.pop_front();
      if (tasks_.empty()) return;
      tasks_.front()();
    }
  }

private:
  // key: floor ID, value: request count
  typedef std::map<uint16_t, uint16_t> FloorRequestMap;

  uint16_t userID_;
  string displayName_;
  string uri_;
  muduo::net::InetAddress addr_;
  FloorRequestMap floorRequestCounter_;
  std::list<SendMessageTask> tasks_;
  bool isAvailable_;
  muduo::Timestamp activeTime_;
};

typedef boost::shared_ptr<User> UserPtr;
typedef std::map<uint16_t, UserPtr> UserDict;

} // namespace bfcp

#endif // BFCP_USER_H