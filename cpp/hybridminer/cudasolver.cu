#include "cudasolver.h"

#include <assert.h>
#include <sstream>
#include <iomanip>
#include <stdio.h>
#include <iostream>
#include <string.h>

//we will need this!
#include "cuda_sha3.cu"

extern int h_done[1];
extern unsigned char* h_message;

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

CUDASolver::CUDASolver() noexcept :
m_address( ADDRESS_LENGTH ),
m_challenge( UINT256_LENGTH ),
m_target( UINT256_LENGTH ),
m_target_tmp( UINT256_LENGTH ),
m_buffer( ADDRESS_LENGTH + 2 * UINT256_LENGTH ),
m_buffer_tmp( ADDRESS_LENGTH + 2 * UINT256_LENGTH ), //this has something to do with updateBuffer
m_buffer_ready( false ),
m_target_ready( false ),
m_updated_gpu_inputs( false )
{
}

void CUDASolver::setAddress( std::string const& addr )
{
  std::cout << "Setting cuda addr " << std::endl;

  assert( addr.length() == ( ADDRESS_LENGTH * 2 + 2 ) );
  hexToBytes( addr, m_address );
  //updateBuffer();

  m_updated_gpu_inputs = true;
  updateGPULoop();
}

void CUDASolver::setChallenge( std::string const& chal )
{
  //std::cout << "Setting cuda chal " << std::endl;

  s_challenge = chal;

  assert( chal.length() == ( UINT256_LENGTH * 2 + 2 ) );
  hexToBytes( chal, m_challenge );
  //updateBuffer();
  m_updated_gpu_inputs = true;
  updateGPULoop();
}

void CUDASolver::setTarget( std::string const& target )
{
  std::cout << "Setting cuda tar " << target << std::endl;

  assert( target.length() <= ( UINT256_LENGTH * 2 + 2 ) );
  std::string const t( static_cast<std::string::size_type>( UINT256_LENGTH * 2 + 2 ) - target.length(), '0' );

  s_target = target;

  // Double-buffer system, the trySolution() function will be blocked
  //  only when a change occurs.
  {
    std::lock_guard<std::mutex> g( m_target_mutex );
    hexToBytes( "0x" + t + target.substr( 2 ), m_target_tmp );
  }
  m_target_ready = true;

  m_updated_gpu_inputs = true;
  updateGPULoop();
}

bool CUDASolver::requiresRestart()
{
  return m_updated_gpu_inputs;
}

//This will restart the miner if needed
void CUDASolver::updateGPULoop()
{
  if( m_updated_gpu_inputs
      && m_target_ready
      && m_challenge.size() > 0
      && m_address.size() > 0 )
  {
    m_updated_gpu_inputs = false;

    //printf( "Target input:\n" );

    if( s_target.length() < 66 )
    {
      std::string zeros = std::string( 66 - s_target.length(), '0' );
      std::string s = "0x" + zeros + s_target.substr( 2, s_target.length() );
      s_target = s;
    }

    unsigned char target_input[64];
    bytes_t target_bytes( 32 );

    hexToBytes( s_target, target_bytes );

    for( int i = 0; i < 32; i++ )
    {
      target_input[i] = (unsigned char)target_bytes[i];
      //printf( "%02x", (unsigned char)target_input[i] );
    }

    unsigned char hash_prefix[52];
    std::string clean_challenge = s_challenge;
    bytes_t challenge_bytes( 32 );

    hexToBytes( clean_challenge, challenge_bytes );

    for( int i = 0; i < 32; i++ )
    {
      hash_prefix[i] = (unsigned char)challenge_bytes[i];
    }
    for( int i = 0; i < 20; i++ )
    {
      hash_prefix[i + 32] = (unsigned char)m_address[i];
    }

    //printf( "Challenge+Address:\n" );
    //for( int i = 0; i < 52; i++ )
    //{
    //  printf( "%02x", (unsigned char)hash_prefix[i] );
    //}
    //printf( "\n/prefix\n" );

    //printf( "Updating mining inputs\n" );
    update_mining_inputs( (const char *)target_input, (const char *)hash_prefix );
    stop_solving();
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
  std::cout << "CUDA initializing ..." << std::endl;
  gpu_init();
}

void CUDASolver::stopFinding()
{
  //std::cout << "CUDA has stopped hashing for now." << std::endl;

  //set h_done[0] = 1
  stop_solving();
}

CUDASolver::bytes_t CUDASolver::findSolution()
{
  m_updated_gpu_inputs = false;

  //std::cout << "CUDA is trying to find a solution :)" << std::endl;

  // What are these even here for?
  //cudaEventCreate( &start );
  //cudaEventCreate( &stop );

  //printf( "Target input:\n" );

  if( s_target.length() < 66 )
  {
    std::string zeros = std::string( 66 - s_target.length(), '0' );
    std::string s = "0x" + zeros + s_target.substr( 2, s_target.length() );
    s_target = s;
  }

  unsigned char  target_input[64];
  bytes_t target_bytes( 32 );

  hexToBytes( s_target, target_bytes );

  for( int i = 0; i < 32; i++ )
  {
    target_input[i] = (unsigned char)target_bytes[i];
    //printf( "%02x", (unsigned char)target_input[i] );
  }

  unsigned   char  hash_prefix[52];
  std::string clean_challenge = s_challenge;
  bytes_t challenge_bytes( 32 );

  hexToBytes( clean_challenge, challenge_bytes );

  for( int i = 0; i < 32; i++ )
  {
    hash_prefix[i] = (unsigned char)challenge_bytes[i];
  }
  for( int i = 0; i < 20; i++ )
  {
    hash_prefix[i + 32] = (unsigned char)m_address[i];
  }

  //printf( "\nChallenge+Address:\n" );
  //for( int i = 0; i < 52; i++ )
  //{
  //  printf( "%02x", (unsigned char)hash_prefix[i] );
  //}
  ////printf( "\n/prefix\n" );

  CUDASolver::bytes_t byte_solution( 32 );
  h_done[0] = 0;

  do
  {
    if( !find_message( (const char *)target_input, (const char *)hash_prefix ) )
      continue;

    //here
    for( int i = 52; i < 84; i++ )
    {
      byte_solution[i - 52] = (uint8_t)h_message[i];

      //cout << (uint8_t)s_solution[i] << "\n";
    }
  } while( !h_done[0] );
  gpu_cleanup();

  // What are these even here for?
  //cudaEventDestroy( start );
  //cudaEventDestroy( stop );

  return byte_solution;
}

std::string CUDASolver::hexStr( char* data, int len )
{
  std::stringstream ss;
  ss << std::hex;
  for( int i = 0; i < len; ++i )
    ss << std::setw( 2 ) << std::setfill( '0' ) << (int)data[i];
  return ss.str();
}

// static
void CUDASolver::hexToBytes( std::string const& hex, bytes_t& bytes )
{
  /*
      cout << "hex to bytes: " << hex << "\n";
      cout << bytes.size()  << "\n";
      cout << hex.length()  << "\n";
  */
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
