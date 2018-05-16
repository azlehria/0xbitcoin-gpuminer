#ifndef _CPUSOLVER_H_
#define _CPUSOLVER_H_

#include <atomic>
#include <thread>

#include "miner_state.h"
#include "types.h"
#include "sph_keccak.h"
#include "basesolver.h"

class CPUSolver : public IBaseSolver
{
public:
  CPUSolver() noexcept;
  ~CPUSolver();

  auto stopFinding() -> void final;
  auto findSolution() -> void final;

  auto getHashrate() const -> double const final;

  auto updateTarget() -> void final;
  auto updateMessage() -> void final;

private:
  auto getNextSearchSpace() -> uint64_t const;

  std::thread m_run_thread;

  sph_keccak256_context m_ctx;

  std::atomic<bool> m_new_target;
  std::atomic<uint64_t> m_target;

  std::atomic<bool> m_new_message;
  message_t m_message;

  std::atomic<uint64_t> m_hash_count;
  std::atomic<uint64_t> m_hash_count_samples;
  std::atomic<double> m_hash_average;

  std::atomic<bool> m_stop;
  std::atomic<bool> m_stopped;

  std::chrono::steady_clock::time_point m_start;
};

#endif // !_SOLVER_H_
