/* Nick - taken from https://stackoverflow.com/questions/51818655/clflush-to-invalidate-cache-line-via-c-function */



/* compile with gcc at optimization level -O3 */
/* set the minimum and maximum CPU frequency for all cores using cpupower to get meaningful results */ 
/* run using "sudo nice -n -20 ./a.out" to minimize possible context switches, or at least use "taskset -c 0 ./a.out" */
/* you can optionally use a p-state scaling driver other than intel_pstate to get more reproducable results */
/* This code still needs improvement to obtain more accurate measurements,
   and a lot of effort is required to do that—argh! */
/* Specifically, there is no single constant latency for the L1 because of
   the way it's designed, and more so for main memory. */
/* Things such as virtual addresses, physical addresses, TLB contents,
   code addresses, and interrupts may have an impact that needs to be
   investigated */
/* The instructions that GCC puts unnecessarily in the timed section are annoying AF */
/* This code is written to run on Intel processors! */

#include <stdint.h>
#include <x86intrin.h>
#include <stdio.h>
int main()
{
  int array[ 100 ];

  /* this is optional */
  /* will bring array in the cache */
  for ( int i = 0; i < 100; i++ )
    array[ i ] = i;

  printf( "address = %p \n", &array[ 0 ] ); /* guaranteed to be aligned within a single cache line */

  _mm_mfence();                      /* prevent clflush from being reordered by the CPU or the compiler in this direction */

  /* flush the line containing the element */
  _mm_clflush( &array[ 0 ] );

  //unsigned int aux;
  uint64_t time1, time2, msl, hsl, osl; /* initial values don't matter */

  /* You can generally use rdtsc or rdtscp.
     See: https://stackoverflow.com/questions/59759596/is-there-any-difference-in-between-rdtsc-lfence-rdtsc-and-rdtsc-rdtscp
     I AM NOT SURE THOUGH THAT THE SERIALIZATION PROERTIES OF
     RDTSCP ARE APPLICABLE AT THE COMPILER LEVEL WHEN USING THE
     __RDTSCP INTRINSIC. THIS IS TRUE FOR PURE FENCES SUCH AS LFENCE. */

  _mm_mfence();                      /* this properly orders both clflush and rdtsc*/
  _mm_lfence();                      /* mfence and lfence must be in this order + compiler barrier for rdtsc */
  time1 = __rdtsc();                 /* set timer */
  _mm_lfence();                      /* serialize __rdtsc with respect to trailing instructions + compiler barrier for rdtsc and the load */
  int temp = array[ 0 ];             /* array[0] is a cache miss */
  /* measring the write miss latency to array is not meaningful because it's an implementation detail and the next write may also miss */
  /* no need for mfence because there are no stores in between */
  _mm_lfence();                      /* mfence and lfence must be in this order + compiler barrier for rdtsc and the load*/
  time2 = __rdtsc();
  _mm_lfence();                      /* serialize __rdtsc with respect to trailing instructions */
  msl = time2 - time1;

  printf( "array[ 0 ] = %i \n", temp );             /* prevent the compiler from optimizing the load */
  printf( "miss section latency = %lu \n", msl );   /* the latency of everything in between the two rdtsc */

  _mm_mfence();                      /* this properly orders both clflush and rdtsc*/
  _mm_lfence();                      /* mfence and lfence must be in this order + compiler barrier for rdtsc */
  time1 = __rdtsc();                 /* set timer */
  _mm_lfence();                      /* serialize __rdtsc with respect to trailing instructions + compiler barrier for rdtsc and the load */
  temp = array[ 0 ];                 /* array[0] is a cache hit as long as the OS, a hardware prefetcher, or a speculative accesses to the L1D or lower level inclusive caches don't evict it */
  /* measring the write miss latency to array is not meaningful because it's an implementation detail and the next write may also miss */
  /* no need for mfence because there are no stores in between */
  _mm_lfence();                      /* mfence and lfence must be in this order + compiler barrier for rdtsc and the load */
  time2 = __rdtsc();
  _mm_lfence();                      /* serialize __rdtsc with respect to trailing instructions */
  hsl = time2 - time1;

  printf( "array[ 0 ] = %i \n", temp );            /* prevent the compiler from optimizing the load */
  printf( "hit section latency = %lu \n", hsl );   /* the latency of everything in between the two rdtsc */


  _mm_mfence();                      /* this properly orders both clflush and rdtsc */
  _mm_lfence();                      /* mfence and lfence must be in this order + compiler barrier for rdtsc */
  time1 = __rdtsc();                 /* set timer */
  _mm_lfence();                      /* serialize __rdtsc with respect to trailing instructions + compiler barrier for rdtsc */
  /* no need for mfence because there are no stores in between */
  _mm_lfence();                      /* mfence and lfence must be in this order + compiler barrier for rdtsc */
  time2 = __rdtsc();
  _mm_lfence();                      /* serialize __rdtsc with respect to trailing instructions */
  osl = time2 - time1;

  printf( "overhead latency = %lu \n", osl ); /* the latency of everything in between the two rdtsc */


  printf( "Measured L1 hit latency = %lu TSC cycles\n", hsl - osl ); /* hsl is always larger than osl */
  printf( "Measured main memory latency = %lu TSC cycles\n", msl - osl ); /* msl is always larger than osl and hsl */

  return 0;
}
