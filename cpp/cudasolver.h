#ifndef _CUDASOLVER_H_
#define _CUDASOLVER_H_

#include <vector_types.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <string>
#include "types.h"
#include "basesolver.h"

class CUDASolver : public IBaseSolver
{
public:
  CUDASolver() = delete;
  CUDASolver( int32_t const device, double const intensity ) noexcept;
  ~CUDASolver();

  auto findSolution() -> void final;
  auto stopFinding() -> void final;

  auto getHashrate() const -> double const final;

  auto updateTarget() -> void final;
  auto updateMessage() -> void final;

private:
  static uint32_t constexpr TPB35{  384u };
  static uint32_t constexpr TPB50{ 1024u };

  auto updateGPULoop() -> void;

  auto pushTarget() -> void;
  auto pushMessage() -> void;

  auto cudaInit() -> void;
  auto cudaCleanup() -> void;

  auto cudaResetSolution() -> void;

  auto getNextSearchSpace() -> uint64_t const;
  auto getTarget() const -> uint64_t const;
  auto getMidstate() const -> state_t const;
  auto pushSolution() const -> void;

  std::thread m_run_thread;

  std::atomic<bool> m_stop;
  std::atomic<bool> m_stopped;
  std::atomic<bool> m_new_target;
  std::atomic<bool> m_new_message;

  std::atomic<uint64_t> m_hash_count;
  std::atomic<uint64_t> m_hash_count_samples;
  std::atomic<double> m_hash_average;

  double m_intensity;
  uint64_t m_threads;

  uint_fast8_t m_device_failure_count;
  bool m_gpu_initialized;
  int32_t m_device;
  uint32_t* h_solution_count;
  uint32_t* d_solution_count;
  uint64_t* h_solutions;
  uint64_t* d_solutions;

  dim3 m_grid;
  dim3 m_block;

  std::chrono::steady_clock::time_point m_start;
};

#endif // !_CUDASOLVER_H_
