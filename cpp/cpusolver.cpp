#include <chrono>
#include "cpusolver.h"

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
m_new_target( true ),
m_new_message( true ),
m_hash_count( 0u ),
m_hash_count_samples( 0u ),
m_hash_average( 0 ),
m_stop( false ),
m_stopped( false ),
m_start( steady_clock::now() )
{
  sph_keccak256_init( &m_ctx );
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
  uint64_t solution;
  message_t in_buffer;
  hash_t out_buffer;

  do
  {
    if( m_new_target )
    {
      m_target = MinerState::getTargetNum();
      m_new_target = false;
    }
    if( m_new_message )
    {
      m_message = MinerState::getMessage();
      m_new_message = false;
    }

    solution = getNextSearchSpace();
    std::memcpy( in_buffer.data(), m_message.data(), 84 );
    std::memcpy( &in_buffer[64], &solution, 8 );

    sph_keccak256( &m_ctx, in_buffer.data(), in_buffer.size() );
    sph_keccak256_close( &m_ctx, out_buffer.data() );

    // printf("%" PRIx64 "\n%" PRIx64 "\n", bswap64( digest.data() ), reinterpret_cast<uint64_t&>(digest[0]));
    if( bswap64( reinterpret_cast<uint64_t&>(out_buffer[0]) ) < MinerState::getTargetNum() )
    {
      MinerState::pushSolution( solution );
    }
  } while( !m_stop );

  m_stopped = true;
}

auto CPUSolver::updateTarget() -> void
{
  m_new_target = true;
}
auto CPUSolver::updateMessage() -> void
{
  m_new_message = true;
}

auto CPUSolver::getHashrate() const -> double const
{
  return m_hash_average;
}

auto CPUSolver::getNextSearchSpace() -> uint64_t const
{
  m_hash_count++;
  double t{ static_cast<double>(duration_cast<milliseconds>(steady_clock::now() - m_start).count()) / 1000 };

  if( m_hash_count_samples < 100 )
  {
    ++m_hash_count_samples;
  }

  double temp_average{ m_hash_average };
  temp_average += ((m_hash_count / t) / 1000000 - temp_average) / m_hash_count_samples;
  if( std::isnan( temp_average ) || std::isinf( temp_average ) )
  {
    temp_average = m_hash_average;
  }
  m_hash_average = temp_average;
  return MinerState::getIncSearchSpace( 1 );
}
