#include "kfifo.h"

void kfifo_init(kfifo_t *fifo, uint8_t *buffer, uint32_t size)
{
	fifo->buffer = buffer;
	fifo->size = size;
	fifo->mask = size - 1;
	fifo->in = 0;
	fifo->out = 0;
}

uint32_t kfifo_put(kfifo_t *fifo, const uint8_t *data, uint32_t len)
{
	uint32_t l;

	len = len < (fifo->size - (fifo->in - fifo->out)) ?
	      len : (fifo->size - (fifo->in - fifo->out));

	l = len < (fifo->size - (fifo->in & fifo->mask)) ?
	    len : (fifo->size - (fifo->in & fifo->mask));

	memcpy(fifo->buffer + (fifo->in & fifo->mask), data, l);
	memcpy(fifo->buffer, data + l, len - l);

	smp_wmb();
	fifo->in += len;

	return len;
}

uint32_t kfifo_get(kfifo_t *fifo, uint8_t *data, uint32_t len)
{
	uint32_t l;

	len = len < (fifo->in - fifo->out) ? len : (fifo->in - fifo->out);

	l = len < (fifo->size - (fifo->out & fifo->mask)) ?
	    len : (fifo->size - (fifo->out & fifo->mask));

	memcpy(data, fifo->buffer + (fifo->out & fifo->mask), l);
	memcpy(data + l, fifo->buffer, len - l);

	smp_rmb();
	fifo->out += len;

	return len;
}
