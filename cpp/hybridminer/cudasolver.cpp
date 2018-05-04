#include <cmath>
#include "cudasolver.h"

// don't put this in the header . . .
#include "cuda_sha3.h"

CUDASolver::CUDASolver( int32_t device, double intensity ) noexcept :
m_stop( false ),
m_new_target( true ),
m_new_message( true ),
m_intensity( [=]() -> double {
    return intensity <= 41.99 ? intensity : 41.99;
  }() ),
m_threads( [=]() -> uint64_t {
    if( intensity <= 41.99 )
    {
      return static_cast<uint64_t>(std::pow( 2, intensity ));
    }
    return static_cast<uint64_t>(std::pow( 2, 41.99 ));
  }() ),
m_device_failure_count( 0u ),
m_gpu_initialized( false ),
m_device( device ),
m_grid( 1u ),
m_block( 1u )
{
}

CUDASolver::~CUDASolver()
{
  cudaCleanup();
}

auto CUDASolver::updateTarget() -> void
{
  m_new_target = true;
}

auto CUDASolver::updateMessage() -> void
{
  m_new_message = true;
}

auto CUDASolver::stopFinding() -> void
{
  m_stop = true;
}
