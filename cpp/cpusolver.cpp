#include <chrono>
#include "cpusolver.h"
#include "sha3.h"

using namespace std::chrono;

// --------------------------------------------------------------------

auto bswap64( uint64_t const& in ) -> uint64_t
{
  uint64_t out;
  uint8_t* temp = reinterpret_cast<uint8_t*>(&out);
  uint8_t const* t_in = reinterpret_cast<uint8_t const*>(&in);

  temp[0] = t_in[7];
  temp[1] = t_in[6];
  temp[2] = t_in[5];
  temp[3] = t_in[4];
  temp[4] = t_in[3];
  temp[5] = t_in[2];
  temp[6] = t_in[1];
  temp[7] = t_in[0];

  return out;
}

CPUSolver::CPUSolver() noexcept :
m_hash_count( 0u ),
m_hash_count_samples( 0u ),
m_hash_average( 0 ),
m_stop( false ),
m_stopped( false ),
m_start( steady_clock::now() )
{
  m_run_thread = std::thread( &CPUSolver::findSolution, this );
}

CPUSolver::~CPUSolver()
{
  stopFinding();
  while( !m_stopped || !m_run_thread.joinable() )
  {
    std::this_thread::sleep_for( std::chrono::milliseconds( 50u ) );
  }
  m_run_thread.join();
}

auto CPUSolver::stopFinding() -> void
{
  m_stop = true;
}

auto CPUSolver::findSolution() -> void
{
  uint64_t solution[25];
  message_t buffer;

  do
  {
    buffer = MinerState::getMessage();
    std::memset( solution, 0, 200 );
    std::memcpy( solution, buffer.data(), 84 );
    solution[8] = MinerState::getIncSearchSpace( 1 );

    // keccakf( solution );
    // keccak_256( &digest[0], digest.size(), &buffer[0], buffer.size() );

    // printf("%" PRIx64 "\n%" PRIx64 "\n", bswap64( digest.data() ), reinterpret_cast<uint64_t&>(digest[0]));
    if( bswap64( solution[0] ) < MinerState::getTargetNum() )
    {
      MinerState::pushSolution( solution[0] );
    }
  } while( !m_stop );

  m_stopped = true;
}

auto CPUSolver::getHashrate() const -> double const
{
  return m_hash_average;
}
