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

int32_t intensity;
int32_t cuda_device;
int32_t clock_speed;
int32_t compute_version;
struct timeb start, end;

uint64_t cnt;
uint64_t printable_hashrate_cnt;
uint64_t print_counter;

bool gpu_initialized;
bool new_input;

uint8_t solution[32] = { 0 };

uint64_t* h_message;
uint8_t init_message[84];

uint64_t* d_solution;

uint8_t* d_challenge;
uint8_t* d_hash_prefix;
__constant__ uint64_t d_mid[25];
__constant__ uint64_t d_target;
__constant__ uint32_t threads;

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
uint64_t bswap_64( uint64_t input )
{
  uint64_t output{ 0 };
  asm( "{"
       "  prmt.b32 %0, %3, 0, 0x0123;"
       "  prmt.b32 %1, %2, 0, 0x0123;"
       "}" : "=r"(((uint2&)output).x), "=r"(((uint2&)output).y) : "r"(((uint2&)input).x), "r"(((uint2&)input).y) );
  return output;
}

__device__ __forceinline__
uint64_t xor5( uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e )
{
  uint64_t output{ 0 };
  asm( "{"
       "  xor.b64 %0, %1, %2;"
       "  xor.b64 %0, %0, %3;"
       "  xor.b64 %0, %0, %4;"
       "  xor.b64 %0, %0, %5;"
       "}" : "+l"(output) : "l"(a), "l"(b), "l"(c), "l"(d), "l"(e) );
  return output;
}

__device__ __forceinline__
uint64_t xor3( uint64_t a, uint64_t b, uint64_t c )
{
  uint64_t output{ 0 };
  asm( "{"
       "  xor.b64 %0, %1, %2;"
       "  xor.b64 %0, %0, %3;"
       "}" : "+l"(output) : "l"(a), "l"(b), "l"(c) );
  return output;
}

__device__ __forceinline__
uint64_t chi( uint64_t a, uint64_t b, uint64_t c )
{
#if __CUDA_ARCH__ >= 500
  uint64_t output{ 0 };
  asm( "{"
       "  lop3.b32 %0, %2, %3, %4, 0xD2;"
       "  lop3.b32 %1, %5, %6, %7, 0xD2;"
       "}" : "=r"(((uint2&)output).x), "=r"(((uint2&)output).y)
           : "r"(((uint2&)a).x), "r"(((uint2&)b).x), "r"(((uint2&)c).x),
             "r"(((uint2&)a).y), "r"(((uint2&)b).y), "r"(((uint2&)c).y) );
  return output;
#else
  return a ^ ((~b) & c);
#endif
}

