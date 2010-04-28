#ifndef TD_RINGBUFFER_H_
#define TD_RINGBUFFER_H_

#include <linux/kernel.h>
#include <linux/spinlock.h>

struct tD_ringbuf {
  unsigned char active;
  unsigned int size;
  struct tD_dma dma;
};

struct tD_ring {
  spinlock_t lock;
  struct tD_ringbuf *ring;
  unsigned int depth;
  unsigned int first;
};
int testD_dmarb_initRing(struct tD_ring *rp,
			unsigned int size,
			 unsigned int depth);
void testD_dmarb_deleteRing(struct tD_ring *rp);
struct tD_ringbuf *testD_dmarb_getAvailableBuffer(struct tD_ring *rp);
void testD_dmarb_setBufferFull(struct tD_ringbuf *buf);
struct tD_ringbuf *testD_dmarb_getFirstBuffer(struct tD_ring *rp);
int testD_dmarb_setBufferEmpty(struct tD_ring *rp,
			       struct tD_dma *dma);
int testD_dmarb_fastRingEmpty(struct tD_ring *rp);

#endif
