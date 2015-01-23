#include "bfcp_client.h"

#include <boost/bind.hpp>
#include <muduo/base/Logging.h>

#include "common/bfcp_conn.h"

using namespace muduo;
using namespace muduo::net;

namespace bfcp
{

BfcpClient::BfcpClient(muduo::net::EventLoop* loop, 
                       const muduo::net::InetAddress& serverAddr, 
                       uint32_t conferenceID, 
                       uint16_t userID)
    : loop_(loop),
      client_(loop, serverAddr, "BfcpClient"),
      serverAddr_(serverAddr),
      conferenceID_(conferenceID),
      userID_(userID),
      state_(kDisconnected)
{
  client_.setMessageCallback(
    boost::bind(&BfcpClient::onMessage, this, _1, _2, _3, _4));
  client_.setStartedRecvCallback(
    boost::bind(&BfcpClient::onStartedRecv, this, _1));
  client_.setWriteCompleteCallback(
    boost::bind(&BfcpClient::onWriteComplete, this, _1, _2));

  initResponseHandlers();
}

void BfcpClient::initResponseHandlers()
{
  responseHandlers_.insert(std::make_pair(
    BFCP_FLOOR_REQUEST, 
    boost::bind(&BfcpClient::handleFloorRequestStatus, this, _1)));

  responseHandlers_.insert(std::make_pair(
    BFCP_FLOOR_RELEASE,
    boost::bind(&BfcpClient::handleFloorRequestStatus, this, _1)));

  responseHandlers_.insert(std::make_pair(
    BFCP_FLOOR_REQUEST_QUERY,
    boost::bind(&BfcpClient::handleFloorRequestStatus, this, _1)));

  responseHandlers_.insert(std::make_pair(
    BFCP_USER_QUERY,
    boost::bind(&BfcpClient::handleUserStatus, this, _1)));

  responseHandlers_.insert(std::make_pair(
    BFCP_FLOOR_QUERY,
    boost::bind(&BfcpClient::handleFloorStatus, this, _1)));

  responseHandlers_.insert(std::make_pair(
    BFCP_CHAIR_ACTION,
    boost::bind(&BfcpClient::handleChairAcionAck, this, _1)));

  responseHandlers_.insert(std::make_pair(
    BFCP_HELLO, 
    boost::bind(&BfcpClient::handleHelloAck, this, _1)));

  responseHandlers_.insert(std::make_pair(
    BFCP_GOODBYE,
    boost::bind(&BfcpClient::handleGoodbyeAck, this, _1)));
}

void BfcpClient::connect()
{
  if (state_ == kDisconnected)
  {
    client_.connect();
    changeState(kConnecting);
  }
}

void BfcpClient::disconnect()
{
  if (state_ == kConnected)
  {
    changeState(kDisconnecting);
  }
}

void BfcpClient::forceDisconnect()
{
  client_.disconnect();
  changeState(kDisconnected);
}

void BfcpClient::changeState( State state )
{
  if (state_ != state)
  {
    state_ = state;
    LOG_TRACE << "BfcpClient::changeState is " << toString(state);
    if (stateChangedCallback_)
      stateChangedCallback_(state);
  }
}

const char* BfcpClient::toString( State state ) const
{
  switch (state)
  {
    case kDisconnecting: return "disconnecting";
    case kDisconnected: return "disconnected";
    case kConnecting: return "connecting";
    case kConnected: return "connected";
    default: return "???";
  }
}

void BfcpClient::onStartedRecv( const UdpSocketPtr& socket )
{
  LOG_TRACE << socket->getLocalAddr().toIpPort() << " started receiving data from " 
            << socket->getPeerAddr().toIpPort();

  assert(socket == client_.socket());
  if (!connection_) 
  {
    connection_ = boost::make_shared<BfcpConnection>(loop_, client_.socket());
    connection_->setNewRequestCallback(
      boost::bind(&BfcpClient::onNewRequest, this, _1));
  }
  if (state_ == kConnecting)
  {
    sendHello();
  }
}

void BfcpClient::sendFloorRequest( const FloorRequestParam &floorRequest )
{
  assert(client_.isConnected());
  assert(connection_);

  connection_->sendFloorRequest(
    generateBasicParam(BFCP_FLOOR_REQUEST),
    floorRequest);
}

void BfcpClient::sendFloorRelease( uint16_t floorRequestID )
{
  assert(client_.isConnected());
  assert(connection_);
  connection_->sendFloorRelease(
    generateBasicParam(BFCP_FLOOR_RELEASE), 
    floorRequestID);
}

void BfcpClient::sendFloorRequestQuery( uint16_t floorRequestID )
{
  assert(client_.isConnected());
  assert(connection_);

  connection_->sendFloorRequestQuery(
    generateBasicParam(BFCP_FLOOR_REQUEST_QUERY), 
    floorRequestID);
}

void BfcpClient::sendUserQuery( const UserQueryParam userQuery )
{
  assert(client_.isConnected());
  assert(connection_);

  connection_->sendUserQuery(
    generateBasicParam(BFCP_USER_QUERY), 
    userQuery);
}

void BfcpClient::sendFloorQuery( const bfcp_floor_id_list &floorIDs )
{
  assert(client_.isConnected());
  assert(connection_);

  connection_->sendFloorQuery(
    generateBasicParam(BFCP_FLOOR_QUERY), 
    floorIDs);
}

void BfcpClient::sendChairAction( const FloorRequestInfoParam &frqInfo )
{
  assert(client_.isConnected());
  assert(connection_);

  connection_->sendChairAction(
    generateBasicParam(BFCP_CHAIR_ACTION),
    frqInfo);
}

void BfcpClient::sendHello()
{
  assert(client_.isConnected());
  assert(connection_);

  connection_->sendHello(generateBasicParam(BFCP_HELLO));
}

void BfcpClient::sendGoodBye()
{
  assert(client_.isConnected());
  assert(connection_);

  connection_->sendGoodBye(generateBasicParam(BFCP_GOODBYE));
}

BasicRequestParam BfcpClient::generateBasicParam( bfcp_prim primitive )
{
  BasicRequestParam param;
  param.cb = boost::bind(&BfcpClient::onResponse, this, primitive, _1, _2);
  param.conferenceID = conferenceID_;
  param.userID = userID_;
  param.dst = serverAddr_;
  return param;
}

void BfcpClient::onMessage(const UdpSocketPtr& socket, 
                           Buffer* buf, 
                           const InetAddress &src, 
                           Timestamp time)
{
  LOG_TRACE << client_.name() << " recv " << buf->readableBytes() 
            << " bytes at " << time.toString() 
            << " from " << src.toIpPort();

  if (connection_)
  {
    connection_->onMessage(buf, src);
  }
}

void BfcpClient::onWriteComplete( const UdpSocketPtr& socket, int messageId )
{
  LOG_TRACE << "message " << messageId << " write completed";
}

void BfcpClient::onNewRequest( const BfcpMsg &msg )
{
  LOG_TRACE << "BfcpClient received new request " 
            << bfcp_prim_name(msg.primitive());

  switch (msg.primitive())
  {
    case BFCP_FLOOR_STATUS: 
      handleFloorStatus(msg); break;

    case BFCP_FLOOR_REQUEST_STATUS: 
      handleFloorRequestStatus(msg); break;

    default:
      LOG_ERROR << "Ignore unexpected BFCP request message " 
                << msg.primitive();
      break;
  }
}

void BfcpClient::onResponse(bfcp_prim requestPrimitive, 
                            ResponseError err, 
                            const BfcpMsg &msg)
{
  if (err != kNoError)
  {
    LOG_TRACE << "BfcpClient received response with error " 
              << responce_error_name(err);
    changeState(kDisconnected);
  }
  else
  {
    LOG_TRACE << "BfcpClient received response " 
              << bfcp_prim_name(msg.primitive());
    if (msg.primitive() == BFCP_ERROR) 
    {
      handleError(msg);
    }
    else
    {
      auto it = responseHandlers_.find(requestPrimitive);
      assert(it != responseHandlers_.end());
      (*it).second(msg);
    }
  }
}

void BfcpClient::handleFloorRequestStatus( const BfcpMsg &msg )
{
  if (!checkMsg(msg, BFCP_FLOOR_REQUEST_STATUS)) return;

  if (!msg.isResponse())
  {
    connection_->replyWithFloorRequestStatusAck(msg);
  }

  auto attr = msg.findAttribute(BFCP_FLOOR_REQ_INFO);
  if (!attr) 
  {
    LOG_ERROR << "No FLOOR-REQUEST-INFORMATION found in BFCP FloorRequestStatus message";
    // TODO: 
    return;
  }
  else
  {
    BfcpAttr frqInfoAttr(*attr);
    bfcp_floor_request_info info = frqInfoAttr.getFloorRequestInfo();

    FloorRequestInfoParam param;
    param.set(info);

    // TODO: notify info
  }
}

void BfcpClient::handleUserStatus( const BfcpMsg &msg )
{
  assert(msg.isResponse());
  if (!checkMsg(msg, BFCP_USER_STATUS)) return;

  UserStatusParam param;
  auto attr = msg.findAttribute(BFCP_BENEFICIARY_INFO);
  if (attr) 
  {
    BfcpAttr beneficiaryInfoAttr(*attr);
    bfcp_user_info info = beneficiaryInfoAttr.getBeneficiaryInfo();
    param.setBeneficiary(info);
  }

  auto attrs = msg.findAttributes(BFCP_FLOOR_REQ_INFO);
  param.frqInfoList.reserve(attrs.size());
  for (auto &attr : attrs)
  {
    bfcp_floor_request_info frqInfo = attr.getFloorRequestInfo();
    param.addFloorRequestInfo(frqInfo);
  }

  // TODO: notify the user status

}

void BfcpClient::handleFloorStatus( const BfcpMsg &msg )
{
  if (!checkMsg(msg, BFCP_FLOOR_STATUS)) return;

  if (!msg.isResponse()) 
  {
    connection_->replyWithFloorStatusAck(msg);
  }

  auto attr = msg.findAttribute(BFCP_FLOOR_ID);
  if (attr)
  {
    BfcpAttr floorIDAttr(*attr);
    FloorStatusParam param;
    param.floorID = floorIDAttr.getFloorID();

    auto attrs = msg.findAttributes(BFCP_FLOOR_REQ_INFO);
    param.frqInfoList.reserve(attrs.size());
    for (auto &attr : attrs)
    {
      bfcp_floor_request_info info = attr.getFloorRequestInfo();
      param.addFloorRequestInfo(info);
    }
  }
  // TODO: notify floor status
}

void BfcpClient::handleChairAcionAck( const BfcpMsg &msg )
{
  assert(msg.isResponse());
  if (!checkMsg(msg, BFCP_CHAIR_ACTION_ACK)) return;
  // TODO: notify 
}

void BfcpClient::handleHelloAck( const BfcpMsg &msg )
{
  assert(msg.isResponse());
  if (!checkMsg(msg, BFCP_HELLO_ACK)) return;
  
  changeState(kConnected);

  auto attr = msg.findAttribute(BFCP_SUPPORTED_PRIMS);
  if (!attr)
  {
    LOG_ERROR << "No SUPPORTED-PRIMITIVES found in BFCP HelloAck message";
    // TODO:
    return;
  }
  else
  {
    BfcpAttr supportedPrimsAttr(*attr);
    auto &supPrims = supportedPrimsAttr.getSupportedPrims();
    // TODO: check supported primitives
  }

  attr = msg.findAttribute(BFCP_SUPPORTED_ATTRS);
  if (!attr)
  {
    LOG_ERROR << "No SUPPORTED-ATTRIBUTES found in BFCP HelloAck message";
    return;
  }
  else
  {
    BfcpAttr supportedAttrsAttr(*attr);
    auto &supAttrs = supportedAttrsAttr.getSupportedAttrs();
    // TODO: check supported attributes
  }
}

void BfcpClient::handleGoodbyeAck( const BfcpMsg &msg )
{
  assert(msg.isResponse());
  if (!checkMsg(msg, BFCP_GOODBYE_ACK)) return;

  if (state_ == kDisconnecting)
  {
    client_.disconnect();
    changeState(kDisconnected);
  }
}

void BfcpClient::handleError( const BfcpMsg &msg )
{
  assert(msg.isResponse());
  if (!checkMsg(msg, BFCP_ERROR)) return;
  
  ErrorParam param;
  auto attr = msg.findAttribute(BFCP_ERROR_CODE);
  if (!attr)
  {
    LOG_ERROR << "No ERROR-CODE found in BFCP Error message";
    // TODO:
    return;
  }
  else
  {
    BfcpAttr errorCodeAttr(*attr);
    bfcp_errcode errcode = errorCodeAttr.getErrorCode();
    param.errorCode.set(errcode);
  }

  attr = msg.findAttribute(BFCP_ERROR_INFO);
  if (attr)
  {
    BfcpAttr errorInfoAttr(*attr);
    const char *errorInfo = errorInfoAttr.getErrorInfo();
    param.setErrorInfo(errorInfo);
  }
  // TODO: notify error
}

bool BfcpClient::checkMsg( const BfcpMsg &msg, bfcp_prim expectedPrimitive ) const
{
  if (msg.getConferenceID() != conferenceID_)
  {
    LOG_ERROR << "Expected BFCP conference ID is " << conferenceID_
              << " but get " << msg.getConferenceID();
    return false;
  }

  if (msg.getUserID() != userID_)
  {
    LOG_ERROR << "Expected BFCP user ID is " << userID_
              << " but get " << msg.getUserID();
    return false;
  }

  if (msg.primitive() != expectedPrimitive) 
  {
    LOG_ERROR << "Expected BFCP " << bfcp_prim_name(expectedPrimitive) 
              << " but get " << bfcp_prim_name(msg.primitive());
    return false;
  }
  return true;
}

}