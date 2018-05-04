#include <fstream>
#include "miner_state.h"

static char const* ascii[] = {
  "00","01","02","03","04","05","06","07","08","09","0a","0b","0c","0d","0e","0f",
  "10","11","12","13","14","15","16","17","18","19","1a","1b","1c","1d","1e","1f",
  "20","21","22","23","24","25","26","27","28","29","2a","2b","2c","2d","2e","2f",
  "30","31","32","33","34","35","36","37","38","39","3a","3b","3c","3d","3e","3f",
  "40","41","42","43","44","45","46","47","48","49","4a","4b","4c","4d","4e","4f",
  "50","51","52","53","54","55","56","57","58","59","5a","5b","5c","5d","5e","5f",
  "60","61","62","63","64","65","66","67","68","69","6a","6b","6c","6d","6e","6f",
  "70","71","72","73","74","75","76","77","78","79","7a","7b","7c","7d","7e","7f",
  "80","81","82","83","84","85","86","87","88","89","8a","8b","8c","8d","8e","8f",
  "90","91","92","93","94","95","96","97","98","99","9a","9b","9c","9d","9e","9f",
  "a0","a1","a2","a3","a4","a5","a6","a7","a8","a9","aa","ab","ac","ad","ae","af",
  "b0","b1","b2","b3","b4","b5","b6","b7","b8","b9","ba","bb","bc","bd","be","bf",
  "c0","c1","c2","c3","c4","c5","c6","c7","c8","c9","ca","cb","cc","cd","ce","cf",
  "d0","d1","d2","d3","d4","d5","d6","d7","d8","d9","da","db","dc","dd","de","df",
  "e0","e1","e2","e3","e4","e5","e6","e7","e8","e9","ea","eb","ec","ed","ee","ef",
  "f0","f1","f2","f3","f4","f5","f6","f7","f8","f9","fa","fb","fc","fd","fe","ff"
};

#define ROTL64(x, y) (((x) << (y)) ^ ((x) >> (64 - (y))))

static auto fromAscii( uint8_t const c ) -> uint8_t
{
  if( c >= '0' && c <= '9' )
    return ( c - '0' );
  if( c >= 'a' && c <= 'f' )
    return ( c - 'a' + 10 );
  if( c >= 'A' && c <= 'F' )
    return ( c - 'A' + 10 );

#if defined(__EXCEPTIONS) || defined(DEBUG)
  throw std::runtime_error( "invalid character" );
#else
  return 0xff;
#endif
}

static auto ascii_r( uint8_t const a, uint8_t const b ) -> uint8_t
{
  return fromAscii( a ) * 16 + fromAscii( b );
}

static auto HexToBytes( std::string const hex, uint8_t bytes[] ) -> void
{
  for( std::string::size_type i = 0, j = 0; i < hex.length(); i += 2, ++j )
  {
    bytes[j] = ascii_r( hex[i], hex[i + 1] );
  }
}

// --------------------------------------------------------------------

std::mutex MinerState::m_solutions_mutex;
std::queue<uint64_t> MinerState::m_solutions_queue;
std::string MinerState::m_solution_start;
std::string MinerState::m_solution_end;
std::atomic<uint64_t> MinerState::m_hash_count{ 0ull };
std::atomic<uint64_t> MinerState::m_hash_count_printable{ 0ull };
std::atomic<uint64_t> MinerState::m_hash_count_samples{ 0ull };
std::atomic<double> MinerState::m_hash_average{ 0.0 };
MinerState::bytes_t MinerState::m_message( MESSAGE_LENGTH );
std::mutex MinerState::m_message_mutex;
std::atomic<uint64_t> m_sol_count{ 0ull };
std::mutex MinerState::m_print_mutex;
std::string MinerState::m_challenge_printable;
std::string MinerState::m_address_printable;
std::atomic<uint64_t> MinerState::m_target{ 0ull };
std::atomic<bool> MinerState::m_custom_diff{ false };
std::atomic<uint64_t> MinerState::m_diff{ 1ull };
std::atomic<uint64_t> MinerState::m_sol_count{ 0ull };
std::atomic<bool> MinerState::m_new_solution{ false };
std::string MinerState::m_address;
std::mutex MinerState::m_address_mutex;
std::string MinerState::m_pool_address;
std::mutex MinerState::m_pool_mutex;

