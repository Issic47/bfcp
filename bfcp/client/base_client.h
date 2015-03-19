#ifndef BFCP_CLIENT_H
#define BFCP_CLIENT_H

#include <unordered_map>

#include <muduo/net/UdpSocket.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/UdpClient.h>

#include <bfcp/common/bfcp_callbacks.h>
#include <bfcp/common/bfcp_param.h>
enum bfcp_prim;

namespace bfcp
{

class BfcpConnection;
typedef boost::shared_ptr<BfcpConnection> BfcpConnectionPtr;

class BasicRequestParam;

class BaseClient : boost::noncopyable
{
public:
  enum State
  {
    kDisconnecting = 0,
    kDisconnected = 1,
    kConnecting = 2, 
    kConnected = 3,
  };

  enum Error
  {
    kNoError = 0,
    kMissingMandatoryAttr = 1,
  };

  typedef boost::function<void (State)> StateChangedCallback;
  // FIXME: use boost::any instead of void*?
  typedef boost::function<void (Error, bfcp_prim, void*)> ResponseReceivedCallback;

  BaseClient(muduo::net::EventLoop* loop, 
             const muduo::net::InetAddress& serverAddr,
             uint32_t conferenceID,
             uint16_t userID);

  muduo::net::EventLoop* getLoop() { return loop_; }

  State getState() const { return state_; }

  void connect();
  // will send Goodbye message and wait for GoodbyeAck
  void disconnect();
  void forceDisconnect();

  void setStateChangedCallback(const StateChangedCallback &cb)
  { stateChangedCallback_ = cb; }

  void setStateChangedCallback(StateChangedCallback &&cb)
  { stateChangedCallback_ = std::move(cb); }

  void setResponseReceivedCallback(const ResponseReceivedCallback &cb)
  { responseReceivedCallback_ = cb; }

  void setResponseReceivedCallback(ResponseReceivedCallback &&cb)
  { responseReceivedCallback_ = std::move(cb); }

  void sendFloorRequest(const FloorRequestParam &floorRequest);
  void sendFloorRelease(uint16_t floorRequestID);
  void sendFloorRequestQuery(uint16_t floorRequestID);
  void sendUserQuery(const UserQueryParam userQuery);
  void sendFloorQuery(const bfcp_floor_id_list &floorIDs);
  void sendChairAction(const FloorRequestInfoParam &frqInfo);  
  void sendHello();
  void sendGoodbye();

private:
  typedef boost::function<void (const BfcpMsg&)> Handler;
  typedef std::unordered_map<int, Handler> HandlerDict;

  void onStartedRecv(const muduo::net::UdpSocketPtr& socket);
  void onMessage(const muduo::net::UdpSocketPtr& socket, 
                 muduo::net::Buffer* buf,
                 const muduo::net::InetAddress &src,
                 muduo::Timestamp time);

  void onWriteComplete(const muduo::net::UdpSocketPtr& socket, int messageId);
  void onNewRequest(const BfcpMsg &msg);
  void onResponse(bfcp_prim requestPrimitive, ResponseError err, const BfcpMsg &msg);

  void initResponseHandlers();
  void handleFloorRequestStatus(const BfcpMsg &msg);
  void handleUserStatus(const BfcpMsg &msg);
  void handleFloorStatus(const BfcpMsg &msg);
  void handleChairAcionAck(const BfcpMsg &msg);
  void handleHelloAck(const BfcpMsg &msg);
  void handleGoodbyeAck(const BfcpMsg &msg);
  void handleError(const BfcpMsg &msg);

  bool checkMsg(const BfcpMsg &msg, bfcp_prim expectedPrimitive) const;
  BasicRequestParam generateBasicParam(bfcp_prim primitive);

  void changeState(State state);
  const char* toString(State state) const;

  muduo::net::EventLoop* loop_;
  muduo::net::UdpClient client_;
  muduo::net::InetAddress serverAddr_;
  BfcpConnectionPtr connection_;
  HandlerDict responseHandlers_;

  uint32_t conferenceID_;
  uint16_t userID_;

  State state_;
  StateChangedCallback stateChangedCallback_;
  ResponseReceivedCallback responseReceivedCallback_;
};

} // namespace bfcp



#endif // BFCP_CLIENT_H