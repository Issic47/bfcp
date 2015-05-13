#include <bfcp/client/base_client.h>

#include <boost/bind.hpp>
#include <muduo/base/Logging.h>

#include <bfcp/common/bfcp_conn.h>

using namespace muduo;
using namespace muduo::net;

namespace bfcp
{

BaseClient::BaseClient(muduo::net::EventLoop* loop, 
                       const muduo::net::InetAddress& serverAddr, 
                       uint32_t conferenceID, 
                       uint16_t userID, 
                       double heartBeatInterval)
    : loop_(loop),
      client_(loop, serverAddr, "BfcpClient"),
      serverAddr_(serverAddr),
      conferenceID_(conferenceID),
      userID_(userID),
      state_(kDisconnected),
      heartBeatInterval_(heartBeatInterval),
      msgSendCount_(0)
{
  client_.setMessageCallback(
    boost::bind(&BaseClient::onMessage, this, _1, _2, _3, _4));
  client_.setStartedRecvCallback(
    boost::bind(&BaseClient::onStartedRecv, this, _1));
  client_.setWriteCompleteCallback(
    boost::bind(&BaseClient::onWriteComplete, this, _1, _2));

  initResponseHandlers();
}

BaseClient::~BaseClient()
{
  loop_->cancel(heartBeatTimer_);
}

void BaseClient::initResponseHandlers()
{
  responseHandlers_.insert(std::make_pair(
    BFCP_FLOOR_REQUEST, 
    boost::bind(&BaseClient::handleFloorRequestStatus, this, _1)));

  responseHandlers_.insert(std::make_pair(
    BFCP_FLOOR_RELEASE,
    boost::bind(&BaseClient::handleFloorRequestStatus, this, _1)));

  responseHandlers_.insert(std::make_pair(
    BFCP_FLOOR_REQUEST_QUERY,
    boost::bind(&BaseClient::handleFloorRequestStatus, this, _1)));

  responseHandlers_.insert(std::make_pair(
    BFCP_USER_QUERY,
    boost::bind(&BaseClient::handleUserStatus, this, _1)));

  responseHandlers_.insert(std::make_pair(
    BFCP_FLOOR_QUERY,
    boost::bind(&BaseClient::handleFloorStatus, this, _1)));

  responseHandlers_.insert(std::make_pair(
    BFCP_CHAIR_ACTION,
    boost::bind(&BaseClient::handleChairAcionAck, this, _1)));

  responseHandlers_.insert(std::make_pair(
    BFCP_HELLO, 
    boost::bind(&BaseClient::handleHelloAck, this, _1)));

  responseHandlers_.insert(std::make_pair(
    BFCP_GOODBYE,
    boost::bind(&BaseClient::handleGoodbyeAck, this, _1)));
}

void BaseClient::connect()
{
  if (state_ == kDisconnected)
  {
    client_.connect();
    changeState(kConnecting);
  }
}

void BaseClient::disconnect()
{
  if (state_ == kConnected)
  {
    sendGoodbye();
    changeState(kDisconnecting);
  }
}

void BaseClient::forceDisconnect()
{
  if (state_ != kDisconnected)
  {
    client_.disconnect();
    changeState(kDisconnected);
  }
}

void BaseClient::changeState( State state )
{
  if (state_ != state)
  {
    LOG_TRACE << "BfcpClient change state to " << toString(state);
    state_ = state;
    if (state == kConnected && heartBeatInterval_ > 0.0)
    {
      heartBeatTimer_ = loop_->runEvery(heartBeatInterval_, 
        boost::bind(&BaseClient::onHeartBeatTimeout, this));
    }
    else if (state == kDisconnected)
    {
      loop_->cancel(heartBeatTimer_);
    }
    if (stateChangedCallback_)
      stateChangedCallback_(state);
  }

  // clear task queue whatever
  if (state == kDisconnected) 
  {
    tasks_.clear();
  }
}

const char* BaseClient::toString( State state ) const
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

void BaseClient::onHeartBeatTimeout()
{
  if (tasks_.empty())
  {
    sendHello();
  }
}

void BaseClient::onStartedRecv( const UdpSocketPtr& socket )
{
  LOG_TRACE << socket->getLocalAddr().toIpPort() << " started receiving data from " 
            << socket->getPeerAddr().toIpPort();

  assert(socket == client_.socket());
  if (!connection_) 
  {
    connection_ = boost::make_shared<BfcpConnection>(loop_, client_.socket());
    connection_->setNewRequestCallback(
      boost::bind(&BaseClient::onNewRequest, this, _1));
  }
  if (state_ == kConnecting)
  {
    sendHello();
  }
}

void BaseClient::sendFloorRequest( const FloorRequestParam &floorRequest )
{
  assert(client_.isConnected());
  assert(connection_);

  runSendMessageTask(boost::bind(
    &BfcpConnection::sendFloorRequest, 
    connection_, 
    generateBasicParam(BFCP_FLOOR_REQUEST),
    floorRequest));
}

void BaseClient::sendFloorRelease( uint16_t floorRequestID )
{
  assert(client_.isConnected());
  assert(connection_);

  runSendMessageTask(boost::bind(
    &BfcpConnection::sendFloorRelease, 
    connection_, 
    generateBasicParam(BFCP_FLOOR_RELEASE),
    floorRequestID));
}

void BaseClient::sendFloorRequestQuery( uint16_t floorRequestID )
{
  assert(client_.isConnected());
  assert(connection_);

  runSendMessageTask(boost::bind(
    &BfcpConnection::sendFloorRequestQuery, 
    connection_, 
    generateBasicParam(BFCP_FLOOR_REQUEST_QUERY),
    floorRequestID));
}

void BaseClient::sendUserQuery( const UserQueryParam userQuery )
{
  assert(client_.isConnected());
  assert(connection_);

  runSendMessageTask(boost::bind(
    &BfcpConnection::sendUserQuery, 
    connection_, 
    generateBasicParam(BFCP_USER_QUERY),
    userQuery));
}

void BaseClient::sendFloorQuery( const bfcp_floor_id_list &floorIDs )
{
  assert(client_.isConnected());
  assert(connection_);

  runSendMessageTask(boost::bind(
    &BfcpConnection::sendFloorQuery, 
    connection_, 
    generateBasicParam(BFCP_FLOOR_QUERY),
    floorIDs));
}

void BaseClient::sendChairAction( const FloorRequestInfoParam &frqInfo )
{
  assert(client_.isConnected());
  assert(connection_);

  runSendMessageTask(boost::bind(
    &BfcpConnection::sendChairAction, 
    connection_, 
    generateBasicParam(BFCP_CHAIR_ACTION),
    frqInfo));
}

void BaseClient::sendHello()
{
  assert(client_.isConnected());
  assert(connection_);

  runSendMessageTask(boost::bind(
    &BfcpConnection::sendHello, 
    connection_, 
    generateBasicParam(BFCP_HELLO)));
}

void BaseClient::sendGoodbye()
{
  assert(client_.isConnected());
  assert(connection_);

  runSendMessageTask(boost::bind(
    &BfcpConnection::sendGoodbye, 
    connection_, 
    generateBasicParam(BFCP_GOODBYE)));
}

BasicRequestParam BaseClient::generateBasicParam( bfcp_prim primitive )
{
  BasicRequestParam param;
  param.cb = boost::bind(&BaseClient::onResponse, this, primitive, _1, _2);
  param.conferenceID = conferenceID_;
  param.userID = userID_;
  param.dst = serverAddr_;
  return param;
}

void BaseClient::onMessage(const UdpSocketPtr& socket, 
                           Buffer* buf, 
                           const InetAddress &src, 
                           Timestamp time)
{
  LOG_TRACE << client_.name() << " recv " << buf->readableBytes() 
            << " bytes at " << time.toString() 
            << " from " << src.toIpPort();

  assert(connection_);
  connection_->onMessage(buf, src, time);
}

void BaseClient::onWriteComplete( const UdpSocketPtr& socket, int messageId )
{
  LOG_TRACE << "message " << messageId << " write completed";
  ++msgSendCount_;
}

void BaseClient::onNewRequest( const BfcpMsgPtr &msg )
{
  LOG_TRACE << "BfcpClient received new request " 
            << bfcp_prim_name(msg->primitive());

  switch (msg->primitive())
  {
    case BFCP_FLOOR_STATUS: 
      handleFloorStatus(msg); break;

    case BFCP_FLOOR_REQUEST_STATUS: 
      handleFloorRequestStatus(msg); break;

    default:
      LOG_ERROR << "Ignore unexpected BFCP request message " 
                << msg->primitive();
      break;
  }
}

void BaseClient::onResponse(bfcp_prim requestPrimitive, 
                            ResponseError err, 
                            const BfcpMsgPtr &msg)
{
  if (err != ResponseError::kNoError)
  {
    LOG_WARN << "BfcpClient received response with error " 
             << response_error_name(err);
    changeState(kDisconnected);
  }
  else
  {
    LOG_TRACE << "BfcpClient received response " 
              << bfcp_prim_name(msg->primitive());
    runNextSendMesssageTask();
    if (msg->primitive() == BFCP_ERROR) 
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

void BaseClient::handleFloorRequestStatus( const BfcpMsgPtr &msg )
{
  if (!checkMsg(msg, BFCP_FLOOR_REQUEST_STATUS)) return;

  if (!msg->isResponse())
  {
    connection_->replyWithFloorRequestStatusAck(msg);
  }

  auto attr = msg->findAttribute(BFCP_FLOOR_REQ_INFO);
  if (!attr) 
  {
    LOG_ERROR << "No FLOOR-REQUEST-INFORMATION found in BFCP FloorRequestStatus message";
    if (responseReceivedCallback_)
      responseReceivedCallback_(kMissingMandatoryAttr, BFCP_FLOOR_REQUEST_STATUS, nullptr);
  }
  else
  {
    BfcpAttr frqInfoAttr(*attr);
    bfcp_floor_request_info info = frqInfoAttr.getFloorRequestInfo();

    FloorRequestInfoParam param;
    param.set(info);

    if (responseReceivedCallback_)
      responseReceivedCallback_(kNoError, BFCP_FLOOR_REQUEST_STATUS, &param);
  }
}

void BaseClient::handleUserStatus( const BfcpMsgPtr &msg )
{
  assert(msg->isResponse());
  if (!checkMsg(msg, BFCP_USER_STATUS)) return;

  UserStatusParam param;
  auto attr = msg->findAttribute(BFCP_BENEFICIARY_INFO);
  if (attr) 
  {
    BfcpAttr beneficiaryInfoAttr(*attr);
    bfcp_user_info info = beneficiaryInfoAttr.getBeneficiaryInfo();
    param.setBeneficiary(info);
  }

  auto attrs = msg->findAttributes(BFCP_FLOOR_REQ_INFO);
  for (auto &attr : attrs)
  {
    bfcp_floor_request_info frqInfo = attr.getFloorRequestInfo();
    param.addFloorRequestInfo(frqInfo);
  }

  if (responseReceivedCallback_)
    responseReceivedCallback_(kNoError, BFCP_USER_STATUS, &param);
}

void BaseClient::handleFloorStatus( const BfcpMsgPtr &msg )
{
  if (!checkMsg(msg, BFCP_FLOOR_STATUS)) return;

  if (!msg->isResponse()) 
  {
    connection_->replyWithFloorStatusAck(msg);
  }

  auto attr = msg->findAttribute(BFCP_FLOOR_ID);
  if (!attr)
  {
    if (responseReceivedCallback_)
      responseReceivedCallback_(kNoError, BFCP_FLOOR_STATUS, nullptr);
  }
  else
  {
    BfcpAttr floorIDAttr(*attr);
    FloorStatusParam param;
    param.floorID = floorIDAttr.getFloorID();
    param.hasFloorID = true;

    auto attrs = msg->findAttributes(BFCP_FLOOR_REQ_INFO);
    for (auto &attr : attrs)
    {
      bfcp_floor_request_info info = attr.getFloorRequestInfo();
      param.addFloorRequestInfo(info);
    }

    if (responseReceivedCallback_)
      responseReceivedCallback_(kNoError, BFCP_FLOOR_STATUS, &param);
  }
}

void BaseClient::handleChairAcionAck( const BfcpMsgPtr &msg )
{
  assert(msg->isResponse());
  if (!checkMsg(msg, BFCP_CHAIR_ACTION_ACK)) return;
  if (responseReceivedCallback_)
    responseReceivedCallback_(kNoError, BFCP_CHAIR_ACTION_ACK, nullptr);
}

void BaseClient::handleHelloAck( const BfcpMsgPtr &msg )
{
  assert(msg->isResponse());
  if (!checkMsg(msg, BFCP_HELLO_ACK)) return;
  
  changeState(kConnected);

  auto attr = msg->findAttribute(BFCP_SUPPORTED_PRIMS);
  if (!attr)
  {
    LOG_ERROR << "No SUPPORTED-PRIMITIVES found in BFCP HelloAck message";
    if (responseReceivedCallback_)
      responseReceivedCallback_(kMissingMandatoryAttr, BFCP_HELLO_ACK, nullptr);
    return;
  }
  else
  {
    BfcpAttr supportedPrimsAttr(*attr);
    auto &supPrims = supportedPrimsAttr.getSupportedPrims();
    // TODO: check supported primitives
    (void)(supPrims);
  }

  attr = msg->findAttribute(BFCP_SUPPORTED_ATTRS);
  if (!attr)
  {
    LOG_ERROR << "No SUPPORTED-ATTRIBUTES found in BFCP HelloAck message";
    if (responseReceivedCallback_)
      responseReceivedCallback_(kMissingMandatoryAttr, BFCP_HELLO_ACK, nullptr);
    return;
  }
  else
  {
    BfcpAttr supportedAttrsAttr(*attr);
    auto &supAttrs = supportedAttrsAttr.getSupportedAttrs();
    // TODO: check supported attributes
    (void)(supAttrs);
  }

  if (responseReceivedCallback_)
    responseReceivedCallback_(kNoError, BFCP_HELLO_ACK, nullptr);
}

void BaseClient::handleGoodbyeAck( const BfcpMsgPtr &msg )
{
  assert(msg->isResponse());
  if (!checkMsg(msg, BFCP_GOODBYE_ACK)) return;

  if (state_ == kDisconnecting)
  {
    client_.disconnect();
    changeState(kDisconnected);
  }
}

void BaseClient::handleError( const BfcpMsgPtr &msg )
{
  assert(msg->isResponse());
  if (!checkMsg(msg, BFCP_ERROR)) return;
  
  ErrorParam param;
  auto attr = msg->findAttribute(BFCP_ERROR_CODE);
  if (!attr)
  {
    LOG_ERROR << "No ERROR-CODE found in BFCP Error message";
    if (responseReceivedCallback_)
      responseReceivedCallback_(kMissingMandatoryAttr, BFCP_ERROR, nullptr);
    return;
  }

  BfcpAttr errorCodeAttr(*attr);
  bfcp_errcode errcode = errorCodeAttr.getErrorCode();
  param.errorCode.set(errcode);

  attr = msg->findAttribute(BFCP_ERROR_INFO);
  if (attr)
  {
    BfcpAttr errorInfoAttr(*attr);
    const char *errorInfo = errorInfoAttr.getErrorInfo();
    param.setErrorInfo(errorInfo);
  }
  
  if (responseReceivedCallback_)
    responseReceivedCallback_(kNoError, BFCP_ERROR, &param);
}

bool BaseClient::checkMsg( const BfcpMsgPtr &msg, bfcp_prim expectedPrimitive ) const
{
  if (msg->getConferenceID() != conferenceID_)
  {
    LOG_ERROR << "Expected BFCP Conference ID is " << conferenceID_
              << " but get " << msg->getConferenceID();
    return false;
  }

  if (msg->getUserID() != userID_)
  {
    LOG_ERROR << "Expected BFCP User ID is " << userID_
              << " but get " << msg->getUserID();
    return false;
  }

  if (msg->primitive() != expectedPrimitive) 
  {
    LOG_ERROR << "Expected BFCP " << bfcp_prim_name(expectedPrimitive) 
              << " but get " << bfcp_prim_name(msg->primitive());
    return false;
  }

  auto &unknownAttrs = msg->getUnknownAttrs();
  if (unknownAttrs.typec != 0)
  {
    LOG_WARN << "Ignore " << unknownAttrs.typec 
             << " unknown attribute(s) in the received " 
             << bfcp_prim_name(msg->primitive());
  }

  return true;
}

} // namespace bfcp