std::chrono::steady_clock::time_point MinerState::m_start;
std::chrono::steady_clock::time_point MinerState::m_end;

auto MinerState::initState() -> void
{
  bytes_t temp_solution( MESSAGE_LENGTH );
  reinterpret_cast<uint64_t&>(temp_solution[0]) = 06055134500533075101ull;

  std::random_device r;
  std::mt19937_64 gen{ r() };
  std::uniform_int_distribution<uint64_t> urInt{ 0, UINT64_MAX };

  for( uint_fast8_t i_rand{ 8 }; i_rand < 32; i_rand += 8 )
  {
    reinterpret_cast<uint64_t&>(temp_solution[i_rand]) = urInt( gen );
  }

  std::memset( &temp_solution[12], 0, 8 );

  m_message_mutex.lock();
  std::memcpy( &m_message[52], temp_solution.data(), 32 );
  m_message_mutex.unlock();

  std::string str_solution{ bytesToString( temp_solution ) };

  m_solution_start = str_solution.substr( 0, 24 );
  m_solution_end = str_solution.substr( 40, 24 );

  m_start = std::chrono::steady_clock::now();
}

auto MinerState::getIncSearchSpace( uint64_t const threads ) -> uint64_t
{
  m_hash_count_printable += threads;

  return m_hash_count.fetch_add( threads, std::memory_order_seq_cst );
}

auto MinerState::resetCounter() -> void
{
  m_hash_count_printable = 0ull;

  m_start = std::chrono::steady_clock::now();
}

auto MinerState::printStatus( bool old_ui ) -> void
{
  if( m_hash_count_printable <= 0 ) return;

  auto t = static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::steady_clock::now() - m_start ).count()) / 1000;

  ++m_hash_count_samples;

  double temp_average{ m_hash_average };
  //temp_average = ((temp_average) * (m_hash_count_samples - 1) + m_hash_count_printable) / m_hash_count_samples;
  temp_average -= temp_average / m_hash_count_samples;
  temp_average += ((m_hash_count_printable / t) / m_hash_count_samples);
  m_hash_average = temp_average;

  double t2{ temp_average / 1000000 };

  std::stringstream ss_out;

  if( old_ui )
  {
    static uint64_t time_counter{ 0 };
    ++time_counter;
    if( (time_counter % 50) != 0 ) return;

    ss_out << getPrintableTimeStamp()
           << std::setw( 10 ) << std::setfill( ' ' ) << std::fixed << std::setprecision( 2 )
           << ( std::isnan( t2 ) || std::isinf( t2 ) ? 0 : t2 )
           << " MH/s  Sols:"
           << std::setw( 6 ) << std::setfill( ' ' ) << m_sol_count
           << (m_new_solution ? '^' : ' ')
           << " Search time: "
           << std::fixed << std::setprecision( 0 )
           << std::setw( 2 ) << std::setfill( '0' ) << std::floor(t/60) << ":"
           << std::setw( 2 ) << std::setfill( '0' ) << std::floor( std::fmod( t, 60 ) )
           << '\n';

    m_new_solution = false;
  }
  else
  {
    // maybe breaking the control codes into macros is a good idea . . .
    ss_out << "\x1b[s\x1b[?25l\x1b[2;22f\x1b[38;5;221m"
           << std::setw( 8 ) << std::setfill( ' ' ) << std::fixed << std::setprecision( 2 )
           << ( std::isnan( t2 ) || std::isinf( t2 ) ? 0 : t2 )
           << "\x1b[2;75f\x1b[38;5;33m"
           << std::fixed << std::setprecision( 0 )
           << std::setw( 2 ) << std::setfill( '0' ) << std::floor(t/60) << ":"
           << std::setw( 2 ) << std::setfill( '0' ) << std::floor( std::fmod( t, 60 ) )
           << "\x1b[3;14f\x1b[38;5;34m"
           << m_diff
           << "\x1b[3;22f\x1b[38;5;221m"
           << std::setw( 8 ) << std::setfill( ' ' ) << m_sol_count
           << "\x1b[3;72f\x1b[38;5;33m";
    m_print_mutex.lock();
    ss_out << m_address_printable
           <<"\x1b[2;13f\x1b[38;5;34m"
           <<  m_challenge_printable;
    m_print_mutex.unlock();
    ss_out.imbue( std::locale( "" ) );
    ss_out << "\x1b[3;36f\x1b[38;5;208m"
           << std::setw( 25 ) << m_hash_count_printable;
    ss_out << "\x1b[0m\x1b[u\x1b[?25h";
  }

  std::cout << ss_out.str();
}

