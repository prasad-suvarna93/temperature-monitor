#include "sample_queue.h"

#include "compiler.h"

#define MASK (SAMPLE_QUEUE_DEPTH - 1u)

// volatile is enough for one producer and one consumer on Cortex-M
// the barriers below supply the ordering

void SampleQueueInit(sample_queue_t* queue) {
  queue->head     = 0u;
  queue->tail     = 0u;
  queue->overruns = 0u;
}

bool SampleQueuePush(sample_queue_t* queue, uint8_t block_index) {
  const uint32_t head = queue->head;

  // unsigned so it stays correct across the wrap
  if ((uint32_t)(head - queue->tail) >= SAMPLE_QUEUE_DEPTH) {
    queue->overruns++;
    return false;  // full so drop it
  }

  queue->slot[head & MASK] = block_index;

  // write the slot before publishing the head
  MEM_BARRIER();
  queue->head = head + 1u;

  return true;
}

bool SampleQueuePop(sample_queue_t* queue, uint8_t* block_index) {
  const uint32_t tail = queue->tail;

  if (queue->head == tail) return false;

  // pairs with the push barrier
  MEM_BARRIER();
  *block_index = queue->slot[tail & MASK];

  // release the slot only after taking the value
  MEM_BARRIER();
  queue->tail = tail + 1u;

  return true;
}

uint32_t SampleQueueOverruns(const sample_queue_t* queue) { return queue->overruns; }