__device__
bool keccak( uint64_t nounce )
{
  uint64_t state[25], C[5], D[5];

  C[0] = d_mid[ 2] ^ ROTR64(nounce, 20);
  C[1] = d_mid[ 4] ^ ROTL64(nounce, 14);
  state[ 0] = chi( d_mid[ 0], d_mid[ 1], C[ 0] ) ^ RC[0];
  state[ 1] = chi( d_mid[ 1], C[ 0], d_mid[ 3] );
  state[ 2] = chi( C[ 0], d_mid[ 3], C[ 1] );
  state[ 3] = chi( d_mid[ 3], C[ 1], d_mid[ 0] );
  state[ 4] = chi( C[ 1], d_mid[ 0], d_mid[ 1] );

  C[0] = d_mid[ 6] ^ ROTL64(nounce, 20);
  C[1] = d_mid[ 9] ^ ROTR64(nounce,  2);
  state[ 5] = chi( d_mid[ 5], C[ 0], d_mid[7] );
  state[ 6] = chi( C[0], d_mid[ 7], d_mid[8] );
  state[ 7] = chi( d_mid[ 7], d_mid[ 8], C[1] );
  state[ 8] = chi( d_mid[ 8], C[1], d_mid[5] );
  state[ 9] = chi( C[1], d_mid[ 5], C[0] );

  C[0] = d_mid[11] ^ ROTL64(nounce, 7);
  C[1] = d_mid[13] ^ ROTL64(nounce, 8);
  state[10] = chi( d_mid[10], C[0], d_mid[12] );
  state[11] = chi( C[0], d_mid[12], C[1] );
  state[12] = chi( d_mid[12], C[1], d_mid[14] );
  state[13] = chi( C[1], d_mid[14], d_mid[10] );
  state[14] = chi( d_mid[14], d_mid[10], C[0] );

  C[0] = d_mid[15] ^ ROTL64(nounce, 27);
  C[1] = d_mid[18] ^ ROTL64(nounce, 16);
  state[15] = chi( C[0], d_mid[16], d_mid[17] );
  state[16] = chi( d_mid[16], d_mid[17], C[1] );
  state[17] = chi( d_mid[17], C[1], d_mid[19] );
  state[18] = chi( C[1], d_mid[19], C[0] );
  state[19] = chi( d_mid[19], C[0], d_mid[16] );

  C[0] = d_mid[20] ^ ROTR64(nounce,  1);
  C[1] = d_mid[21] ^ ROTR64(nounce,  9);
  C[2] = d_mid[22] ^ ROTR64(nounce, 25);
  state[20] = chi( C[0], C[1], C[2] );
  state[21] = chi( C[1], C[2], d_mid[23] );
  state[22] = chi( C[2], d_mid[23], d_mid[24] );
  state[23] = chi( d_mid[23], d_mid[24], C[0] );
  state[24] = chi( d_mid[24], C[0], C[1] );

#if __CUDA_ARCH__ >= 350
#  pragma unroll
#endif
  for( int32_t i{ 1 }; i < 23; ++i )
  {
    // Theta
    for( uint32_t x{ 0 }; x < 5; ++x )
    {
      C[(x + 6) % 5] = xor5( state[x], state[x + 5], state[x + 10], state[x + 15], state[x + 20] );
    }

#if __CUDA_ARCH__ >= 350
    for( uint32_t x{ 0 }; x < 5; ++x )
    {
			D[x] = ROTL64(C[(x + 2) % 5], 1);
      state[x]      = xor3( state[x]     , D[x], C[x] );
      state[x +  5] = xor3( state[x +  5], D[x], C[x] );
      state[x + 10] = xor3( state[x + 10], D[x], C[x] );
      state[x + 15] = xor3( state[x + 15], D[x], C[x] );
      state[x + 20] = xor3( state[x + 20], D[x], C[x] );
    }
#else
    for( uint32_t x{ 0 }; x < 5; ++x )
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
    for( uint32_t x{ 0 }; x < 25; x += 5 )
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

  for( uint32_t x{ 0 }; x < 5; ++x )
  {
    C[(x + 6) % 5 ] = xor5( state[x], state[x + 5], state[x + 10], state[x + 15], state[x + 20] );
  }

  D[0] = ROTL64(C[2], 1);
  D[1] = ROTL64(C[3], 1);
  D[2] = ROTL64(C[4], 1);

  state[ 0] = xor3( state[ 0], D[0], C[0] );
  state[ 6] = ROTR64(xor3( state[ 6], D[1], C[1] ), 20);
  state[12] = ROTR64(xor3( state[12], D[2], C[2] ), 21);

  state[ 0] = chi( state[ 0], state[ 6], state[12] ) ^ RC[23];

  return bswap_64( state[0] ) <= d_target;
}

KERNEL_LAUNCH_PARAMS
void gpu_mine( uint64_t* solution, uint64_t cnt )
{
  uint64_t nounce{ cnt + (blockDim.x * blockIdx.x + threadIdx.x) };

  if( keccak( nounce ) )
  {
    *solution = nounce;
    return;
  }
}

__host__
void stop_solving()
{
  // h_done[0] = -2;
}

__host__
uint64_t getHashCount()
{
  return cnt;
}

__host__
void resetHashCount()
{
  cudaSetDevice( cuda_device );

  *h_message = UINT64_MAX;
  cudaMemcpy( d_solution, h_message, sizeof( uint64_t ), cudaMemcpyHostToDevice );

  printable_hashrate_cnt = 0;
  print_counter = 0;

  ftime( &start );
}

__host__
void send_to_device( uint64_t target, uint64_t* message )
{
  cudaSetDevice( cuda_device );

  uint64_t C[4], D[5], mid[25];
  C[0] = message[0] ^ message[5] ^ message[10] ^ 0x100000000ull;
  C[1] = message[1] ^ message[6] ^ 0x8000000000000000ull;
  C[2] = message[2] ^ message[7];
  C[3] = message[4] ^ message[9];

  D[0] = ROTL64(C[1], 1) ^ C[3];
  D[1] = ROTL64(C[2], 1) ^ C[0];
  D[2] = ROTL64(message[3], 1) ^ C[1];
  D[3] = ROTL64(C[3], 1) ^ C[2];
  D[4] = ROTL64(C[0], 1) ^ message[3];

  mid[ 0] = message[ 0] ^ D[0];
  mid[ 1] = ROTL64( message[6] ^ D[1], 44 );
  mid[ 2] = ROTL64(D[2], 43);
  mid[ 3] = ROTL64(D[3], 21);
  mid[ 4] = ROTL64(D[4], 14);
  mid[ 5] = ROTL64( message[3] ^ D[3], 28 );
  mid[ 6] = ROTL64( message[9] ^ D[4], 20 );
  mid[ 7] = ROTL64( message[10] ^ D[0] ^ 0x100000000ull, 3 );
  mid[ 8] = ROTL64( 0x8000000000000000ull ^ D[1], 45 );
  mid[ 9] = ROTL64(D[2], 61);
  mid[10] = ROTL64( message[1] ^ D[1],  1 );
  mid[11] = ROTL64( message[7] ^ D[2],  6 );
  mid[12] = ROTL64(D[3], 25);
  mid[13] = ROTL64(D[4],  8);
  mid[14] = ROTL64(D[0], 18);
  mid[15] = ROTL64( message[4] ^ D[4], 27 );
  mid[16] = ROTL64( message[5] ^ D[0], 36 );
  mid[17] = ROTL64(D[1], 10);
  mid[18] = ROTL64(D[2], 15);
  mid[19] = ROTL64(D[3], 56);
  mid[20] = ROTL64( message[2] ^ D[2], 62 );
  mid[21] = ROTL64(D[3], 55);
  mid[22] = ROTL64(D[4], 39);
  mid[23] = ROTL64(D[0], 41);
  mid[24] = ROTL64(D[1],  2);

  cudaMemcpyToSymbol( d_mid, mid, sizeof( mid ), 0, cudaMemcpyHostToDevice);

  cudaMemcpyToSymbol( d_target, &target, sizeof( target ), 0, cudaMemcpyHostToDevice);
}

/**
 * Initializes the global variables by calling the cudaGetDeviceProperties().
 */
__host__
void gpu_init()
{
  cudaDeviceProp device_prop;
  int32_t device_count;

  char config[10];
  FILE * inf;
  inf = fopen( "0xbtc.conf", "r" );
  if( inf )
  {
    fgets( config, 10, inf );
    fclose( inf );
    intensity = atol( strtok( config, " " ) );
    cuda_device = atol( strtok( NULL, " " ) );
  }
  else
  {
    intensity = INTENSITY;
    cuda_device = CUDA_DEVICE;
  }

  cudaGetDeviceCount( &device_count );

  if( cudaGetDeviceProperties( &device_prop, cuda_device ) != cudaSuccess )
  {
    printf( "Problem getting properties for device, exiting...\n" );
    exit( EXIT_FAILURE );
  }

  cudaSetDevice( cuda_device );

  if( !gpu_initialized )
  {
    // CPU usage goes _insane_ without this.
    cudaDeviceReset();
    cudaSetDeviceFlags( cudaDeviceScheduleBlockingSync | cudaDeviceLmemResizeToMax );
    cudaDeviceSetCacheConfig( cudaFuncCachePreferL1 );

    cudaMalloc( (void**)&d_solution, sizeof( uint64_t ) ); // solution
    cudaMallocHost( (void**)&h_message, sizeof( uint64_t ) );

    (uint32_t&)(init_message[52]) = 014533075101u;
    (uint32_t&)(init_message[56]) = 014132271150u;

    srand((time(NULL) & 0xFFFF) | (getpid() << 16));
    for(int8_t i_rand{ 60 }; i_rand < 84; ++i_rand){
      init_message[i_rand] = (uint8_t)rand() % 256;
    }
    memcpy( solution, &init_message[52], 32 );

    uint32_t h_threads{ 1u << intensity };
    cudaMemcpyToSymbol( threads, &h_threads, sizeof( h_threads ), 0, cudaMemcpyHostToDevice );

    gpu_initialized = true;
  }

  compute_version = device_prop.major * 100 + device_prop.minor * 10;

  // convert from GHz to hertz
  clock_speed = (int32_t)( device_prop.memoryClockRate * 1000 * 1000 );

  //cnt = 0;
  printable_hashrate_cnt = 0;
  print_counter = 0;

  ftime( &start );
  if( new_input ) new_input = false;
}

__host__
void update_mining_inputs( uint64_t target, uint8_t* hash_prefix )
{
  memcpy( init_message, hash_prefix, 52 );
  send_to_device( target, (uint64_t*)init_message );
}

__host__
void gpu_cleanup()
{
  cudaSetDevice( cuda_device );

  cudaThreadSynchronize();

  cudaFree( d_solution );
  cudaFreeHost( h_message );

  cudaDeviceReset();
}

__host__
bool find_message()
{
  cudaSetDevice( cuda_device );

  uint32_t threads{ 1u << intensity };

  uint32_t tpb{ compute_version > 500 ? TPB50 : TPB35 };
  dim3 grid{ (threads + tpb - 1) / tpb };
  dim3 block{ tpb };

  gpu_mine <<< grid, block >>> ( d_solution, cnt );
  // cudaError_t cudaerr = cudaDeviceSynchronize();
  // if( cudaerr != cudaSuccess )
  // {
  //  printf( "kernel launch failed with error %d: \x1b[38;5;196m%s.\x1b[0m\n", cudaerr, cudaGetErrorString( cudaerr ) );
  //  exit( EXIT_FAILURE );
  // }

  cnt += threads;
  printable_hashrate_cnt += threads;

  cudaMemcpy( h_message, d_solution, sizeof( uint64_t ), cudaMemcpyDeviceToHost );
  if( *h_message != UINT64_MAX )
    memcpy( &solution[12], h_message, sizeof( uint64_t ) );

  ftime( &end );
  double t{ (double)((end.time * 1000 + end.millitm) - (start.time * 1000 + start.millitm)) / 1000 };

  if( t*10 > print_counter )
  {
    ++print_counter;

    // maybe breaking the control codes into macros is a good idea . . .
    printf( "\x1b[s\x1b[?25l\x1b[2;22f\x1b[38;5;221m%*.2f\x1b[0m\x1b[u\x1b[?25h"
            "\x1b[s\x1b[?25l\x1b[3;36f\x1b[38;5;208m%*" PRIu64 "\x1b[0m\x1b[u\x1b[?25h"
            "\x1b[s\x1b[?25l\x1b[2;75f\x1b[38;5;33m%02u:%02u\x1b[0m\x1b[u\x1b[?25h",
            8, ( (double)printable_hashrate_cnt / t / 1000000 ),
            25, printable_hashrate_cnt,
            ((uint32_t)t/60), ((uint32_t)t%60) );
  }

  return ( *h_message != UINT64_MAX );
  // return ( h_done[0] >= 0 );
}
