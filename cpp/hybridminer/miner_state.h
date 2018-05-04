#ifndef _MINER_STATE_H_
#define _MINER_STATE_H_

#include <cstdio>
#include <cassert>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <vector>
#include <queue>
#include <random>
#include <atomic>
#include <string>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>
#include <chrono>

#define ROTL64(x, y) (((x) << (y)) ^ ((x) >> (64 - (y))))

class MinerState
{
public:
  typedef std::vector<uint8_t> bytes_t;
  static const uint_fast8_t PREFIX_LENGTH = 52u;
  static const uint_fast8_t UINT256_LENGTH = 32u;
  static const uint_fast8_t MESSAGE_LENGTH = 84u;

  MinerState() = delete;

  static auto initState() -> void;

  static auto hexToBytes( std::string const hex, bytes_t& bytes ) -> void;
  static auto bytesToString( bytes_t const buffer ) -> std::string;
  static auto hexStr( uint8_t const* data, int32_t const len ) -> std::string;

  static auto getIncSearchSpace( uint64_t const threads ) -> uint64_t;
  static auto resetCounter() -> void;
  static auto getPrintableHashCount() -> uint64_t;
  static auto printStatus( bool old_ui ) -> void;
  static auto getPrintableTimeStamp() -> std::string const;

  static auto pushSolution( uint64_t const sol ) -> void;
  static auto getSolution() -> std::string const;
  static auto incSolCount( uint64_t const count = 1 ) -> void;
  static auto getSolCount() -> uint64_t const;

  static auto setTarget( std::string const target ) -> void;
  static auto getTarget() -> uint64_t const;

  static auto setPrefix( std::string const prefix ) -> void;
  static auto getMessage( uint64_t const device = 0 ) -> bytes_t const;
  static auto getMidstate( uint64_t (& message_out)[25], uint64_t const device = 0 ) -> void;

  static auto setAddress( std::string const address ) -> void;
  static auto getAddress() -> std::string const;

  static auto setCustomDiff( uint64_t const diff ) -> void;
  static auto getCustomDiff() -> bool const;
  static auto setDiff( uint64_t const diff ) -> void;
  static auto getDiff() -> uint64_t const;

  static auto setPoolAddress( std::string const pool ) -> void;
  static auto getPoolAddress() -> std::string const;

private:
  static bytes_t m_message;
  static std::mutex m_message_mutex;

  static std::atomic<uint64_t> m_target;
  static std::atomic<bool> m_custom_diff;
  static std::atomic<uint64_t> m_diff;

  static std::string m_solution_start;
  static std::string m_solution_end;

  static std::chrono::steady_clock::time_point m_start;
  static std::chrono::steady_clock::time_point m_end;

  static std::queue<uint64_t> m_solutions_queue;
  static std::mutex m_solutions_mutex;

  static std::atomic<uint64_t> m_hash_count;
  static std::atomic<uint64_t> m_hash_count_printable;
  static std::atomic<uint64_t> m_hash_count_samples;
  static std::atomic<double> m_hash_average;

  static std::atomic<uint64_t> m_sol_count;
  static std::atomic<bool> m_new_solution;

  static std::string m_challenge_printable;
  static std::string m_address_printable;
  static std::mutex m_print_mutex;

  static std::string m_address;
  static std::mutex m_address_mutex;

  static std::string m_pool_address;
  static std::mutex m_pool_mutex;
};

#endif // !_MINER_STATE_H_