auto MinerState::getPrintableTimeStamp() -> std::string const
{
  using namespace std::chrono;

  std::stringstream ss_ts;

  auto now{ system_clock::now() };
  auto now_ms{ duration_cast<milliseconds>( now.time_since_epoch() ) % 1000 };
  std::time_t tt_ts{ system_clock::to_time_t( now ) };
  std::tm tm_ts{ *std::localtime( &tt_ts ) };

  ss_ts << std::put_time( &tm_ts, "[%T" ) << '.'
        << std::setw( 3 ) << std::setfill( '0' ) << now_ms.count() << "] ";

  return ss_ts.str();
}

auto MinerState::getPrintableHashCount() -> uint64_t
{
  return m_hash_count_printable;
}

auto MinerState::hexStr( uint8_t const* data, int32_t const len ) -> std::string
{
  std::stringstream ss;
  ss << std::hex;
  for( int_fast32_t i{ 0 }; i < len; ++i )
    ss << std::setw( 2 ) << std::setfill( '0' ) << static_cast<int8_t>(data[i]);
  return ss.str();
}

auto MinerState::hexToBytes( std::string const hex, bytes_t& bytes ) -> void
{
  assert( hex.length() % 2 == 0 );
  // assert( bytes.size() == ( hex.length() / 2 - 1 ) );
  HexToBytes( hex.substr( 2 ), &bytes[0] );
}

auto MinerState::bytesToString( bytes_t const buffer ) -> std::string
{
  std::string output;
  output.reserve( buffer.size() * 2 + 1 );

  for( uint_fast32_t i{ 0 }; i < buffer.size(); ++i )
    output += ascii[buffer[i]];

  return output;
}

auto MinerState::getSolution() -> std::string const
{
  if( m_solutions_queue.empty() )
    return "";

  uint64_t ret;
  bytes_t buf( 8 );

  m_solutions_mutex.lock();
  ret = m_solutions_queue.front();
  m_solutions_queue.pop();
  m_solutions_mutex.unlock();

  std::memcpy( buf.data(), &ret, 8 );
  return m_solution_start + bytesToString( buf ) + m_solution_end;
}

auto MinerState::pushSolution( uint64_t const sol ) -> void
{
  m_solutions_mutex.lock();
  m_solutions_queue.push( sol );
  m_solutions_mutex.unlock();
}

auto MinerState::incSolCount( uint64_t const count ) -> void
{
  m_sol_count += count;
  if( count > 0 ) m_new_solution = true;
}

auto MinerState::getSolCount() -> uint64_t const
{
  return m_sol_count;
}

auto MinerState::setPrefix( std::string const prefix ) -> void
{
  assert( prefix.length() == ( PREFIX_LENGTH * 2 + 2 ) );

  bytes_t temp( 52 );
  hexToBytes( prefix, temp );

  m_message_mutex.lock();
  std::memcpy( m_message.data(), temp.data(), 52 );
  m_message_mutex.unlock();

  m_print_mutex.lock();
  m_challenge_printable = prefix.substr( 2, 8 );
  m_print_mutex.unlock();
}

auto MinerState::setTarget( std::string const target ) -> void
{
  assert( target.length() <= ( UINT256_LENGTH * 2 + 2 ) );

  std::string const t( static_cast<std::string::size_type>( UINT256_LENGTH * 2 + 2 ) - target.length(), '0' );

  uint64_t temp{ std::stoull( (t + target.substr( 2 )).substr( 0, 16 ), nullptr, 16 ) };
  if( temp == m_target ) return;

  m_target = temp;
}

auto MinerState::getMessage( uint64_t const device ) -> bytes_t const
{
  m_message_mutex.lock();
  bytes_t temp = m_message;
  m_message_mutex.unlock();

  return temp;
}

