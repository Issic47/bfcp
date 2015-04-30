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
typedef boost::shared_ptr<BfcpMsg> BfcpMsgPtr;

enum class ResponseError
{
  kNoError = 0,
  kTimeout = 1,
};

typedef boost::function<void (ResponseError, const BfcpMsgPtr&)> ResponseCallback;
typedef boost::function<void (const BfcpMsgPtr&)> NewRequestCallback;

void defaultResponseCallback(ResponseError err, const BfcpMsgPtr &msg);
const char* response_error_name(ResponseError err);

} // namespace bfcp


#endif // BFCP_CALLBACKS_H