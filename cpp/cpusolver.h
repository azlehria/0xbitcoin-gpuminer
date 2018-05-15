#ifndef _CPUSOLVER_H_
#define _CPUSOLVER_H_

#include <atomic>

#include "miner_state.h"

class CPUSolver
{
public:
  CPUSolver() noexcept;
  ~CPUSolver();

  auto stopFinding() -> void;
  auto findSolution() -> void;

  auto getHashrate() const -> double const;

private:
  std::thread m_run_thread;

  std::atomic<uint64_t> m_hash_count;
  std::atomic<uint64_t> m_hash_count_samples;
  std::atomic<double> m_hash_average;

  std::atomic<bool> m_stop;
  std::atomic<bool> m_stopped;

  std::chrono::steady_clock::time_point m_start;
};

#endif // !_SOLVER_H_
