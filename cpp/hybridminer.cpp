#include "hybridminer.h"
#include "commo.h"
#include "isolver.h"
#include "cpusolver.h"
#include "cudasolver.h"
#include "clsolver.h"
#include "miner_state.h"

#include "json.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <memory>
#include <chrono>
#include <random>
#include <thread>
#include <string>

using namespace std::literals::string_literals;
using namespace std::chrono;

#ifdef _MSC_VER
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif // _MSC_VER

namespace
{
  static std::vector<std::unique_ptr<ISolver>> m_solvers;

  static uint_fast16_t m_solvers_cuda{ 0u };
  static uint_fast16_t m_solvers_cpu{ 0u };
  static uint_fast16_t m_solvers_cl{ 0u };

  static std::atomic<bool> m_old_ui{ []() -> bool
  {
#ifdef _MSC_VER
    OSVERSIONINFO winVer;
    ZeroMemory( &winVer, sizeof( OSVERSIONINFO ) );
    winVer.dwOSVersionInfoSize = sizeof( OSVERSIONINFO );

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
  }( ) };
  static std::atomic<bool> m_stop{ false };
  static std::atomic<bool> m_stopped{ false };

  static auto printUiBase() -> void
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
        << "\x1b]2;Nabiki v" << MINER_VERSION << "\x07"
        << "\x1b[5r\x1b[5;1f\x1b[?25h";
    }

    std::stringstream ss_out;
    ss_out << "Mining on "s;
    if( m_solvers_cuda > 0u )
    {
      ss_out << m_solvers_cuda
        << " GPU"s << (m_solvers_cuda > 1 ? "s"s : ""s) << " using CUDA"s;
    }
    if( m_solvers_cuda > 0u && m_solvers_cl > 0u )
    {
      ss_out << " and"s << (m_solvers_cpu > 0u ? ", "s : " "s);
    }
    if( m_solvers_cl > 0u )
    {
      ss_out << m_solvers_cl
        << " GPU"s << ( m_solvers_cl > 1 ? "s"s : ""s ) << " using OpenCL"s;
    }
    if( m_solvers_cl > 0u && m_solvers_cpu > 0u )
    {
      ss_out << (m_solvers_cuda > 0u ? ", "s : " "s) << "and "s;
    }
    if( m_solvers_cpu > 0u )
    {
      ss_out << m_solvers_cpu << " CPU core"s << (m_solvers_cpu > 1 ? "s"s : ""s);
    }

    ss_out << (m_old_ui ? ".\n"s : ".\r"s);

    MinerState::pushLog( ss_out.str() );
  }

  static auto startMining() -> void
  {
    while( !MinerState::isReady() )
    {
      std::this_thread::sleep_for( 10ms );
    }
    for( auto& device : MinerState::getCudaDevices() )
    {
      m_solvers.push_back( std::make_unique<CUDASolver>( device.first,
                                                         device.second ) );
      ++m_solvers_cuda;
    }
    for( m_solvers_cpu = 0; m_solvers_cpu < MinerState::getCpuThreads(); ++m_solvers_cpu )
    {
      m_solvers.push_back( std::make_unique<CPUSolver>() );
    }
    //{
    //  std::vector<cl::Platform> platform;
    //  cl_int error = cl::Platform::get( &platform );
    //  std::vector<cl::Device> pldev;
    //  error = platform[1].getDevices( CL_DEVICE_TYPE_ALL, &pldev );

    //  m_solvers.push_back( std::make_unique<CLSolver>( pldev[0], 15 ) );
    //  ++m_solvers_cl;
    //}
  }

#ifdef _MSC_VER
  static BOOL WINAPI SignalHandler( DWORD dwSig )
  {
    if( dwSig == CTRL_C_EVENT || dwSig == CTRL_BREAK_EVENT || dwSig == CTRL_CLOSE_EVENT )
    {
      m_stop = true;
      return TRUE;
    }
    return FALSE;
  }
#else
#  include <signal.h>
#  include <termios.h>
  static termios term_original;

  static void SignalHandler( int signal )
  {
    m_stop = true;
    tcsetattr( 0, TCSANOW, &term_original );
    return;
  }
#endif // _MSC_VER

  static auto setBasicState() -> void
  {
#ifdef _MSC_VER
    {
      HANDLE hOut = GetStdHandle( STD_OUTPUT_HANDLE );
      DWORD dwMode = 0;
      GetConsoleMode( hOut, &dwMode );
      dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
      SetConsoleMode( hOut, dwMode );
      SetConsoleTitleA( ( "0xBitcoin Miner v"s + std::string( MINER_VERSION ) ).c_str() );
      SetConsoleCtrlHandler( SignalHandler, TRUE );
    }
#else
    struct termios term_new;
    tcgetattr( 0, &term_original );
    std::memcpy( &term_new, &term_original, sizeof(struct termios) );
    term_new.c_lflag &= ~ECHO;
    tcsetattr( 0, TCSANOW, &term_new );

    struct sigaction sig_handler;

    sig_handler.sa_handler = &SignalHandler;

    sigemptyset( &sig_handler.sa_mask );

    sigaddset( &sig_handler.sa_mask, SIGINT );
    sigaddset( &sig_handler.sa_mask, SIGTERM );
    sigaddset( &sig_handler.sa_mask, SIGHUP );
    sigaddset( &sig_handler.sa_mask, SIGQUIT );

    sigaction( SIGINT, &sig_handler, NULL );
    sigaction( SIGTERM, &sig_handler, NULL );
    sigaction( SIGHUP, &sig_handler, NULL );
    sigaction( SIGQUIT, &sig_handler, NULL );
#endif // _MSC_VER
  }
}

auto HybridMiner::updateTarget() -> void
{
  for( auto&& solver : m_solvers )
  {
    solver->updateTarget();
  }
}

auto HybridMiner::updateMessage() -> void
{
  for( auto&& solver : m_solvers )
  {
    solver->updateMessage();
  }
}

// This is the "main" thread of execution
auto HybridMiner::run() -> void
{
  setBasicState();

  MinerState::initState();

  Commo::Init();

  startMining();

  printUiBase();

  std::thread printer{ [] {
  do
  {
    auto timerNext = steady_clock::now() + 100ms;

    MinerState::printStatus();

    std::this_thread::sleep_until( timerNext );
  } while( !m_stop ); } };

  printer.join();

  std::cerr << MinerState::getPrintableTimeStamp() << "Process exiting... stopping miner\n"s;

  stop();

  if( !m_old_ui )
  {
    std::cerr << "\x1b[s\x1b[?25h\x1b[r\x1b[u";
  }

  Commo::Cleanup();
  m_stopped = true;
}

auto HybridMiner::stop() -> void
{
  m_solvers.clear();

  m_solvers_cuda = m_solvers_cpu = 0u;
}

auto HybridMiner::getHashrates() -> std::vector<double> const
{
  std::vector<double> temp;
  for( auto&& solver : m_solvers )
  {
    temp.emplace_back( solver->getHashrate() );
  }
  return temp;
}
