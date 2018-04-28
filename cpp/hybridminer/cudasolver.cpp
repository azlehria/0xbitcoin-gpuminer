#include "cudasolver.h"

// don't put this in the header . . .
#include "cuda_sha3.h"

CUDASolver::CUDASolver( int32_t device, int32_t intensity ) noexcept :
m_stop( false ),
m_new_target( true ),
m_new_message( true ),
m_intensity( intensity ),
m_threads( 1u << intensity ),
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
