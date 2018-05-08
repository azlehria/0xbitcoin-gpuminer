#include <cmath>
#include "cudasolver.h"

// don't put this in the header . . .
#include "miner_state.h"

CUDASolver::CUDASolver( int32_t device, double intensity ) noexcept :
m_stop( false ),
m_new_target( true ),
m_new_message( true ),
m_intensity( intensity <= 41.99 ? intensity : 41.99 ),
m_threads( static_cast<uint64_t>(std::pow( 2, intensity <= 41.99 ? intensity : 41.99 )) ),
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

auto CUDASolver::getNextSearchSpace() -> uint64_t
{
  return MinerState::getIncSearchSpace( m_threads );
}

auto CUDASolver::getTarget() -> uint64_t
{
  return MinerState::getTarget();
}

auto CUDASolver::getMidstate( uint64_t (& message)[25] ) -> void
{
  MinerState::getMidstate( message, m_device );
}

auto CUDASolver::pushSolution() -> void
{
  MinerState::pushSolution( *h_solution );
}
