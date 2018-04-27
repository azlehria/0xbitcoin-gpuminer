/*
Author: Mikers
date march 4, 2018 for 0xbitcoin dev

based off of https://github.com/Dunhili/SHA3-gpu-brute-force-cracker/blob/master/sha3.cu

 * Author: Brian Bowden
 * Date: 5/12/14
 *
 * This is the parallel version of SHA-3.
 */

#include "cuda_sha3.h"
#include "cudasolver.h"

__constant__ uint64_t d_mid[25];
__constant__ uint64_t d_target;

__device__ __constant__ const uint64_t RC[24] = {
  0x0000000000000001, 0x0000000000008082, 0x800000000000808a,
  0x8000000080008000, 0x000000000000808b, 0x0000000080000001,
  0x8000000080008081, 0x8000000000008009, 0x000000000000008a,
  0x0000000000000088, 0x0000000080008009, 0x000000008000000a,
  0x000000008000808b, 0x800000000000008b, 0x8000000000008089,
  0x8000000000008003, 0x8000000000008002, 0x8000000000000080,
  0x000000000000800a, 0x800000008000000a, 0x8000000080008081,
  0x8000000000008080, 0x0000000080000001, 0x8000000080008008
};

__device__ __forceinline__
auto bswap_64( uint64_t input ) -> uint64_t
{
  uint64_t output;
  asm( "{"
       "  prmt.b32 %0, %3, 0, 0x0123;"
       "  prmt.b32 %1, %2, 0, 0x0123;"
       "}" : "=r"(reinterpret_cast<uint2&>(output).x), "=r"(reinterpret_cast<uint2&>(output).y)
           : "r"(reinterpret_cast<uint2&>(input).x), "r"(reinterpret_cast<uint2&>(input).y) );
  return output;
}

__device__ __forceinline__
auto xor5( uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e ) -> uint64_t
{
  uint64_t output;
  asm( "{"
       "  xor.b64 %0, %1, %2;"
       "  xor.b64 %0, %0, %3;"
       "  xor.b64 %0, %0, %4;"
       "  xor.b64 %0, %0, %5;"
       "}" : "=l"(output) : "l"(a), "l"(b), "l"(c), "l"(d), "l"(e) );
  return output;
}

__device__ __forceinline__
auto xor3( uint64_t a, uint64_t b, uint64_t c ) -> uint64_t
{
  uint64_t output;
#if __CUDA_ARCH__ >= 500
  asm( "{"
       "  lop3.b32 %0, %2, %4, %6, 0x96;"
       "  lop3.b32 %1, %3, %5, %7, 0x96;"
       "}" : "=r"(reinterpret_cast<uint2&>(output).x), "=r"(reinterpret_cast<uint2&>(output).y)
           : "r"(reinterpret_cast<uint2&>(a).x), "r"(reinterpret_cast<uint2&>(a).y),
             "r"(reinterpret_cast<uint2&>(b).x), "r"(reinterpret_cast<uint2&>(b).y),
             "r"(reinterpret_cast<uint2&>(c).x), "r"(reinterpret_cast<uint2&>(c).y) );
#else
  asm( "{"
       "  xor.b64 %0, %1, %2;"
       "  xor.b64 %0, %0, %3;"
       "}" : "=l"(output) : "l"(a), "l"(b), "l"(c) );
#endif
  return output;
}

__device__ __forceinline__
auto chi( uint64_t a, uint64_t b, uint64_t c ) -> uint64_t
{
#if __CUDA_ARCH__ >= 500
  uint64_t output;
  asm( "{"
       "  lop3.b32 %0, %2, %4, %6, 0xD2;"
       "  lop3.b32 %1, %3, %5, %7, 0xD2;"
       "}" : "=r"(reinterpret_cast<uint2&>(output).x), "=r"(reinterpret_cast<uint2&>(output).y)
           : "r"(reinterpret_cast<uint2&>(a).x), "r"(reinterpret_cast<uint2&>(a).y),
             "r"(reinterpret_cast<uint2&>(b).x), "r"(reinterpret_cast<uint2&>(b).y),
             "r"(reinterpret_cast<uint2&>(c).x), "r"(reinterpret_cast<uint2&>(c).y) );
  return output;
#else
  return a ^ ((~b) & c);
#endif
}

