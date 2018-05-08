#ifndef _CUDASOLVER_H_
#define _CUDASOLVER_H_

#include <vector_types.h>
#include <atomic>
#include <string>

class CUDASolver
{
public:
  CUDASolver() = delete;
  CUDASolver( int32_t device, double intensity ) noexcept;
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

  auto getNextSearchSpace() -> uint64_t;
  auto getTarget() -> uint64_t;
  auto getMidstate( uint64_t (& message)[25] ) -> void;
  auto pushSolution() -> void;

  std::atomic<bool> m_stop;
  std::atomic<bool> m_new_target;
  std::atomic<bool> m_new_message;

  double m_intensity;
  uint64_t m_threads;

  uint_fast8_t m_device_failure_count;
  bool m_gpu_initialized;
  int32_t m_device;
  uint64_t* h_solution;
  uint64_t* d_solution;

  dim3 m_grid;
  dim3 m_block;
};

#endif // !_CUDASOLVER_H_
