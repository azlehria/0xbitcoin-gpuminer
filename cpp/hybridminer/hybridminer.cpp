#include <fstream>
#include "hybridminer.h"
#include "json.hpp"

HybridMiner::HybridMiner() noexcept
{
  MinerState::initState();
}

HybridMiner::~HybridMiner()
{
  stop();

  // Wait for run() to terminate
  //  This is not very clean but it's the easiest portable way to
  //  exit gracefully if stop() has not been called before the destructor.
  std::this_thread::yield();
  for( auto&& thr : m_threads )
  {
    if( !thr.joinable() )
      std::this_thread::sleep_for( std::chrono::milliseconds( 50u ) );
  }
}

//set the hardware type to 'cpu' or 'gpu'
auto HybridMiner::setHardwareType( std::string const& hardwareType ) -> void
{
  m_hardwareType = hardwareType;
}

auto HybridMiner::updateTarget() const -> void
{
  if( isUsingCuda() )
  {
    set( &CUDASolver::updateTarget );
  }
}

auto HybridMiner::updateMessage() const -> void
{
  if( isUsingCuda() )
  {
    set( &CUDASolver::updateMessage );
  }
}

// This is a the "main" thread of execution
auto HybridMiner::run() -> void
{
  std::ifstream in("0xbitcoin.json");
  nlohmann::json jsConf;
  in >> jsConf;
  in.close();

  MinerState::setAddress( jsConf["address"] );

  if( isUsingCuda() )
  {
    int32_t device_count;
    cudaGetDeviceCount( &device_count );

    if( jsConf.find( "cuda" ) != jsConf.end() && jsConf["cuda"].size() > 0u )
    {
      for( auto& device : jsConf["cuda"] )
      {
        if( device["enabled"] && device["device"] < device_count )
        {
          cudaSolvers.push_back( std::make_unique<CUDASolver>( device["device"],
                                                               device["intensity"] ) );
        }
      }
    }
    else
    {
      for( int_fast32_t i{ 0u }; i < device_count; ++i )
      {
        cudaSolvers.push_back( std::make_unique<CUDASolver>( i, INTENSITY ) );
      }
    }

    for( const auto& solver : cudaSolvers )
    {
      m_threads.emplace_back( [&] { solver->findSolution(); } );
    }
  }
  else
  {
    if( jsConf.find( "threads" ) != jsConf.end() && jsConf["threads"] > 0u )
    {
      for( uint_fast32_t i{ 0u }; i < jsConf["threads"]; ++i)
      {
        m_solvers.push_back( std::make_unique<CPUSolver>() );
      }
    }
    else
    {
      for( uint_fast32_t i{ 0u }; i < std::thread::hardware_concurrency() - 1; ++i )
      {
        m_solvers.push_back( std::make_unique<CPUSolver>() );
      }
    }

    // These are the Solver threads
    for( const auto& solver : m_solvers )
    {
      m_threads.emplace_back( [&] { solver->findSolution(); } );
    }
  }

  for( auto&& thr : m_threads )
  {
    thr.join();
  }
}

auto HybridMiner::stop() -> void
{
  if( isUsingCuda() )
  {
    for( auto&& i : cudaSolvers )
      ( (*i).*(&CUDASolver::stopFinding) )();
  }
  else
  {
    for( auto&& i : m_solvers )
      ( (*i).*(&CPUSolver::stopFinding) )();
  }
}

// //edit a variable within each of the solvers
// void HybridMiner::set( void ( CPUSolver::*fn )( std::string const& ), std::string const& p ) const
// {
//   for( auto&& i : m_solvers )
//     ( (*i).*fn )( p );
// }

//edit a variable within each of the solvers
auto HybridMiner::set( void ( CUDASolver::*fn )() ) const -> void
{
  for( auto&& i : cudaSolvers )
    ( (*i).*fn )();
}

auto HybridMiner::isUsingCuda() const -> bool
{
  return m_hardwareType == "cuda";
}
