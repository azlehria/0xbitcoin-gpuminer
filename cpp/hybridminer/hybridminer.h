/*
  Header file to declare prototypes

*/

#ifndef  _HYBRIDMINER_H_
#define  _HYBRIDMINER_H_

#include "cpusolver.h"
#include "cudasolver.h"

#include <chrono>
#include <random>
#include <thread>
#include <string>

class HybridMiner
{
public:
  std::string solution() const;
  std::string getSolution();

  HybridMiner() noexcept;
  ~HybridMiner();

  void setChallengeNumber( std::string const& challengeNumber );
  void setDifficultyTarget( std::string const& difficultyTarget );
  void setMinerAddress( std::string const& minerAddress );
  void setHardwareType( std::string const& hardwareType );

  void run();
  void stop();

private:
  void thr_func( CPUSolver& solver );
  void solutionFound( CPUSolver::bytes_t const& solution );

  //set a var in the solver !!
  void set( void ( CPUSolver::*fn )( std::string const& ), std::string const& p );

  bool isUsingCuda();

  std::vector<CPUSolver> m_solvers;
  std::vector<std::thread> m_threads;

  CUDASolver cudaSolver;
  std::mutex m_solution_mutex;
  CPUSolver::bytes_t m_solution; //make one for GPU ?

//  GPUSolver gpuSolver;
  bool m_bSolutionFound;
  std::string m_hardwareType;
  volatile bool m_bExit;
};

#endif // ! _CPUMINER_H_
