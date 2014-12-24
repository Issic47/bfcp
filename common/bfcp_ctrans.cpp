#include "bfcp_ctrans.h"

namespace bfcp
{

bfcp::ClientTransaction::ClientTransaction(const muduo::net::UdpSocketPtr &socket, 
                                           const muduo::net::InetAddress &dst, 
                                           const bfcp_entity &entity, 
                                           mbuf_t *msgBuf)
    : socket_(socket), dst_(dst), entity_(entity), msgBuf_(msgBuf)
{

}

ClientTransaction::~ClientTransaction()
{
  mem_deref(msgBuf_);
}

void ClientTransaction::onTimeout()
{

}



} // namespace bfcp