// magic numbers we need to tune
#define INTENSITY 26
// magic numbers we need to tune

#include <time.h>
#include <curand.h>
#include <assert.h>
#include <curand_kernel.h>

/*
Author: Mikers
date march 4, 2018 for 0xbitcoin dev

based off of https://github.com/Dunhili/SHA3-gpu-brute-force-cracker/blob/master/sha3.cu

 * Author: Brian Bowden
 * Date: 5/12/14
 *
 * This is the parallel version of SHA-3.
 */

#include "cudasolver.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#ifdef __INTELLISENSE__
 /* reduce vstudio warnings (__byteperm, blockIdx...) */
#include <device_functions.h>
#include <device_launch_parameters.h>
#define __launch_bounds__(max_tpb, min_blocks)
#endif

#define TPB52 1024
#define TPB50 384
#define NPT 2
#define NBN 2

void gpu_init();
//void runBenchmarks();
//char *read_in_messages();
int gcd( int a, int b );

// updated message the gpu_init() function
int clock_speed;
int compute_version;
int h_done[1] = { 0 };
clock_t start;

unsigned long long cnt;
unsigned long long printable_hashrate_cnt;

int num_messages;
const int digest_size = 256;
const int digest_size_bytes = digest_size / 8;
const size_t str_length = 7;	//change for different sizes

unsigned char * h_message;

//cudaEvent_t start, stop;

int* d_done;
char* d_solution;

unsigned char* d_challenge_hash;
unsigned char* d_hash_prefix;

#define ROTL64(x, y) (((x) << (y)) | ((x) >> (64 - (y))))

__device__ const uint64_t RC[24] = {
    0x0000000000000001, 0x0000000000008082, 0x800000000000808a,
    0x8000000080008000, 0x000000000000808b, 0x0000000080000001,
    0x8000000080008081, 0x8000000000008009, 0x000000000000008a,
    0x0000000000000088, 0x0000000080008009, 0x000000008000000a,
    0x000000008000808b, 0x800000000000008b, 0x8000000000008089,
    0x8000000000008003, 0x8000000000008002, 0x8000000000000080,
    0x000000000000800a, 0x800000008000000a, 0x8000000080008081,
    0x8000000000008080, 0x0000000080000001, 0x8000000080008008
};

__device__ const int r[24] = {
    1,  3,  6,  10, 15, 21, 28, 36, 45, 55, 2,  14,
    27, 41, 56, 8,  25, 43, 62, 18, 39, 61, 20, 44
};

__device__ const int piln[24] = {
    10, 7,  11, 17, 18, 3, 5,  16, 8,  21, 24, 4,
    15, 23, 19, 13, 12, 2, 20, 14, 22, 9,  6,  1
};

__device__ int compare_hash( unsigned char *target, unsigned char *hash, int length )
{
  int i = 0;
  for( i = 0; i < length; i++ )
  {
    if( hash[i] != target[i] )break;
  }
  return (unsigned char)( hash[i] ) < (unsigned char)( target[i] );
}

