#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/pci.h>

#include "tD_ringbuffer.h"

//
// This is an attempt at a doofy DMA ring buffer.
// Since interrupts can't vmalloc/vfree, the ring buffer
// is primed when it's initialized - that is, it 

// No one else needs to have this macro.
#define TD_RING_NEXT(x,i) (((i)<((x)->depth - 1)) ? (i)+1 : 0)

int testDriver_initRing(struct tD_ring *rp,
			 unsigned int depth) {
  unsigned int i;

  rp->ring = kmalloc(sizeof(struct tD_ringbuf)*depth, GFP_KERNEL);
  if (!rp->ring)
    return -ENOMEM;
  for (i=0;i<depth;i++) {
    rp->ring[i].active = 0;
    rp->ring[i].buffer = NULL;
    rp->ring[i].size = 0;
  }
  rp->depth = depth;
  rp->first = 0;
  spin_lock_init(&rp->lock);
  return 0;
}

void testDriver_deleteRing(struct tD_ring *rp) {
  unsigned long flags;
  int i;

  if (unlikely(rp->ring == NULL)) {
    printk(KERN_ERR "testDriver: ring buffer uninitialized!\n");
    return;
  }
  spin_lock_irqsave(&rp->lock, flags);
  for (i=0;i<rp->depth;i++) {
    if (rp->ring[i].active) {
      if (rp->ring[i].buffer) {
	vfree(rp->ring[i].buffer);
	rp->ring[i].buffer = NULL;
      }
    }
  }
  kfree(rp->ring);
  rp->ring = NULL;
  spin_unlock_irqrestore(&rp->lock, flags);
}

//
// if (!buf->active), then there was no data to get.
//
// The ring should be locked when this function is called.
// 
void testDriver_takeFromRing(struct tD_ring *rp,
			     struct tD_ringbuf *buf) {
  if (unlikely(rp->ring == NULL)) {
    printk(KERN_ERR "testDriver: ring buffer uninitialized!\n");
    buf->active = 0;
    return;
  }
  buf->active = rp->ring[rp->first].active;
  buf->buffer = rp->ring[rp->first].buffer;
  buf->size = rp->ring[rp->first].size;
  if (rp->ring[rp->first].active) {
    rp->ring[rp->first].active = 0;
    rp->ring[rp->first].buffer = NULL;
    rp->first = TD_RING_NEXT(rp,rp->first);
  }
}

int testDriver_fastRingEmpty(struct tD_ring *rp) {
  return (rp->ring[rp->first].active == 0);
}
  
// This will only fail if the ring buffer isn't initialized.
// In that case we promptly trash the buffer. We don't return
// an error code, as this can only happen via stupid coding, but
// it'd be nice to see it happen.
//
// The ring should be locked when this function is called.
//
void testDriver_addToRing(struct tD_ring *rp, 
			  unsigned int *buffer, 
			  unsigned int size) {
  int i;
  
  if (unlikely(rp->ring == NULL)) {
    printk(KERN_ERR "testDriver: ring buffer uninitialized!\n");
    if (buffer) vfree(buffer);
    return;
  }
  i = rp->first;
  do {
    if (!rp->ring[i].active)
      break;
    i = TD_RING_NEXT(rp,i);
  } while (i != rp->first);
  if (i == rp->first) {
    if (rp->ring[rp->first].active) {
      // ring buffer is full.
      if (rp->ring[rp->first].buffer) {
	vfree(rp->ring[rp->first].buffer);
	rp->ring[rp->first].buffer = NULL;
      }
      rp->first = TD_RING_NEXT(rp,rp->first);
    }
  }
  rp->ring[i].buffer = buffer;
  rp->ring[i].size = size;
  rp->ring[i].active = 1;
}
