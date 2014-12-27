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

enum ResponseError
{
  kNoError = 0,
  kTimeout = 1,
};

typedef boost::function<void (ResponseError, const BfcpMsg&)> ResponseCallback;
typedef boost::function<void (const BfcpMsg&)> NewRequestCallback;

void defaultResponseCallback(ResponseError err, const BfcpMsg &msg);

} // namespace bfcp


#endif // BFCP_CALLBACKS_H