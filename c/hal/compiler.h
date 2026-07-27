#ifndef COMPILER_H
#define COMPILER_H

// orders the sample writes before the queue index write
#ifdef HOST_BUILD
#define MEM_BARRIER() ((void)0)
#else
#define MEM_BARRIER() __asm volatile("dmb 0xF" ::: "memory")
#endif

// keeps hot code in fast memory on target
#ifdef HOST_BUILD
#define FAST_TEXT
#else
#define FAST_TEXT __attribute__((section(".fasttext")))
#endif

#define UNUSED(x) ((void)(x))

#endif  // COMPILER_H