__device__
auto keccak( uint64_t nounce ) -> bool
{
  uint64_t state[25], C[5], D[5];
  uint64_t n[11] { ROTL64(nounce,  7) };
  n[ 1] = ROTL64(n[ 0],  1);
  n[ 2] = ROTL64(n[ 1],  6);
  n[ 3] = ROTL64(n[ 2],  2);
  n[ 4] = ROTL64(n[ 3],  4);
  n[ 5] = ROTL64(n[ 4],  7);
  n[ 6] = ROTL64(n[ 5], 12);
  n[ 7] = ROTL64(n[ 6],  5);
  n[ 8] = ROTL64(n[ 7], 11);
  n[ 9] = ROTL64(n[ 8],  7);
  n[10] = ROTL64(n[ 9],  1);

  C[0] = d_mid[ 0];
  C[1] = d_mid[ 1];
  C[2] = d_mid[ 2] ^ n[ 7];
  C[3] = d_mid[ 3];
  C[4] = d_mid[ 4] ^ n[ 2];
  state[ 0] = chi( C[0], C[1], C[2] ) ^ RC[0];
  state[ 1] = chi( C[1], C[2], C[3] );
  state[ 2] = chi( C[2], C[3], C[4] );
  state[ 3] = chi( C[3], C[4], C[0] );
  state[ 4] = chi( C[4], C[0], C[1] );

  C[0] = d_mid[ 5];
  C[1] = d_mid[ 6] ^ n[ 4];
  C[2] = d_mid[ 7];
  C[3] = d_mid[ 8];
  C[4] = d_mid[ 9] ^ n[ 9];
  state[ 5] = chi( C[0], C[1], C[2] );
  state[ 6] = chi( C[1], C[2], C[3] );
  state[ 7] = chi( C[2], C[3], C[4] );
  state[ 8] = chi( C[3], C[4], C[0] );
  state[ 9] = chi( C[4], C[0], C[1] );

  C[0] = d_mid[10];
  C[1] = d_mid[11] ^ n[ 0];
  C[2] = d_mid[12];
  C[3] = d_mid[13] ^ n[ 1];
  C[4] = d_mid[14];
  state[10] = chi( C[0], C[1], C[2] );
  state[11] = chi( C[1], C[2], C[3] );
  state[12] = chi( C[2], C[3], C[4] );
  state[13] = chi( C[3], C[4], C[0] );
  state[14] = chi( C[4], C[0], C[1] );

  C[0] = d_mid[15] ^ n[ 5];
  C[1] = d_mid[16];
  C[2] = d_mid[17];
  C[3] = d_mid[18] ^ n[ 3];
  C[4] = d_mid[19];
  state[15] = chi( C[0], C[1], C[2] );
  state[16] = chi( C[1], C[2], C[3] );
  state[17] = chi( C[2], C[3], C[4] );
  state[18] = chi( C[3], C[4], C[0] );
  state[19] = chi( C[4], C[0], C[1] );

  C[0] = d_mid[20] ^ n[10];
  C[1] = d_mid[21] ^ n[ 8];
  C[2] = d_mid[22] ^ n[ 6];
  C[3] = d_mid[23];
  C[4] = d_mid[24];
  state[20] = chi( C[0], C[1], C[2] );
  state[21] = chi( C[1], C[2], C[3] );
  state[22] = chi( C[2], C[3], C[4] );
  state[23] = chi( C[3], C[4], C[0] );
  state[24] = chi( C[4], C[0], C[1] );

#if __CUDA_ARCH__ >= 350
#  pragma unroll
#endif
  for( uint_fast8_t i{ 1 }; i < 23; ++i )
  {
    // Theta
    for( uint_fast8_t x{ 0 }; x < 5; ++x )
    {
      C[(x + 6) % 5] = xor5( state[x], state[x + 5], state[x + 10], state[x + 15], state[x + 20] );
    }

#if __CUDA_ARCH__ >= 350
    for( uint_fast8_t x{ 0 }; x < 5; ++x )
    {
			D[x] = ROTL64(C[(x + 2) % 5], 1);
      state[x]      = xor3( state[x]     , D[x], C[x] );
      state[x +  5] = xor3( state[x +  5], D[x], C[x] );
      state[x + 10] = xor3( state[x + 10], D[x], C[x] );
      state[x + 15] = xor3( state[x + 15], D[x], C[x] );
      state[x + 20] = xor3( state[x + 20], D[x], C[x] );
    }
#else
    for( uint_fast8_t x{ 0 }; x < 5; ++x )
    {
      D[x] = ROTL64(C[(x + 2) % 5], 1) ^ C[x];
      state[x]      = state[x]      ^ D[x];
      state[x +  5] = state[x +  5] ^ D[x];
      state[x + 10] = state[x + 10] ^ D[x];
      state[x + 15] = state[x + 15] ^ D[x];
      state[x + 20] = state[x + 20] ^ D[x];
    }
#endif

    // Rho Pi
    C[0] = state[1];
    state[ 1] = ROTR64( state[ 6], 20 );
    state[ 6] = ROTL64( state[ 9], 20 );
    state[ 9] = ROTR64( state[22],  3 );
    state[22] = ROTR64( state[14], 25 );
    state[14] = ROTL64( state[20], 18 );
    state[20] = ROTR64( state[ 2],  2 );
    state[ 2] = ROTR64( state[12], 21 );
    state[12] = ROTL64( state[13], 25 );
    state[13] = ROTL64( state[19],  8 );
    state[19] = ROTR64( state[23],  8 );
    state[23] = ROTR64( state[15], 23 );
    state[15] = ROTL64( state[ 4], 27 );
    state[ 4] = ROTL64( state[24], 14 );
    state[24] = ROTL64( state[21],  2 );
    state[21] = ROTR64( state[ 8],  9 );
    state[ 8] = ROTR64( state[16], 19 );
    state[16] = ROTR64( state[ 5], 28 );
    state[ 5] = ROTL64( state[ 3], 28 );
    state[ 3] = ROTL64( state[18], 21 );
    state[18] = ROTL64( state[17], 15 );
    state[17] = ROTL64( state[11], 10 );
    state[11] = ROTL64( state[ 7],  6 );
    state[ 7] = ROTL64( state[10],  3 );
    state[10] = ROTL64( C[0], 1 );

    // Chi
    for( uint_fast8_t x{ 0 }; x < 25; x += 5 )
    {
      C[0] = state[x];
      C[1] = state[x + 1];
      C[2] = state[x + 2];
      C[3] = state[x + 3];
      C[4] = state[x + 4];
      state[x]     = chi( C[0], C[1], C[2] );
      state[x + 1] = chi( C[1], C[2], C[3] );
      state[x + 2] = chi( C[2], C[3], C[4] );
      state[x + 3] = chi( C[3], C[4], C[0] );
      state[x + 4] = chi( C[4], C[0], C[1] );
    }

    // Iota
    state[0] = state[0] ^ RC[i];
  }

  for( uint_fast8_t x{ 0 }; x < 5; ++x )
  {
    C[(x + 6) % 5 ] = xor5( state[x], state[x + 5], state[x + 10], state[x + 15], state[x + 20] );
  }

  D[0] = ROTL64(C[2], 1);
  D[1] = ROTL64(C[3], 1);
  D[2] = ROTL64(C[4], 1);

  state[ 0] = xor3( state[ 0], D[0], C[0] );
  state[ 6] = xor3( state[ 6], D[1], C[1] );
  state[12] = xor3( state[12], D[2], C[2] );
  state[ 6] = ROTR64(state[ 6], 20);
  state[12] = ROTR64(state[12], 21);

  state[ 0] = chi( state[ 0], state[ 6], state[12] ) ^ RC[23];

  return bswap_64( state[0] ) <= d_target;
}

