#ifndef KFIFO_H
#define KFIFO_H

#include <stdint.h>
#include <string.h>

/* 内存屏障定义：仅支持 GCC / Keil MDK / IAR */
#if defined(__CC_ARM) || defined(__ARMCC_VERSION)
/* Keil MDK (ARMCC 5 / ARMCLANG 6) */
#include <intrinsics.h>
#define smp_wmb() __DMB()
#define smp_rmb() __DMB()
#elif defined(__GNUC__)
/* GCC */
#define smp_wmb() __sync_synchronize()
#define smp_rmb() __sync_synchronize()
#elif defined(__ICCARM__)
/* IAR Embedded Workbench for ARM */
#include <intrinsics.h>
#define smp_wmb() __DMB()
#define smp_rmb() __DMB()
#else
#error "Unsupported compiler. Please use GCC, Keil MDK, or IAR."
#endif

typedef struct {
	uint8_t *buffer;
	uint32_t size;
	uint32_t mask;
	uint32_t in;
	uint32_t out;
} kfifo_t;

void kfifo_init(kfifo_t *fifo, uint8_t *buffer, uint32_t size);
uint32_t kfifo_put(kfifo_t *fifo, const uint8_t *data, uint32_t len);
uint32_t kfifo_get(kfifo_t *fifo, uint8_t *data, uint32_t len);

static inline uint32_t kfifo_len(kfifo_t *fifo)
{
	return fifo->in - fifo->out;
}

static inline void kfifo_reset(kfifo_t *fifo)
{
	fifo->in = fifo->out;
}

static inline uint32_t kfifo_avail(kfifo_t *fifo)
{
	return fifo->size - (fifo->in - fifo->out);
}

#endif
