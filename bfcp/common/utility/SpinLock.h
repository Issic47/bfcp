#ifndef BFCP_SPIN_LOCK_H
#define BFCP_SPIN_LOCK_H

#include <atomic>

namespace bfcp
{

class SpinLock
{
public:
  void lock() { while (lock_.test_and_set(std::memory_order_acquire)); }
  void unlock() { lock_.clear(std::memory_order_release); }
private:
  std::atomic_flag lock_;
};

} // namespace bfcp


#endif // BFCP_SPIN_LOCK_H