KERNEL_LAUNCH_PARAMS
void cuda_mine( uint64_t* __restrict__ solution, const uint64_t cnt )
{
  uint64_t nounce{ cnt + (blockDim.x * blockIdx.x + threadIdx.x) };

  if( keccak( nounce ) )
  {
#if defined(_MSC_VER)
    atomicCAS( solution, UINT64_MAX, nounce );
#else
    atomicCAS( reinterpret_cast<unsigned long long*>(solution),
               static_cast<unsigned long long>(UINT64_MAX),
               static_cast<unsigned long long>(nounce) );
#endif
  }
}

// --------------------------------------------------------------------

auto CUDASolver::cudaInit() -> void
{
  cudaSetDevice( m_device );

  cudaDeviceProp device_prop;
  if( cudaGetDeviceProperties( &device_prop, m_device ) != cudaSuccess )
  {
    printf( "Problem getting properties for device, exiting...\n" );
    exit( EXIT_FAILURE );
  }
  int32_t compute_version = device_prop.major * 100 + device_prop.minor * 10;

  m_block.x = compute_version > 500 ? TPB50 : TPB35;
  m_grid.x = (m_threads + m_block.x - 1) / m_block.x;

  if( !m_gpu_initialized )
  {
    // CPU usage goes _insane_ without this.
    cudaDeviceReset();
    // so we don't actually _use_ L1 or local memory . . .
    cudaSetDeviceFlags( cudaDeviceScheduleBlockingSync /*| cudaDeviceLmemResizeToMax );
    cudaDeviceSetCacheConfig( cudaFuncCachePreferL1*/ );

    cudaMalloc( reinterpret_cast<void**>(&d_solution), 8 );
    cudaMallocHost( reinterpret_cast<void**>(&h_solution), 8 );

    cudaResetSolution();

    m_gpu_initialized = true;
  }
}

