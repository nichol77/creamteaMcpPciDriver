#ifndef TD_RINGBUFFER_H_
#define TD_RINGBUFFER_H_

#include <linux/kernel.h>
#include <linux/spinlock.h>

struct tD_ringbuf {
  unsigned char active;
  unsigned int *buffer;
  unsigned int size;
};

struct tD_ring {
  spinlock_t lock;
  struct tD_ringbuf *ring;
  unsigned int depth;
  unsigned int first;
};

int mcpPciDriver_initRing(struct tD_ring *rp,
			unsigned int depth);

int mcpPciDriver_fastRingEmpty(struct tD_ring *rp);

void mcpPciDriver_deleteRing(struct tD_ring *rp);

void mcpPciDriver_takeFromRing(struct tD_ring *rp,
			     struct tD_ringbuf *buf);

void mcpPciDriver_addToRing(struct tD_ring *rp, 
			  unsigned int *buffer, 
			  unsigned int size);
#endif
