#ifndef _CUDASOLVER_H_
#define _CUDASOLVER_H_

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

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
  void setDifficulty( uint64_t const& diff );
  void setTarget( std::string const& target );

  void init();

  bytes_t findSolution();
  void stopFinding();
  bool trySolution( bytes_t const& solution );

  static void hexToBytes( std::string const& hex, bytes_t& bytes );
  static std::string bytesToString( bytes_t const& buffer );
  static std::string hexStr( char* data, int32_t len );

  // Implement this for verification or pass to CPUSolver somehow?
  static bool lte( bytes_t const& left, bytes_t const& right );

  bool requiresRestart();

private:
  //void updateBuffer();

  void updateGPULoop( bool force_update = false );
  // Implement this for verification or pass to CPUSolver somehow?
  void hash( bytes_t const& solution, bytes_t& digest );

  std::string s_challenge;
  std::string s_target;
  bytes_t m_address;
  bytes_t m_challenge;
  bytes_t m_target, m_target_tmp;
  bytes_t m_buffer, m_buffer_tmp;
  uint64_t m_diff, m_diff_tmp;
  std::mutex m_buffer_mutex;
  std::mutex m_target_mutex;
  std::atomic<bool> m_buffer_ready;
  std::atomic<bool> m_target_ready;
  std::atomic<bool> m_diff_ready;

  std::atomic<bool> m_updated_gpu_inputs;
};

#endif // !_SOLVER_H_
