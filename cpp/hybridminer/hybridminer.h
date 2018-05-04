/*
  Header file to declare prototypes

*/

#ifndef  _HYBRIDMINER_H_
#define  _HYBRIDMINER_H_

#include "cpusolver.h"
#include "cudasolver.h"
#include "miner_state.h"

#include <memory>
#include <chrono>
#include <random>
#include <thread>
#include <string>
#include <cuda_runtime.h>

auto constexpr MINER_VERSION = "2.10.3";

class HybridMiner
{
public:
  HybridMiner() noexcept;
  ~HybridMiner();

  auto setHardwareType( std::string const& hardwareType ) -> void;
  auto updateTarget() const -> void;
  auto updateMessage() const -> void;

  auto isInitComplete() const -> bool;

  auto run() -> void;
  auto stop() -> void;

private:
  //set a var in the solver !!
  // void set( void ( CPUSolver::*fn )( std::string const& ), std::string const& p ) const -> void;
  auto set( void ( CUDASolver::*fn )() ) const -> void;

  auto isUsingCuda() const -> bool;

  auto printUiBase() const -> void;

  std::vector<std::unique_ptr<CPUSolver>> m_solvers;
  std::vector<std::unique_ptr<CUDASolver>> cudaSolvers;
  std::vector<std::thread> m_threads;

  std::string m_hardwareType;
  std::string m_json_config;

  std::atomic<bool> m_init_complete;
  std::atomic<bool> m_old_ui;
};

#endif // ! _CPUMINER_H_
