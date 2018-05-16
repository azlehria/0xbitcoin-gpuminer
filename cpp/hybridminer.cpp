#include <cstdlib>
#include <fstream>
#include <sstream>
#include "addon.h"
#include "hybridminer.h"
#include "json.hpp"

using namespace std::literals::string_literals;
using namespace std::chrono;

void ExitHandler()
{
  miner::cleanup( miner::hybridminer );
}

#ifdef _MSC_VER
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>

BOOL WINAPI SignalHandler( DWORD dwSig )
{
  if( dwSig == CTRL_C_EVENT || dwSig == CTRL_BREAK_EVENT || dwSig == CTRL_CLOSE_EVENT )
  {
    // miner::hybridminer->stop();
    // miner::cleanup( miner::hybridminer );
    std::exit( EXIT_SUCCESS );
    // return FALSE;
  }
  return FALSE;
}
#else
#  include <signal.h>

void SignalHandler( int signal )
{
  // miner::cleanup( miner::hybridminer );
  std::exit( EXIT_SUCCESS );
}
#endif // _MSC_VER

HybridMiner::HybridMiner() noexcept :
m_solvers_cuda( 0u ),
m_solvers_cpu( 0u ),
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
          }() ),
m_stop( false )
{
  MinerState::initState();

  std::atexit( ExitHandler );

#ifdef _MSC_VER
  SetConsoleTitleA( ("0xBitcoin Miner v"s + std::string(MINER_VERSION)).c_str() );
  SetConsoleCtrlHandler( SignalHandler, TRUE );
#else
  struct sigaction sig_handler;

  sig_handler.sa_handler = &SignalHandler;

  sigemptyset( &sig_handler.sa_mask );

  sigaddset( &sig_handler.sa_mask, SIGINT );
  sigaddset( &sig_handler.sa_mask, SIGTERM );
  sigaddset( &sig_handler.sa_mask, SIGHUP );
  sigaddset( &sig_handler.sa_mask, SIGQUIT );

  sigaction( SIGINT,   &sig_handler, NULL );
  sigaction( SIGTERM,  &sig_handler, NULL );
  sigaction( SIGHUP,   &sig_handler, NULL );
  sigaction( SIGQUIT,  &sig_handler, NULL );
#endif // _MSC_VER
}

HybridMiner::~HybridMiner()
{
  stop();
}

auto HybridMiner::updateTarget() const -> void
{
  for( auto&& solver : m_solvers )
  {
    solver->updateTarget();
  }
}

auto HybridMiner::updateMessage() const -> void
{
  for( auto&& solver : m_solvers )
  {
    solver->updateMessage();
  }
}

// This is a the "main" thread of execution
auto HybridMiner::run() -> void
{
  startMining();

  printUiBase();

  do {
    auto timerNext = steady_clock::now() + 100ms;

    MinerState::printStatus();

    std::this_thread::sleep_until( timerNext );
  } while( !m_stop );

  std::cerr << MinerState::getPrintableTimeStamp() << "Process exiting... stopping miner\n"s;
  if( !m_old_ui )
  {
    std::cerr << "\x1b[s\x1b[?25h\x1b[r\x1b[u";
  }
}

auto HybridMiner::startMining() -> void
{
  while( !MinerState::isReady() )
  {
    std::this_thread::sleep_for( 100ms );
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
}

auto HybridMiner::stop() -> void
{
  m_stop = true;

  m_solvers.clear();

  m_solvers_cuda = m_solvers_cpu = 0u;
}

auto HybridMiner::getHashrates() const -> double const
{
  double temp{ 0 };
  for( auto&& solver : m_solvers )
  {
    temp += solver->getHashrate();
  }
  return temp;
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

  std::stringstream ss_out;
  ss_out << "Mining on "s;
  if( m_solvers_cuda > 0u )
  {
    ss_out << m_solvers_cuda
           << " GPU"s << (m_solvers_cuda > 1 ? "s"s : ""s) << " using CUDA"s;
  }
  if( m_solvers_cuda > 0u && m_solvers_cpu > 0u )
  {
    ss_out << " and "s;
  }
  if( m_solvers_cpu > 0u )
  {
    ss_out << m_solvers_cpu << " CPU core"s << (m_solvers_cpu > 1 ? "s"s : ""s);
  }

  ss_out << '.' << (m_old_ui ? '\n' : '\r');

  MinerState::pushLog( ss_out.str() );
}
