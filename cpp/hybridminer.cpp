#include <fstream>
#include "hybridminer.h"
#include "json.hpp"

#ifdef _MSC_VER
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif // _MSC_VER

HybridMiner::HybridMiner() noexcept :
m_init_complete( false ),
m_old_ui( []() -> bool
          {
#ifdef _MSC_VER
            OSVERSIONINFO winVer;
            ZeroMemory( &winVer, sizeof(OSVERSIONINFO) );
            winVer.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

// Stop deprecating things you don't have a _full_ replacement for!
#pragma warning( push )
#pragma warning( disable: 4996 )
            GetVersionEx( &winVer );
#pragma warning( pop )

            if( ( winVer.dwMajorVersion < 10 ) ||
                ( winVer.dwMajorVersion >= 10 &&
                  winVer.dwBuildNumber < 14392 ) )
            {
              return true;
            }
#endif // _MSC_VER
            return false;
          }() )
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
#ifdef _MSC_VER
  SetConsoleTitle((std::string("0xBitcoin Miner v") + std::string(MINER_VERSION)).c_str());
#endif // _MSC_VER

  std::ifstream in("0xbitcoin.json");
  if( !in )
  {
    std::cerr << "Unable to open configuration file '0xbitcoin.json'.\n";
    std::exit( EXIT_FAILURE );
  }

  nlohmann::json jsConf;
  in >> jsConf;
  in.close();

  if( jsConf.find( "address" ) == jsConf.end() || jsConf["address"].get<std::string>().length() != 42 )
  {
    std::cerr << "No valid wallet address set in configuration - how are you supposed to get paid?\n";
    std::exit( EXIT_FAILURE );
  }
  if( jsConf.find( "pool" ) == jsConf.end() || jsConf["pool"].get<std::string>().length() < 15 )
  {
    std::cerr << "No pool address set in configuration - this isn't a solo miner!\n";
    std::exit( EXIT_FAILURE );
  }

  MinerState::setAddress( jsConf["address"] );
  MinerState::setPoolAddress( jsConf["pool"] );
  if( jsConf.find( "customdiff" ) != jsConf.end() && jsConf["customdiff"] > 0u )
  {
    MinerState::setCustomDiff( jsConf["customdiff"] );
  }

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

  m_threads.emplace_back( [&] {
      using namespace std::chrono;

      this->printUiBase();

      do {
        auto timerNext = steady_clock::now() + 100ms;

        MinerState::printStatus();

        std::this_thread::sleep_until( timerNext );
      } while( true );
    } );

  m_init_complete = true;

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

auto HybridMiner::printUiBase() const -> void
{
  if( !m_old_ui )
  {
    std::cout << "\x1b[?25l\x1b[2J\x1b(0"
              << "\x1b[1;1flqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqwqqqqqqqqqqqqqqqqqqqqqqqqqqwqqqqqqqqqqqqqqqqqk"
              << "\x1b[4;1fmqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqvqqqqqqqqqqqqqqqqqqqqqqqqqqvqqqqqqqqqqqqqqqqqj"
              << "\x1b[2;1fx\x1b[2;35fx\x1b[2;62fx\x1b[2;80fx"
              << "\x1b[3;1fx\x1b[3;35fx\x1b[3;62fx\x1b[3;80fx"
              << "\x1b(B\x1b[2;2fChallenge:"
              << "\x1b[3;2fDifficulty:"
              << "\x1b[2;37fHashes this round"
              << "\x1b[2;63fRound time:"
              << "\x1b[3;63fAccount:"
              << "\x1b[2;31fMH/s"
              << "\x1b[3;31fSols"
              << "\x1b[s\x1b[3;29f\x1b[38;5;221m0\x1b[0m\x1b[u"
              << "\x1b[1;64fv" << MINER_VERSION
              << "\x1b]2;0xBitcoin Miner v" << MINER_VERSION << "\x07"
              << "\x1b[5r\x1b[5;1f\x1b[?25h";
  }

  std::cout << "Mining on " << cudaSolvers.size() << " GPUs using CUDA.\n"
            << (m_old_ui ? '\n' : '\r');
}

auto HybridMiner::isInitComplete() const -> bool
{
  return m_init_complete;
}
