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

char constexpr MINER_VERSION[] = "2.10.3";

class HybridMiner
{
public:
  HybridMiner() noexcept;
  ~HybridMiner();

  auto updateTarget() const -> void;
  auto updateMessage() const -> void;

  auto run() -> void;
  auto stop() -> void;

  auto getHashrates() const -> double const;

private:
  auto startMining() -> void;

  auto printUiBase() const -> void;

  std::vector<std::unique_ptr<CPUSolver>> cpuSolvers;
  std::vector<std::unique_ptr<CUDASolver>> cudaSolvers;

  std::string m_hardwareType;

  std::atomic<bool> m_old_ui;
  std::atomic<bool> m_stop;
};

#endif // ! _CPUMINER_H_
