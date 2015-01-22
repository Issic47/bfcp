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
      userID_(userID)
{
  client_.setMessageCallback(
    boost::bind(&BfcpClient::onMessage, this, _1, _2, _3, _4));
  client_.setStartedRecvCallback(
    boost::bind(&BfcpClient::onStartedRecv, this, _1));
  client_.setWriteCompleteCallback(
    boost::bind(&BfcpClient::onWriteComplete, this, _1, _2));

  initHandlers();
}

void BfcpClient::initHandlers()
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
  // TODO: notify 
}


void BfcpClient::sendFloorRequest( const FloorRequestParam &floorRequest )
{
  assert(client_.isConnected());
  assert(connection_);

  BasicRequestParam param;
  initBasicParam(param, BFCP_FLOOR_REQUEST);

  connection_->sendFloorRequest(param, floorRequest);
}

void BfcpClient::sendFloorRelease( uint16_t floorRequestID )
{
  assert(client_.isConnected());
  assert(connection_);

  BasicRequestParam param;
  initBasicParam(param, BFCP_FLOOR_RELEASE);

  connection_->sendFloorRelease(param, floorRequestID);
}

void BfcpClient::sendFloorRequestQuery( uint16_t floorRequestID )
{
  assert(client_.isConnected());
  assert(connection_);

  BasicRequestParam param;
  initBasicParam(param, BFCP_FLOOR_REQUEST_QUERY);

  connection_->sendFloorRequestQuery(param, floorRequestID);
}

void BfcpClient::sendUserQuery( const UserQueryParam userQuery )
{
  assert(client_.isConnected());
  assert(connection_);

  BasicRequestParam param;
  initBasicParam(param, BFCP_USER_QUERY);

  connection_->sendUserQuery(param, userQuery);
}

void BfcpClient::sendFloorQuery( const bfcp_floor_id_list &floorIDs )
{
  assert(client_.isConnected());
  assert(connection_);

  BasicRequestParam param;
  initBasicParam(param, BFCP_FLOOR_QUERY);

  connection_->sendFloorQuery(param, floorIDs);
}

void BfcpClient::sendChairAction( const FloorRequestInfoParam &frqInfo )
{
  assert(client_.isConnected());
  assert(connection_);

  BasicRequestParam param;
  initBasicParam(param, BFCP_CHAIR_ACTION);

  connection_->sendChairAction(param, frqInfo);
}

void BfcpClient::sendHello()
{
  assert(client_.isConnected());
  assert(connection_);

  BasicRequestParam param;
  initBasicParam(param, BFCP_HELLO);

  connection_->sendHello(param);
}

void BfcpClient::sendGoodBye()
{
  assert(client_.isConnected());
  assert(connection_);

  BasicRequestParam param;
  initBasicParam(param, BFCP_GOODBYE);

  connection_->sendGoodBye(param);
}

void BfcpClient::initBasicParam( BasicRequestParam &param, bfcp_prim primitive )
{
  param.cb = boost::bind(&BfcpClient::onResponse, this, primitive, _1, _2);
  param.conferenceID = conferenceID_;
  param.userID = userID_;
  param.dst = serverAddr_;
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
  if (!msg.valid())
  {
    LOG_TRACE << "BfcpClient received response with error " 
              << responce_error_name(err);
    // TODO: handle response error: timeout
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
    return;
  }
  else
  {
    BfcpAttr frqInfoAttr(*attr);
    bfcp_floor_request_info info = frqInfoAttr.getFloorRequestInfo();
    // TODO: handle info
  }
}

void BfcpClient::handleUserStatus( const BfcpMsg &msg )
{
  assert(msg.isResponse());
  if (!checkMsg(msg, BFCP_USER_STATUS)) return;
  
  auto attr = msg.findAttribute(BFCP_BENEFICIARY_INFO);
  if (attr) 
  {
    BfcpAttr beneficiaryInfoAttr(*attr);
    bfcp_user_info info = beneficiaryInfoAttr.getBeneficiaryInfo();
    // TODO: handle info
  }

  auto attrs = msg.findAttributes(BFCP_FLOOR_REQ_INFO);
  for (auto &attr : attrs)
  {
    bfcp_floor_request_info frqInfo = attr.getFloorRequestInfo();
    // TODO: handle frqInfo
  }

}

void BfcpClient::handleFloorStatus( const BfcpMsg &msg )
{
  if (!checkMsg(msg, BFCP_FLOOR_STATUS)) return;

  if (!msg.isResponse()) 
  {
    connection_->replyWithFloorStatusAck(msg);
  }

  // notify floor status
  auto attr = msg.findAttribute(BFCP_FLOOR_ID);
  if (attr)
  {
    BfcpAttr floorIDAttr(*attr);
    uint16_t floorID = floorIDAttr.getFloorID();
    // TODO: handle floorID

    auto attrs = msg.findAttributes(BFCP_FLOOR_REQ_INFO);
    for (auto &attr : attrs)
    {
      bfcp_floor_request_info info = attr.getFloorRequestInfo();
      // TODO: handle info
    }
  }
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

  auto attr = msg.findAttribute(BFCP_SUPPORTED_PRIMS);
  if (!attr)
  {
    LOG_ERROR << "No SUPPORTED-PRIMITIVES found in BFCP HelloAck message";
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
  // TODO: notify that client can exit
}

void BfcpClient::handleError( const BfcpMsg &msg )
{
  assert(msg.isResponse());
  if (!checkMsg(msg, BFCP_ERROR)) return;
  
  auto attr = msg.findAttribute(BFCP_ERROR_CODE);
  if (!attr)
  {
    LOG_ERROR << "No ERROR-CODE found in BFCP Error message";
    return;
  }
  else
  {
    BfcpAttr errorCodeAttr(*attr);
    bfcp_errcode errcode = errorCodeAttr.getErrorCode();
    // TODO: handle with error code
  }

  attr = msg.findAttribute(BFCP_ERROR_INFO);
  if (attr)
  {
    BfcpAttr errorInfoAttr(*attr);
    const char *errorInfo = errorInfoAttr.getErrorInfo();
    // TODO: handle with error info
  }
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