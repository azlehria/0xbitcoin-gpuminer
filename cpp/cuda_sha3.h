#ifndef _CUDASHA3_H_
#define _CUDASHA3_H_

#define TPB50 1024u
#define TPB35 384u
#define NPT 2

#include <stdio.h>
#include <stdint.h>
//#include <inttypes.h>
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

__global__
void cuda_mine( uint64_t* solution, uint64_t cnt );

#endif // !_SOLVER_H_
