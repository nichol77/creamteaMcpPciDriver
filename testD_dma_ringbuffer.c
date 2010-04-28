#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include "testDriver.h"
#include "testD_dma.h"
#include "testD_dma_ringbuffer.h"

//
// This is an attempt at a doofy DMA ring buffer.
// Since interrupts can't vmalloc/vfree, the ring buffer
// is primed when it's initialized. Each ringbuffer entry
// already has a vmalloc'ed buffer, and a scattergather list
// primed and ready to go.

// No one else needs to have this macro.
#define TD_RING_NEXT(x,i) (((i)<((x)->depth - 1)) ? (i)+1 : 0)

// Size is in bytes.
int testD_dmarb_initRing(struct tD_ring *rp,
			unsigned int size,
			 unsigned int depth) {
  unsigned int i;

  rp->ring = kmalloc(sizeof(struct tD_ringbuf)*depth, GFP_KERNEL);
  if (!rp->ring)
    return -ENOMEM;
  for (i=0;i<depth;i++) {
    rp->ring[i].active = 0;
    rp->ring[i].dma.dma_buffer = NULL;
    rp->ring[i].size = 0;
  }
  rp->depth = depth;
  rp->first = 0;
  spin_lock_init(&rp->lock);
  for (i=0;i<depth;i++) {
    rp->ring[i].dma.dma_buffer = vmalloc_32(size);
    rp->ring[i].dma.buf_size = size;
    if (!rp->ring[i].dma.dma_buffer) {
      printk(KERN_ERR "testDriver: unable to allocate DMA buffer #%d\n", i);
      return -ENOMEM;
    }
    if (testDriver_dma_init(&rp->ring[i].dma))
      return -ENOMEM;
  }
      
  return 0;
}

void testD_dmarb_deleteRing(struct tD_ring *rp) {
  int i;

  if (unlikely(rp->ring == NULL)) {
    printk(KERN_ERR "testDriver: ring buffer uninitialized!\n");
    return;
  }
  for (i=0;i<rp->depth;i++) {
    DEBUG("testD_dmarb_deleteRing: freeing ring buffer %d\n", i);
    testDriver_dma_finish(&rp->ring[i].dma);
    if (rp->ring[i].dma.dma_buffer)
      vfree(rp->ring[i].dma.dma_buffer);
  }

  kfree(rp->ring);
  rp->ring = NULL;
}

//
// if (!buf->active), then there was no data to get.
//
// The ring should be locked when this function is called.
// 

//
// OK, the three things we need to do are:
// 1) get available buffer
// 2) set buffer full
// 3) get full buffer
// 4) set buffer empty (by providing new buffer)


struct tD_ringbuf *testD_dmarb_getAvailableBuffer(struct tD_ring *rp) {
  int i;
  
  if (unlikely(rp->ring == NULL)) {
    printk(KERN_ERR "testDriver: ring buffer uninitialized!\n");
    return NULL;
  }
  i = rp->first;
  do {
    if (!rp->ring[i].active)
      break;
    i = TD_RING_NEXT(rp,i);
  } while (i != rp->first);
  if (i == rp->first && rp->ring[i].active) {
    // ring buffer full
    printk(KERN_WARNING "testDriver: ring buffer overflow, lost data\n");
    rp->first = TD_RING_NEXT(rp, rp->first);
    rp->ring[i].active = 0;
  }
  return &rp->ring[i];
}

void testD_dmarb_setBufferFull(struct tD_ringbuf *buf) {
  buf->active = 1;
}

struct tD_ringbuf *testD_dmarb_getFirstBuffer(struct tD_ring *rp) {
  DEBUG("testDriver: providing buffer %d (active: %d)\n",
	rp->first, rp->ring[rp->first].active);
  if (unlikely(rp->ring == NULL)) {
    printk(KERN_ERR "testDriver: ring buffer uninitialized!\n");
    return NULL;
  }
  return &rp->ring[rp->first];
}

int testD_dmarb_setBufferEmpty(struct tD_ring *rp,
			       struct tD_dma *dma) {
  rp->ring[rp->first].dma = *dma;
  rp->ring[rp->first].active = 0;
  rp->first = TD_RING_NEXT(rp, rp->first);
  return 0;
}

int testD_dmarb_fastRingEmpty(struct tD_ring *rp) {
  return (rp->ring[rp->first].active == 0);
  //  return 0;
}

/*
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
    rp->first = TD_RING_NEXT(rp,rp->first);
  }
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
    vfree(buffer);
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
      vfree(rp->ring[rp->first].buffer);
      rp->first = TD_RING_NEXT(rp,rp->first);
    }
  }
  rp->ring[i].buffer = buffer;
  rp->ring[i].size = size;
  rp->ring[i].active = 1;
}
*/
