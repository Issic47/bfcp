#ifndef BFCP_CALLBACKS_H
#define BFCP_CALLBACKS_H

#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>

struct bfcp_msg;
typedef struct bfcp_msg bfcp_msg_t;

namespace bfcp
{

class BfcpMsg;
class ClientTransaction;
typedef boost::shared_ptr<ClientTransaction> ClientTransactionPtr;

typedef boost::function<void (int, const BfcpMsg&)> ResponseCallback;
typedef boost::function<void (ClientTransactionPtr)> SendFailedCallback;

typedef boost::function<void (const BfcpMsg&)> NewRequestCallback;

} // namespace bfcp


#endif // BFCP_CALLBACKS_H