auto MinerState::getMidstate( uint64_t (& message_out)[25], uint64_t const device ) -> void
{
  m_message_mutex.lock();
  bytes_t temp = m_message;
  m_message_mutex.unlock();

  uint64_t message[11]{ 0 };

  std::memcpy( message, temp.data(), 84 );

  uint64_t C[5], D[5], mid[25];
  C[0] = message[0] ^ message[5] ^ message[10] ^ 0x100000000ull;
  C[1] = message[1] ^ message[6] ^ 0x8000000000000000ull;
  C[2] = message[2] ^ message[7];
  C[3] = message[3] ^ message[8];
  C[4] = message[4] ^ message[9];

  D[0] = ROTL64(C[1], 1) ^ C[4];
  D[1] = ROTL64(C[2], 1) ^ C[0];
  D[2] = ROTL64(C[3], 1) ^ C[1];
  D[3] = ROTL64(C[4], 1) ^ C[2];
  D[4] = ROTL64(C[0], 1) ^ C[3];

  mid[ 0] = message[ 0] ^ D[0];
  mid[ 1] = ROTL64(message[6] ^ D[1], 44);
  mid[ 2] = ROTL64(D[2], 43);
  mid[ 3] = ROTL64(D[3], 21);
  mid[ 4] = ROTL64(D[4], 14);
  mid[ 5] = ROTL64(message[3] ^ D[3], 28);
  mid[ 6] = ROTL64(message[9] ^ D[4], 20);
  mid[ 7] = ROTL64(message[10] ^ D[0] ^ 0x100000000ull, 3 );
  mid[ 8] = ROTL64(0x8000000000000000ull ^ D[1], 45 );
  mid[ 9] = ROTL64(D[2], 61);
  mid[10] = ROTL64(message[1] ^ D[1],  1);
  mid[11] = ROTL64(message[7] ^ D[2],  6);
  mid[12] = ROTL64(D[3], 25);
  mid[13] = ROTL64(D[4],  8);
  mid[14] = ROTL64(D[0], 18);
  mid[15] = ROTL64(message[4] ^ D[4], 27);
  mid[16] = ROTL64(message[5] ^ D[0], 36);
  mid[17] = ROTL64(D[1], 10);
  mid[18] = ROTL64(D[2], 15);
  mid[19] = ROTL64(D[3], 56);
  mid[20] = ROTL64(message[2] ^ D[2], 62);
  mid[21] = ROTL64(message[8] ^ D[3], 55);
  mid[22] = ROTL64(D[4], 39);
  mid[23] = ROTL64(D[0], 41);
  mid[24] = ROTL64(D[1],  2);

  std::memcpy( message_out, mid, 200 );
}

auto MinerState::getTarget() -> uint64_t const
{
  return m_target;
}

auto MinerState::setAddress( std::string const address ) -> void
{
  m_address_mutex.lock();
  m_address = address;
  m_address_mutex.unlock();
  m_print_mutex.lock();
  m_address_printable = address.substr( 0, 8 );
  m_print_mutex.unlock();
}

auto MinerState::getAddress() -> std::string const
{
  m_address_mutex.lock();
  std::string ret = m_address;
  m_address_mutex.unlock();
  return ret;
}

auto MinerState::setCustomDiff( uint64_t const diff ) -> void
{
  m_custom_diff = true;
  m_diff = diff;
}

auto MinerState::getCustomDiff() -> bool const
{
  return m_custom_diff;
}

auto MinerState::setDiff( uint64_t const diff ) -> void
{
  m_diff = diff;
}

auto MinerState::getDiff() -> uint64_t const
{
  return m_diff;
}

auto MinerState::setPoolAddress( std::string const pool ) -> void
{
  if( pool.find( "mine0xbtc.eu" ) != std::string::npos )
  {
    std::cerr << "Selected pool '" << pool << "' is blocked for deceiving the community and apparent scamming.\n"
              << "Please select a different pool to mine on.\n";
    std::exit( EXIT_FAILURE );
  }
  m_pool_mutex.lock();
  m_pool_address = pool;
  m_pool_mutex.unlock();
}

auto MinerState::getPoolAddress() -> std::string const
{
  m_pool_mutex.lock();
  std::string ret = m_pool_address;
  m_pool_mutex.unlock();
  return ret;
}
