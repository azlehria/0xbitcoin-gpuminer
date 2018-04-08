#ifndef _CUDASOLVER_H_
#define _CUDASOLVER_H_

#include <cassert>
#include <sstream>
#include <iomanip>
#include <atomic>
#include <mutex>
#include <string>
#include <vector>
#include <queue>

class CUDASolver
{
public:
  static std::atomic<uint32_t> hashes;
  typedef std::vector<uint8_t> bytes_t;

  static const unsigned short ADDRESS_LENGTH = 20u;
  static const unsigned short UINT256_LENGTH = 32u;

  CUDASolver() noexcept;
  ~CUDASolver();

  void setAddress( std::string const& addr );
  void setChallenge( std::string const& chal );
  // void setDifficulty( uint64_t const& diff );
  void setTarget( std::string const& target );

  void init();

  void findSolution();
  void stopFinding();

  static void hexToBytes( std::string const& hex, bytes_t& bytes );
  static std::string bytesToString( bytes_t const& buffer );
  static std::string hexStr( char* data, int32_t len );

  bool requiresRestart();

  static std::string getSolution();

private:
  //void updateBuffer();

  void updateGPULoop( bool force_update = false );

  static void pushSolution( std::string sol );

  std::string s_challenge;
  std::string s_target;
  bytes_t m_address;
  bytes_t m_challenge;
  std::atomic<uint64_t> m_target;
  std::atomic<bool> m_target_ready;

  std::atomic<bool> m_updated_gpu_inputs;

  static std::mutex m_solutions_mutex;
  static std::queue<std::string> m_solutions_queue;
};

#endif // !_SOLVER_H_
