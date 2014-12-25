#include "bfcp_conn.h"

using namespace bfcp;

BfcpConnection::BfcpConnection( uint8_t version, const muduo::net::UdpSocketPtr &socket )
   : version_(version), socket_(socket)
{

}

BfcpConnection::~BfcpConnection()
{

}

void BfcpConnection::sendHello( uint32_t conferenceID, uint16_t userID )
{
  
}


