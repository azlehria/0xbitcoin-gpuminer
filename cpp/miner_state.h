#ifndef _MINER_STATE_H_
#define _MINER_STATE_H_

#include <cassert>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <vector>
#include <queue>
#include <random>
#include <atomic>
#include <string>
#include <chrono>
#include <array>
#include <utility>

#include "BigInt/BigIntegerLibrary.hh"
#include "json.hpp"
#include "types.h"

#define ROTL64(x, y) (((x) << (y)) ^ ((x) >> (64 - (y))))

// this really needs to be broken down
class MinerState
{
public:
  MinerState() = delete;

  static auto initState() -> void;

  template<typename T>
  static auto hexToBytes( std::string const hex, T& bytes ) -> void;
  template<typename T>
  static auto bytesToString( T const buffer ) -> std::string const;

  static auto getIncSearchSpace( uint64_t const threads ) -> uint64_t const;
  static auto resetCounter() -> void;
  static auto getPrintableHashCount() -> uint64_t;
  static auto printStatus() -> void;
  static auto getPrintableTimeStamp() -> std::string const;

  static auto getLog() -> std::string const;
  static auto pushLog( std::string message ) -> void;

  static auto pushSolution( uint64_t const sol ) -> void;
  static auto getSolution() -> std::string const;
  static auto incSolCount( uint64_t const count = 1 ) -> void;
  static auto getSolCount() -> uint64_t const;

  static auto setTarget( std::string const target ) -> void;
  static auto getTarget() -> BigUnsigned const;
  static auto getTargetNum() -> uint64_t const;

  static auto setPrefix( std::string const prefix ) -> void;
  static auto getPrefix() -> std::string const;
  static auto getChallenge() -> std::string const;
  static auto getPreviousChallenge() -> std::string const;
  static auto getPoolAddress() -> std::string const;
  static auto getMessage() -> message_t const;
  static auto getMidstate() -> state_t const;

  static auto setAddress( std::string const address ) -> void;
  static auto getAddress() -> std::string const;

  static auto setCustomDiff( uint64_t const diff ) -> void;
  static auto getCustomDiff() -> bool const;
  static auto setDiff( uint64_t const diff ) -> void;
  static auto getDiff() -> uint64_t const;

  static auto setPoolUrl( std::string const pool ) -> void;
  static auto getPoolUrl() -> std::string const;

  static auto getCudaDevices() -> std::vector<std::pair<int32_t, double>> const;
  static auto getCpuThreads() -> uint32_t const;

  static auto setTokenName( std::string const token ) -> void;
  static auto getTokenName() -> std::string const;

  static auto setSubmitStale( bool const submitStale ) -> void;
  static auto getSubmitStale() -> bool const;

  static auto isReady() -> bool const;

  static auto keccak256( std::string const message ) -> std::string const;

private:
  typedef std::lock_guard<std::mutex> guard;

  static uint_fast8_t constexpr PREFIX_LENGTH{ 52u };
  static uint_fast8_t constexpr UINT256_LENGTH{ 32u };
  static uint_fast8_t constexpr MESSAGE_LENGTH{ 84u };
  static double constexpr DEFAULT_INTENSITY{ 23.0 };

  static state_t m_midstate;
  static message_t m_message;
  static hash_t m_challenge_old;
  static std::mutex m_message_mutex;
  static std::atomic<bool> m_message_ready;

  static std::atomic<uint64_t> m_target_num;
  static BigUnsigned m_target;
  static BigUnsigned m_maximum_target;
  static std::mutex m_target_mutex;
  static std::atomic<bool> m_custom_diff;
  static std::atomic<uint64_t> m_diff;
  static std::atomic<bool> m_diff_ready;

  static hash_t m_solution;

  static std::chrono::steady_clock::time_point m_start;
  static std::chrono::steady_clock::time_point m_end;
  static std::chrono::steady_clock::time_point m_round_start;

  static std::queue<uint64_t> m_solutions_queue;
  static std::mutex m_solutions_mutex;

  static std::atomic<uint64_t> m_hash_count;
  static std::atomic<uint64_t> m_hash_count_printable;

  static std::atomic<uint64_t> m_sol_count;
  static std::atomic<bool> m_new_solution;

  static std::queue<std::string> m_log;
  static std::mutex m_log_mutex;

  static std::string m_challenge_printable;
  static std::string m_address_printable;
  static std::mutex m_print_mutex;

  static std::string m_address;
  static std::mutex m_address_mutex;

  static std::string m_pool_url;
  static std::mutex m_pool_url_mutex;

  static std::atomic<bool> m_old_ui;

  static std::vector<std::pair<int32_t, double>> m_cuda_devices;
  static std::atomic<uint32_t> m_cpu_threads;
  static nlohmann::json m_json_config;

  static std::string m_token_name;
  static std::atomic<bool> m_submit_stale;
};

#endif // !_MINER_STATE_H_
