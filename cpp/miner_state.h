#ifndef _MINER_STATE_H_
#define _MINER_STATE_H_

#include "types.h"
#include "BigInt/BigIntegerLibrary.hh"

#include <string>
#include <vector>
#include <utility>

#define ROTL64(x, y) (((x) << (y)) ^ ((x) >> (64 - (y))))

// this really needs to be broken down
namespace MinerState
{
  auto initState() -> void;

  template<typename T>
  auto hexToBytes( std::string const hex, T& bytes ) -> void;
  template<typename T>
  auto bytesToString( T const buffer ) -> std::string const;

  auto getIncSearchSpace( uint64_t const threads ) -> uint64_t const;
  auto resetCounter() -> void;
  auto getPrintableHashCount() -> uint64_t;
  auto printStatus() -> void;
  auto getPrintableTimeStamp() -> std::string const;

  auto getLog() -> std::string const;
  auto pushLog( std::string message ) -> void;

  auto pushSolution( uint64_t const sol ) -> void;
  auto getSolution() -> std::string const;
  auto incSolCount( uint64_t const count = 1 ) -> void;
  auto getSolCount() -> uint64_t const;

  auto setTarget( std::string const target ) -> void;
  auto getTarget() -> BigUnsigned const;
  auto getTargetNum() -> uint64_t const;
  auto getMaximumTarget() -> BigUnsigned const;

  auto setPrefix( std::string const prefix ) -> void;
  auto getPrefix() -> std::string const;
  auto setChallenge( std::string challenge ) -> void;
  auto getChallenge() -> std::string const;
  auto getPreviousChallenge() -> std::string const;
  auto setPoolAddress( std::string address ) -> void;
  auto getPoolAddress() -> std::string const;
  auto getMessage() -> message_t const;
  auto getMidstate() -> state_t const;

  auto setAddress( std::string const address ) -> void;
  auto getAddress() -> std::string const;

  auto setCustomDiff( uint64_t const diff ) -> void;
  auto getCustomDiff() -> bool const;
  auto setDiff( uint64_t const diff ) -> void;
  auto getDiff() -> uint64_t const;

  auto setPoolUrl( std::string const pool ) -> void;
  auto getPoolUrl() -> std::string const;

  auto getCudaDevices() -> std::vector<std::pair<int32_t, double>> const;
  auto getCpuThreads() -> uint32_t const;

  auto setTokenName( std::string const token ) -> void;
  auto getTokenName() -> std::string const;

  auto setSubmitStale( bool const submitStale ) -> void;
  auto getSubmitStale() -> bool const;

  auto isReady() -> bool const;

  auto keccak256( std::string const message ) -> std::string const;
};

#endif // !_MINER_STATE_H_
