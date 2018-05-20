/*
  Header file to declare prototypes

*/

#ifndef  _HYBRIDMINER_H_
#define  _HYBRIDMINER_H_

#include <vector>

char constexpr MINER_VERSION[] = "1.0.0";

namespace HybridMiner
{
  auto updateTarget() -> void;
  auto updateMessage() -> void;

  auto run() -> void;
  auto stop() -> void;

  auto getHashrates() -> std::vector<double> const;
};

#endif // ! _CPUMINER_H_
