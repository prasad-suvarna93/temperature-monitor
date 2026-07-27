// lock free single producer isr and single consumer main loop
#ifndef SAMPLE_QUEUE_H
#define SAMPLE_QUEUE_H

#include "config.h"
#include "temp_types.h"

#define SAMPLE_QUEUE_DEPTH SAMPLE_BLOCKS

_Static_assert((SAMPLE_QUEUE_DEPTH & (SAMPLE_QUEUE_DEPTH - 1u)) == 0u,
               "depth must be a power of two -- the index masking assumes it");

typedef struct {
  // unsigned so the subtraction stays correct across the wrap
  volatile uint32_t head;
  volatile uint32_t tail;
  volatile uint32_t overruns;
  uint8_t slot[SAMPLE_QUEUE_DEPTH];
} sample_queue_t;

void SampleQueueInit(sample_queue_t* queue);

// interrupt context bounded with no loop
bool SampleQueuePush(sample_queue_t* queue, uint8_t block_index);

// main loop only
bool SampleQueuePop(sample_queue_t* queue, uint8_t* block_index);

uint32_t SampleQueueOverruns(const sample_queue_t* queue);

#endif