auto CUDASolver::cudaCleanup() -> void
{
  cudaSetDevice( m_device );

  cudaThreadSynchronize();

  cudaFree( d_solution );
  cudaFreeHost( h_solution );

  cudaDeviceReset();

  m_gpu_initialized = false;
}

auto CUDASolver::cudaResetSolution() -> void
{
  cudaSetDevice( m_device );

  *h_solution = UINT64_MAX;
  cudaMemcpy( d_solution, h_solution, 8, cudaMemcpyHostToDevice );
}

auto CUDASolver::pushTarget() -> void
{
  cudaSetDevice( m_device );

  uint64_t target{ MinerState::getTarget() };
  cudaMemcpyToSymbol( d_target, &target, 8, 0, cudaMemcpyHostToDevice);
}

auto CUDASolver::pushMessage() -> void
{
  cudaSetDevice( m_device );

  uint64_t message[25];
  MinerState::getMidstate( message, m_device );
  cudaMemcpyToSymbol( d_mid, message, 200, 0, cudaMemcpyHostToDevice);
}

auto CUDASolver::findSolution() -> void
{
  cudaInit();

  cudaSetDevice( m_device );

  do
  {
    if( m_new_target ) { pushTarget(); }
    if( m_new_message ) { pushMessage(); }

    cuda_mine <<< m_grid, m_block >>> ( d_solution, MinerState::getIncSearchSpace( m_threads ) );
    // tiny bit slower (~0.1%) and clears errors. whatever.
    cudaError_t cudaerr = cudaDeviceSynchronize();
    if( cudaerr != cudaSuccess )
    {
      printf( "kernel launch failed with error %d: \x1b[38;5;196m%s.\x1b[0m\n", cudaerr, cudaGetErrorString( cudaerr ) );
      exit( EXIT_FAILURE );
    }

    cudaMemcpy( h_solution, d_solution, 8, cudaMemcpyDeviceToHost );

    if( *h_solution != UINT64_MAX )
    {
      MinerState::pushSolution( *h_solution );
      cudaResetSolution();
    }
  } while( !m_stop );
}
