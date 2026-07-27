// ADC is hardware triggered not polled
#ifndef HAL_ADC_H
#define HAL_ADC_H

#include <stdbool.h>
#include <stdint.h>

#include "temp_types.h"

// runs in interrupt context and gets an index not the data
typedef void (*hal_adc_block_cb_t)(uint8_t block_index, void* ctx);

// programs hardware but does not start sampling
bool HalAdcInit(uint32_t sample_period_us, hal_adc_block_cb_t cb, void* ctx);

bool HalAdcStart(void);
void HalAdcStop(void);

// valid until the DMA wraps back onto it
const adc_raw_t* HalAdcBlock(uint8_t block_index);

#endif  // HAL_ADC_H
