#ifndef _CUDASOLVER_H_
#define _CUDASOLVER_H_

// default magic numbers
#define INTENSITY 23
#define CUDA_DEVICE 0
// default magic numbers

#include "miner_state.h"

#include <cuda_runtime.h>
#include <atomic>
#include <string>

class CUDASolver
{
public:
  CUDASolver() = delete;
  CUDASolver( int32_t device, int32_t intensity ) noexcept;
  ~CUDASolver();

  auto findSolution() -> void;
  auto stopFinding() -> void;

  auto updateTarget() -> void;
  auto updateMessage() -> void;

private:
  auto updateGPULoop() -> void;

  auto pushTarget() -> void;
  auto pushMessage() -> void;

  auto cudaInit() -> void;
  auto cudaCleanup() -> void;

  auto cudaResetSolution() -> void;

  std::atomic<bool> m_stop;
  std::atomic<bool> m_new_target;
  std::atomic<bool> m_new_message;

  uint32_t m_threads;
  int32_t m_device_id;

  bool m_gpu_initialized;
  int32_t m_device;
  uint64_t* h_solution;
  uint64_t* d_solution;

  dim3 m_grid;
  dim3 m_block;
};

#endif // !_CUDASOLVER_H_