__device__ void keccak( const unsigned char *message, int message_len, unsigned char *output, int output_len )
{
  uint64_t state[25];
  uint8_t temp_message[144];
  const int rsize = 136;
  const int rsize_byte = 17;

  memset( state, 0, sizeof( state ) );

  // last block and padding
  memcpy( temp_message, message, message_len );
  temp_message[message_len++] = 1;
  memset( temp_message + message_len, 0, rsize - message_len );
  temp_message[rsize - 1] |= 0x80;

  for( int i = 0; i < rsize_byte; i++ )
  {
    state[i] ^= ( (uint64_t *)temp_message )[i];
  }

  uint64_t temp, C[5];
  int j;

  for( int i = 0; i < 24; i++ )
  {
    // Theta
    // for i = 0 to 5
    //    C[i] = state[i] ^ state[i + 5] ^ state[i + 10] ^ state[i + 15] ^ state[i + 20];
    C[0] = state[0] ^ state[5] ^ state[10] ^ state[15] ^ state[20];
    C[1] = state[1] ^ state[6] ^ state[11] ^ state[16] ^ state[21];
    C[2] = state[2] ^ state[7] ^ state[12] ^ state[17] ^ state[22];
    C[3] = state[3] ^ state[8] ^ state[13] ^ state[18] ^ state[23];
    C[4] = state[4] ^ state[9] ^ state[14] ^ state[19] ^ state[24];

    // for i = 0 to 5
    //     temp = C[(i + 4) % 5] ^ ROTL64(C[(i + 1) % 5], 1);
    //     for j = 0 to 25, j += 5
    //          state[j + i] ^= temp;
    temp = C[4] ^ ROTL64( C[1], 1 );
    state[0] ^= temp;
    state[5] ^= temp;
    state[10] ^= temp;
    state[15] ^= temp;
    state[20] ^= temp;

    temp = C[0] ^ ROTL64( C[2], 1 );
    state[1] ^= temp;
    state[6] ^= temp;
    state[11] ^= temp;
    state[16] ^= temp;
    state[21] ^= temp;

    temp = C[1] ^ ROTL64( C[3], 1 );
    state[2] ^= temp;
    state[7] ^= temp;
    state[12] ^= temp;
    state[17] ^= temp;
    state[22] ^= temp;

    temp = C[2] ^ ROTL64( C[4], 1 );
    state[3] ^= temp;
    state[8] ^= temp;
    state[13] ^= temp;
    state[18] ^= temp;
    state[23] ^= temp;

    temp = C[3] ^ ROTL64( C[0], 1 );
    state[4] ^= temp;
    state[9] ^= temp;
    state[14] ^= temp;
    state[19] ^= temp;
    state[24] ^= temp;

    // Rho Pi
    // for i = 0 to 24
    //     j = piln[i];
    //     C[0] = state[j];
    //     state[j] = ROTL64(temp, r[i]);
    //     temp = C[0];
    temp = state[1];
    j = piln[0];
    C[0] = state[j];
    state[j] = ROTL64( temp, r[0] );
    temp = C[0];

    j = piln[1];
    C[0] = state[j];
    state[j] = ROTL64( temp, r[1] );
    temp = C[0];

    j = piln[2];
    C[0] = state[j];
    state[j] = ROTL64( temp, r[2] );
    temp = C[0];

    j = piln[3];
    C[0] = state[j];
    state[j] = ROTL64( temp, r[3] );
    temp = C[0];

    j = piln[4];
    C[0] = state[j];
    state[j] = ROTL64( temp, r[4] );
    temp = C[0];

    j = piln[5];
    C[0] = state[j];
    state[j] = ROTL64( temp, r[5] );
    temp = C[0];

    j = piln[6];
    C[0] = state[j];
    state[j] = ROTL64( temp, r[6] );
    temp = C[0];

    j = piln[7];
    C[0] = state[j];
    state[j] = ROTL64( temp, r[7] );
    temp = C[0];

    j = piln[8];
    C[0] = state[j];
    state[j] = ROTL64( temp, r[8] );
    temp = C[0];

    j = piln[9];
    C[0] = state[j];
    state[j] = ROTL64( temp, r[9] );
    temp = C[0];

    j = piln[10];
    C[0] = state[j];
    state[j] = ROTL64( temp, r[10] );
    temp = C[0];

    j = piln[11];
    C[0] = state[j];
    state[j] = ROTL64( temp, r[11] );
    temp = C[0];

    j = piln[12];
    C[0] = state[j];
    state[j] = ROTL64( temp, r[12] );
    temp = C[0];

    j = piln[13];
    C[0] = state[j];
    state[j] = ROTL64( temp, r[13] );
    temp = C[0];

    j = piln[14];
    C[0] = state[j];
    state[j] = ROTL64( temp, r[14] );
    temp = C[0];

    j = piln[15];
    C[0] = state[j];
    state[j] = ROTL64( temp, r[15] );
    temp = C[0];

    j = piln[16];
    C[0] = state[j];
    state[j] = ROTL64( temp, r[16] );
    temp = C[0];

    j = piln[17];
    C[0] = state[j];
    state[j] = ROTL64( temp, r[17] );
    temp = C[0];

    j = piln[18];
    C[0] = state[j];
    state[j] = ROTL64( temp, r[18] );
    temp = C[0];

    j = piln[19];
    C[0] = state[j];
    state[j] = ROTL64( temp, r[19] );
    temp = C[0];

    j = piln[20];
    C[0] = state[j];
    state[j] = ROTL64( temp, r[20] );
    temp = C[0];

    j = piln[21];
    C[0] = state[j];
    state[j] = ROTL64( temp, r[21] );
    temp = C[0];

    j = piln[22];
    C[0] = state[j];
    state[j] = ROTL64( temp, r[22] );
    temp = C[0];

    j = piln[23];
    C[0] = state[j];
    state[j] = ROTL64( temp, r[23] );
    temp = C[0];

    //  Chi
    // for j = 0 to 25, j += 5
    //     for i = 0 to 5
    //         C[i] = state[j + i];
    //     for i = 0 to 5
    //         state[j + 1] ^= (~C[(i + 1) % 5]) & C[(i + 2) % 5];
    C[0] = state[0];
    C[1] = state[1];
    C[2] = state[2];
    C[3] = state[3];
    C[4] = state[4];

    state[0] ^= ( ~C[1] ) & C[2];
    state[1] ^= ( ~C[2] ) & C[3];
    state[2] ^= ( ~C[3] ) & C[4];
    state[3] ^= ( ~C[4] ) & C[0];
    state[4] ^= ( ~C[0] ) & C[1];

    C[0] = state[5];
    C[1] = state[6];
    C[2] = state[7];
    C[3] = state[8];
    C[4] = state[9];

    state[5] ^= ( ~C[1] ) & C[2];
    state[6] ^= ( ~C[2] ) & C[3];
    state[7] ^= ( ~C[3] ) & C[4];
    state[8] ^= ( ~C[4] ) & C[0];
    state[9] ^= ( ~C[0] ) & C[1];

    C[0] = state[10];
    C[1] = state[11];
    C[2] = state[12];
    C[3] = state[13];
    C[4] = state[14];

    state[10] ^= ( ~C[1] ) & C[2];
    state[11] ^= ( ~C[2] ) & C[3];
    state[12] ^= ( ~C[3] ) & C[4];
    state[13] ^= ( ~C[4] ) & C[0];
    state[14] ^= ( ~C[0] ) & C[1];

    C[0] = state[15];
    C[1] = state[16];
    C[2] = state[17];
    C[3] = state[18];
    C[4] = state[19];

    state[15] ^= ( ~C[1] ) & C[2];
    state[16] ^= ( ~C[2] ) & C[3];
    state[17] ^= ( ~C[3] ) & C[4];
    state[18] ^= ( ~C[4] ) & C[0];
    state[19] ^= ( ~C[0] ) & C[1];

    C[0] = state[20];
    C[1] = state[21];
    C[2] = state[22];
    C[3] = state[23];
    C[4] = state[24];

    state[20] ^= ( ~C[1] ) & C[2];
    state[21] ^= ( ~C[2] ) & C[3];
    state[22] ^= ( ~C[3] ) & C[4];
    state[23] ^= ( ~C[4] ) & C[0];
    state[24] ^= ( ~C[0] ) & C[1];

    //  Iota
    state[0] ^= RC[i];
  }
  memcpy( output, state, output_len );
}

