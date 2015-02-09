#ifndef BFCP_MESSAGE_QUEUE_H
#define BFCP_MESSAGE_QUEUE_H

class MessageQueue
{
public:
  MessageQueue(uint32_t handle);

private:
  uint32_t handle_;
  
  bool isInGlobal_;

};

#endif // BFCP_MESSAGE_QUEUE_H