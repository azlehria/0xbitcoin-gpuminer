#include "cudasolver.h"

//we will need this!
#include "cuda_sha3.h"

static const char* const ascii[] = {
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

static uint8_t fromAscii( uint8_t c )
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

static uint8_t ascii_r( uint8_t a, uint8_t b )
{
  return fromAscii( a ) * 16 + fromAscii( b );
}

static void HexToBytes( std::string const& hex, uint8_t bytes[] )
{
  for( std::string::size_type i = 0, j = 0; i < hex.length(); i += 2, ++j )
  {
    bytes[j] = ascii_r( hex[i], hex[i + 1] );
  }
}

// --------------------------------------------------------------------

// static
std::atomic<uint32_t> CUDASolver::hashes( 0u ); // statistics only
std::mutex CUDASolver::m_solutions_mutex;
std::queue<std::string> CUDASolver::m_solutions_queue;

CUDASolver::CUDASolver() noexcept :
m_address( ADDRESS_LENGTH ),
m_challenge( UINT256_LENGTH ),
m_target( 0 ),
m_target_ready( false ),
m_updated_gpu_inputs( false )
{
}

CUDASolver::~CUDASolver()
{
  gpu_cleanup();
}

void CUDASolver::setAddress( std::string const& addr )
{
  assert( addr.length() == ( ADDRESS_LENGTH * 2 + 2 ) );
  hexToBytes( addr, m_address );
  //updateBuffer();

  m_updated_gpu_inputs = true;
  updateGPULoop();
}

void CUDASolver::setChallenge( std::string const& chal )
{
  s_challenge = chal;

  assert( chal.length() == ( UINT256_LENGTH * 2 + 2 ) );
  hexToBytes( chal, m_challenge );
  //updateBuffer();
  m_updated_gpu_inputs = true;
  updateGPULoop();
}

void CUDASolver::setTarget( std::string const& target )
{
  assert( target.length() <= ( UINT256_LENGTH * 2 + 2 ) );
  std::string const t( static_cast<std::string::size_type>( UINT256_LENGTH * 2 + 2 ) - target.length(), '0' );

  s_target = t + target.substr(2);

  m_target = std::stoull( (s_target).substr( 0, 16 ), nullptr, 16 );
  m_target_ready = true;

  m_updated_gpu_inputs = true;
  updateGPULoop();
}

bool CUDASolver::requiresRestart()
{
  return m_updated_gpu_inputs;
}

//This will restart the miner if needed
void CUDASolver::updateGPULoop( bool force_update )
{
  if( force_update ||
      ( m_updated_gpu_inputs
        && m_target_ready
        && m_challenge.size() > 0
        && m_address.size() > 0 ) )
  {
    m_updated_gpu_inputs = false;

    uint8_t hash_prefix[52];
    for( int32_t i = 0; i < 32; i++ )
    {
      hash_prefix[i] = (uint8_t)m_challenge[i];
    }
    for( int32_t i = 0; i < 20; i++ )
    {
      hash_prefix[i + 32] = (uint8_t)m_address[i];
    }

    uint64_t target = m_target;

    update_mining_inputs( target, hash_prefix );
    // stop_solving();
  }
}

// Buffer order: 1-challenge 2-ethAddress 3-solution
/*
void CUDASolver::updateBuffer()
{
  // The idea is to have a double-buffer system in order not to try
  //  to acquire a lock on each hash() loop
  {
    std::lock_guard<std::mutex> g(m_buffer_mutex);
    std::copy(m_challenge.cbegin(), m_challenge.cend(), m_buffer_tmp.begin());
    std::copy(m_address.cbegin(), m_address.cend(), m_buffer_tmp.begin() + m_challenge.size());
  }
  m_buffer_ready = true;
}*/

//call the sha3.cu init func
void CUDASolver::init()
{
  // std::cout << "CUDA initializing ..." << std::endl;
  gpu_init();
}

void CUDASolver::stopFinding()
{
  //set h_done[0] = 1
  stop_solving();
}

void CUDASolver::findSolution()
{
  updateGPULoop( true );

  CUDASolver::bytes_t byte_solution( 32 );
  resetHashCount();

  do
  {
    find_message();


    if( *h_message != UINT64_MAX )
    {
      for( int32_t i = 0; i < 32; i++ )
      {
        byte_solution[i] = solution[i];
      }
      CUDASolver::pushSolution( bytesToString( byte_solution ) );
      resetHashCount();
    }
  } while( *h_message == UINT64_MAX );
}

std::string CUDASolver::hexStr( char* data, int32_t len )
{
  std::stringstream ss;
  ss << std::hex;
  for( int32_t i = 0; i < len; ++i )
    ss << std::setw( 2 ) << std::setfill( '0' ) << (int32_t)data[i];
  return ss.str();
}

// static
void CUDASolver::hexToBytes( std::string const& hex, bytes_t& bytes )
{
  assert( hex.length() % 2 == 0 );
  assert( bytes.size() == ( hex.length() / 2 - 1 ) );
  HexToBytes( hex.substr( 2 ), &bytes[0] );
}

// static
std::string CUDASolver::bytesToString( bytes_t const& buffer )
{
  std::string output;
  output.reserve( buffer.size() * 2 + 1 );

  for( unsigned i = 0; i < buffer.size(); ++i )
    output += ascii[buffer[i]];

  return output;
}

//static
std::string CUDASolver::getSolution()
{
  if( m_solutions_queue.empty() )
    return "";

  std::string ret;
  m_solutions_mutex.lock();
  ret = m_solutions_queue.front();
  m_solutions_queue.pop();
  m_solutions_mutex.unlock();
  return ret;
}

//static
void CUDASolver::pushSolution( std::string sol )
{
  m_solutions_mutex.lock();
  m_solutions_queue.push( sol );
  m_solutions_mutex.unlock();
}
