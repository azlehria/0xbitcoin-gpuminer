#ifndef _CUDASHA3_H_
#define _CUDASHA3_H_

// default magic numbers
#define INTENSITY 23
#define CUDA_DEVICE 0
// default magic numbers

#define TPB50 1024u
#define TPB35 384u
#define NPT 2

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>
#include <sys/timeb.h>
#include <cuda_runtime.h>

#if defined(_MSC_VER)
#  include <process.h>
#else
#  include <sys/types.h>
#  include <unistd.h>
#endif

#ifdef __INTELLISENSE__
 /* reduce vstudio warnings (__byteperm, blockIdx...) */
#  include <device_functions.h>
#  include <device_launch_parameters.h>
#  define __launch_bounds__(max_tpb, min_blocks)
#endif //__INTELLISENSE__

#if __CUDA_ARCH__ > 500
#  define KERNEL_LAUNCH_PARAMS __global__ __launch_bounds__( TPB50, 1 )
#else
#  define KERNEL_LAUNCH_PARAMS __global__ __launch_bounds__( TPB35, 2 )
#endif

#define ROTL64(x, y) (((x) << (y)) ^ ((x) >> (64 - (y))))
#define ROTR64(x, y) (((x) >> (y)) ^ ((x) << (64 - (y))))

extern uint8_t solution[32];
extern uint64_t* h_message;

__global__
void gpu_mine( uint64_t* solution, /*int32_t* done,*/ uint64_t cnt );

__host__
void stop_solving();

__host__
uint64_t getHashCount();

__host__
void resetHashCount();

__host__
void send_to_device( uint64_t target, uint64_t* message );

__host__
void gpu_init();

__host__
void update_mining_inputs( uint64_t target, uint8_t* hash_prefix );

__host__
void gpu_cleanup();

__host__
bool find_message();

// #include <atomic>
// #include <mutex>
// #include <string>
// #include <vector>


// class CUDASha3
// {
// public:
//   CUDASha3() noexcept;
//   void gpu_init();
//   void runBenchmarks();
//   char *read_in_messages();
//   int32_t gcd( int32_t a, int32_t b );

// private:
//   // updated message the gpu_init() function
//   int32_t clock_speed;
//   int32_t number_multi_processors;
//   int32_t number_blocks;
//   int32_t number_threads;
//   int32_t max_threads_per_mp;

//   int32_t num_messages;
//   const int32_t digest_size = 256;
//   const int32_t digest_size_bytes = digest_size / 8;
//   const size_t str_length = 7;	//change for different sizes

//   cudaEvent_t start, stop;
// };

#endif // !_SOLVER_H_
