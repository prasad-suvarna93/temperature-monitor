#ifndef TARGET_REGS_H
#define TARGET_REGS_H

// hand written stand-in for a vendor header not a real part

#include <stdint.h>

// register qualifier spelled the way vendor headers do
#define __IO volatile

// timer owns the sampling instant

typedef struct {
  __IO uint32_t CR1;  // control CEN in bit 0
  __IO uint32_t CR2;  // MMS bits master mode drives TRGO
  __IO uint32_t PSC;  // prescaler minus 1
  __IO uint32_t ARR;  // auto-reload minus 1 sets the period
  __IO uint32_t EGR;  // UG in bit 0 forces a register reload
} tim_t;

#define TIM_CR1_CEN (1u << 0)
#define TIM_CR2_MMS_UPDATE (2u << 4)  // update event drives TRGO
#define TIM_EGR_UG (1u << 0)

// adc

typedef struct {
  __IO uint32_t CR1;
  __IO uint32_t CR2;   // ADON DMA DDS EXTEN EXTSEL
  __IO uint32_t SMPR;  // sample time
  __IO uint32_t SQR;   // channel sequence
  __IO uint32_t DR;    // data register the DMA source
} adc_t;

#define ADC_CR2_ADON (1u << 0)
#define ADC_CR2_DMA (1u << 8)
#define ADC_CR2_DDS (1u << 9)            // keep issuing DMA requests
#define ADC_CR2_EXTEN_RISING (1u << 28)  // hardware trigger rising edge
#define ADC_CR2_EXTSEL_TIM_TRGO (6u << 24)

// dma

typedef struct {
  __IO uint32_t CR;    // EN CIRC MINC interrupt enables
  __IO uint32_t NDTR;  // number of items
  __IO uint32_t PAR;   // peripheral address
  __IO uint32_t MAR;   // memory address
  __IO uint32_t ISR;   // status
  __IO uint32_t IFCR;  // status clear
} dma_stream_t;

#define DMA_CR_EN (1u << 0)
#define DMA_CR_CIRC (1u << 8)
#define DMA_CR_MINC (1u << 10)
#define DMA_CR_PSIZE_16 (1u << 11)
#define DMA_CR_MSIZE_16 (1u << 13)
#define DMA_CR_HTIE (1u << 3)  // half-transfer interrupt
#define DMA_CR_TCIE (1u << 4)  // transfer-complete interrupt
#define DMA_CR_TEIE (1u << 2)  // transfer-error interrupt

#define DMA_ISR_HTIF (1u << 4)
#define DMA_ISR_TCIF (1u << 5)
#define DMA_ISR_TEIF (1u << 3)

// gpio

typedef struct {
  __IO uint32_t MODER;
  __IO uint32_t ODR;
  __IO uint32_t BSRR;  // atomic set and reset not read modify write
} gpio_t;

// i2c for the eeprom

typedef struct {
  __IO uint32_t CR1;
  __IO uint32_t CR2;
  __IO uint32_t SR;
  __IO uint32_t DR;
} i2c_t;

#define I2C_SR_BUSY (1u << 1)
#define I2C_SR_RXNE (1u << 2)
#define I2C_SR_TXE (1u << 3)
#define I2C_SR_NACK (1u << 4)
#define I2C_SR_DONE (1u << 5)

// instances

#define TIM_SAMPLE ((tim_t*)0x40000000u)
#define ADC_TEMP ((adc_t*)0x40012000u)
#define DMA_ADC ((dma_stream_t*)0x40026010u)
#define GPIO_LED ((gpio_t*)0x40020400u)
#define I2C_CFG ((i2c_t*)0x40005400u)

// lamps are active high on this board
#define LED_PIN_GREEN 5u
#define LED_PIN_YELLOW 6u
#define LED_PIN_RED 7u

#define EEPROM_I2C_ADDR 0x50u

// APB clock feeding TIM_SAMPLE
#define TIMER_CLK_HZ 84000000u

#endif  // TARGET_REGS_H