// hash length is 256 bits
#if __CUDA_ARCH__ > 500
__global__ __launch_bounds__( TPB52, 1 )
#else
__global__ __launch_bounds__( TPB50, 2 )
#endif
void gpu_mine( unsigned char *challenge_hash, char * device_solution, int *d_done, const unsigned char * hash_prefix, int now, unsigned long long cnt, unsigned int threads )
{
  uint32_t thread = blockDim.x * blockIdx.x + threadIdx.x;
  unsigned char message[84];
  //char * hash = &working_memory_hash[32 * ( tid )];

  memset( message, 0xf, 84 );

  int str_len = 84;

  int len = 0;
  for( len = 0; len < 52; len++ )
  {
    message[len] = hash_prefix[len];
  }

  //uint2 s[25], t[5], v, w, u[5];
#if __CUDA_ARCH__ > 500
  uint64_t step = gridDim.x * blockDim.x;
  uint64_t maxNonce = cnt + threads;
  for( uint64_t nounce = cnt + thread; nounce < maxNonce; nounce += step )
  {
#else
  uint32_t nounce = cnt + thread;
  if( thread < threads )
  {
#endif
    (uint64_t&)message[52] = nounce;

    const int output_len = 32;
    unsigned char output[output_len];
    keccak( message, str_len, output, output_len );

    if( compare_hash( &challenge_hash[0], &output[0], output_len ) )
    {
      if( d_done[0] != 1 )
      {
        d_done[0] = 1;
        memcpy( device_solution, message, str_len );
      }
      return;
    }
  }
}

__host__ void stop_solving()
{
  h_done[0] = 1;
}

/**
 * Initializes the global variables by calling the cudaGetDeviceProperties().
 */
__host__ void gpu_init()
{
  cudaDeviceProp device_prop;
  int device_count;
  start = clock();

  cudaGetDeviceCount( &device_count );

  if( cudaGetDeviceProperties( &device_prop, 0 ) != cudaSuccess )
  {
    printf( "Problem getting properties for device, exiting...\n" );
    exit( EXIT_FAILURE );
  }

  compute_version = device_prop.major * 100 + device_prop.minor * 10;

  clock_speed = (int)( device_prop.memoryClockRate * 1000 * 1000 );    // convert from GHz to hertz

  h_message = (unsigned char*)malloc( 84 );

  cudaMalloc( (void**)&d_done, sizeof( int ) );
  cudaMalloc( (void**)&d_solution, 84 ); // solution
  cudaMalloc( (void**)&d_challenge_hash, 32 );
  cudaMalloc( (void**)&d_hash_prefix, 52 );

  //cnt = 0;
  printable_hashrate_cnt = 0;
}

__host__ int gcd( int a, int b )
{
  return ( a == 0 ) ? b : gcd( b % a, a );
}

__host__ unsigned long long getHashCount()
{
  return cnt;
}
__host__ void resetHashCount()
{
  cnt = 0;
  printable_hashrate_cnt = 0;
}

__host__ void update_mining_inputs( const char * challenge_target, const char * hash_prefix ) // can accept challenge
{
  cudaMalloc( (void**)&d_done, sizeof( int ) );
  cudaMalloc( (void**)&d_solution, 84 ); // solution
  cudaMalloc( (void**)&d_challenge_hash, 32 );

  cudaMalloc( (void**)&d_hash_prefix, 52 );

  cudaMemcpy( d_done, h_done, sizeof( int ), cudaMemcpyHostToDevice );
  cudaMemset( d_solution, 0xff, 84 );
  cudaMemcpy( d_challenge_hash, challenge_target, 32, cudaMemcpyHostToDevice );
  cudaMemcpy( d_hash_prefix, hash_prefix, 52, cudaMemcpyHostToDevice );
}

__host__ bool find_message( const char * challenge_target, const char * hash_prefix ) // can accept challenge
{
  h_done[0] = 0;

  cudaMemcpy( d_done, h_done, sizeof( int ), cudaMemcpyHostToDevice );
  cudaMemset( d_solution, 0xff, 84 );
  cudaMemcpy( d_challenge_hash, challenge_target, 32, cudaMemcpyHostToDevice );
  cudaMemcpy( d_hash_prefix, hash_prefix, 52, cudaMemcpyHostToDevice );

  //cudaThreadSetLimit( cudaLimitMallocHeapSize, 2 * ( 84 * number_blocks*number_threads + 32 * number_blocks*number_threads ) );

  int now = (int)time( 0 );
  uint32_t threads = 1UL << INTENSITY;

  uint32_t tpb;
  dim3 grid;
  if( compute_version > 500 )
  {
    tpb = TPB52;
    grid.x = ( threads + ( NPT*tpb ) - 1 ) / ( NPT*tpb );
  }
  else
  {
    tpb = TPB50;
    grid.x = ( threads + tpb - 1 ) / tpb;
  }
  const dim3 block( tpb );

  gpu_mine <<< grid, block >>> ( d_challenge_hash, d_solution, d_done, d_hash_prefix, now, cnt, threads );
  cudaError_t cudaerr = cudaDeviceSynchronize();
  if( cudaerr != cudaSuccess )
  {
    h_done[0] = 1;

    printf( "kernel launch failed with error %d: %s.\n", cudaerr, cudaGetErrorString( cudaerr ) );
    exit( EXIT_FAILURE );
  }
  cnt += threads;
  printable_hashrate_cnt += threads;

  cudaMemcpy( h_done, d_done, sizeof( int ), cudaMemcpyDeviceToHost );
  cudaMemcpy( h_message, d_solution, 84, cudaMemcpyDeviceToHost );

  clock_t t = clock() - start;

  fprintf( stderr, "Hash Rate: %i MH/Second\t", int(float(printable_hashrate_cnt)/(((float)t)/CLOCKS_PER_SEC )/1000000));
  fprintf( stderr, "Total hashes: %llu\n", printable_hashrate_cnt );
  return ( h_done[0] == 1 );
}

__host__ void gpu_cleanup()
{
  cudaThreadSynchronize();

  cudaFree( d_done );
  cudaFree( d_solution );
  cudaFree( d_challenge_hash );
  cudaFree( d_hash_prefix );
  free( h_message );
}
