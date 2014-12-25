#ifndef BFCP_CALLBACKS_H
#define BFCP_CALLBACKS_H

#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>

namespace bfcp
{

struct bfcp_msg;
class ClientTransaction;
typedef struct bfcp_msg bfcp_msg_t;
typedef boost::shared_ptr<ClientTransaction> ClientTransactionPtr;

typedef boost::function<void (int, const bfcp_msg_t*)> ResponseCallback;
typedef boost::function<void (ClientTransactionPtr)> SendFailedCallback;

} // namespace bfcp


#endif // BFCP_CALLBACKS_H