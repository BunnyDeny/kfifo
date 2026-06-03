#include "kfifo.h"
#include <stdio.h>

#define FIFO_SZ 32

int main(void)
{
	uint8_t buf[FIFO_SZ];
	uint8_t rbuf[64];
	kfifo_t fifo;

	kfifo_init(&fifo, buf, FIFO_SZ);

	uint8_t data[20] = "Hello kfifo!";
	kfifo_put(&fifo, data, 20);
	printf("len = %u\n", kfifo_len(&fifo));

	uint32_t got = kfifo_get(&fifo, rbuf, 7);
	printf("got = %u, data = %.*s\n", got, (int)got, rbuf);

	printf("free = %u\n", kfifo_avail(&fifo));

	kfifo_reset(&fifo);
	printf("len after reset = %u\n", kfifo_len(&fifo));

	return 0;
}
