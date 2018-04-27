#ifndef _CPUSOLVER_H_
#define _CPUSOLVER_H_

#include <atomic>

#include "miner_state.h"

class CPUSolver
{
public:
  CPUSolver() noexcept;

  auto stopFinding() -> void;
  auto findSolution() const -> void;

private:
  std::atomic<bool> m_stop;
};

#endif // !_SOLVER_H_
