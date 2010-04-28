
/*
 *
 * testDriver interrupt functions. Works with the 'standard' ISR block layout.
 *
 */

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
//#include <asm/tsc.h>

#define ADAQ_INTERRUPT 0x14
#define ADAQ_INTCSR 0x0
// enable bunches of interrupts
#define ADAQ_INTERRUPT_DEFAULT 0xF0001

#define ADAQ_DMA 0x18
#define ADAQ_DMAADR 0x0
#define ADAQ_DMALEN 0x1
#define ADAQ_DMASR 0x2
#define ADAQ_DMASTART 0x3

#define ADAQ_TX 0x1C
#define ADAQ_TXLEN 0x1

#define OPENCORES_PCI_INTCR 0x7B

#define TEST_INTCSR ADAQ_INTERRUPT + ADAQ_INTCSR
#define TEST_INTCSR_MASTER 0x1
#define TEST_INTCSR_ACTIVE 0x2
#define TEST_INTCSR_DEFAULT ADAQ_INTERRUPT_DEFAULT
#define TEST_INTCSR_IRQ0 0x0100
#define TEST_INTCSR_IRQ1 0x0200
#define TEST_INTCSR_IRQ2 0x0400
#define TEST_INTCSR_IRQ3 0x0800
#define TEST_INTCSR_IRQ4 0x1000
#define TEST_INTCSR_IRQ5 0x2000
#define TEST_INTCSR_IRQ6 0x4000
#define TEST_INTCSR_IRQ7 0x8000

#define TEST_DMASR ADAQ_DMA + ADAQ_DMASR
#define TEST_DMAADR ADAQ_DMA + ADAQ_DMAADR
#define TEST_DMALEN ADAQ_DMA + ADAQ_DMALEN
#define TEST_DMASTART ADAQ_DMA + ADAQ_DMASTART

#define TEST_TXLEN ADAQ_TX + ADAQ_TXLEN

#define MASK(x,y) (x & (~(y)))

#define IRQF_DISABLED 0x00000020
#define IRQF_SHARED 0x00000080

#include "testDriver.h"

irqreturn_t testDriver_interruptHandler(unsigned int irq,
					void *dev_id,
					struct pt_regs *regs);

void testDriver_unregisterInterrupt(struct testDriver_dev *devp) {
  volatile unsigned int *oc_intcsr = (unsigned int *)
    devp->pciBase[0] + OPENCORES_PCI_INTCR;
   volatile unsigned int *intcsr = (unsigned int *)
    devp->pciBase[1] + TEST_INTCSR;
  unsigned int val;
  if (!devp->irq_usage) {
    // nothing to do
    return;
  }
  if (--devp->irq_usage) {
    // nothing to do, still in use
    return;
  }

  // Disable master interrupt
  val = readl(intcsr);
  writel(MASK(val, TEST_INTCSR_MASTER), intcsr);
  
  // That's all we need to do.
  free_irq(devp->dev->irq, devp);
}

int testDriver_registerInterrupt(struct testDriver_dev *devp) {
  unsigned int interruptNumber = 0;
  volatile unsigned int *oc_intcsr = (unsigned int *)
    devp->pciBase[0] + OPENCORES_PCI_INTCR;
   volatile unsigned int *intcsr = (unsigned int *)
    devp->pciBase[1] + TEST_INTCSR;
  int result;

  if (devp->irq_usage) {
    devp->irq_usage++;
    return 0;
  }

  interruptNumber = devp->dev->irq;
  DEBUG("testDriver: taking IRQ%d\n", interruptNumber);
  
  result = request_irq(interruptNumber,
		       testDriver_interruptHandler,
		       IRQF_SHARED | IRQF_DISABLED,
		       "testDriver",
		       devp);
  if (result < 0) {
    printk(KERN_ERR "testDriver: error %d registering interrupt handler\n",
	   result);
    return result;
  }
  devp->irq_usage++;
 
  // enable interrupt propagation
  writel(1, oc_intcsr);
   
  writel(TEST_INTCSR_DEFAULT,
	 intcsr);
  return 0;
}

irqreturn_t testDriver_interruptHandler(unsigned int irq,
					void *dev_id,
					struct pt_regs *regs) {
  struct testDriver_dev *devp = (struct testDriver_dev *) dev_id;
  volatile unsigned int *intcsr = (unsigned int *)
    devp->pciBase[1] + TEST_INTCSR;
  unsigned int val;
  static cycles_t cc = 0;
  cycles_t dt;

  DEBUG("testD_irq.c: testDriver_interruptHandler called!\n"); //Kurtis

  val = readl(intcsr);
  if (!(val & TEST_INTCSR_ACTIVE)) {
    return IRQ_NONE;
  }
  // Clear all interrupts.
  DEBUG("testD_irq.c: interrupt cleared in testDriver_interruptHandler\n"); //Kurtis
  writel(val, intcsr);
  // Handle interrupt
  if (val & TEST_INTCSR_IRQ0) {
    //    DEBUG("testDriver: local IRQ0\n");
    //dt = get_cycles();
    //    DEBUG("testDriver: %llu cycles\n", dt-cc);
    //cc = dt;
    //    do nothing
  }
  //
  // IRQ1 gets processed before IRQ2 if both are active. Need to make
  // sure the DMA transfer is finished and the ring buffer is advanced
  // before starting a new DMA transfer.
  if (val & TEST_INTCSR_IRQ1) {
    //
    // DMA is done...
    //
    volatile unsigned int *dmasr = (unsigned int *) 
      devp->pciBase[1] + TEST_DMASR;
    volatile unsigned int *dmaadr = (unsigned int *)
      devp->pciBase[1] + TEST_DMAADR;
    volatile unsigned int *dmalen = (unsigned int *)
      devp->pciBase[1] + TEST_DMALEN;
    unsigned int dma_status;

    DEBUG("testDriver: local IRQ1\n");

    // Unmap the buffer.
    testDriver_dma_unmap(devp->dev, &devp->dmaringbuf->dma);
    dma_status = readl(dmasr);
    if (dma_status & 0xF) {
      printk(KERN_ERR "testDriver: DMA transfer failed (eflag %X)\n",
	     dma_status & 0xF);
      // buffer is not full
    } else {
      // dma transfer completed successfully
      // spinlock
      spin_lock_irq(&devp->ring.lock);      
      devp->dmaringbuf->size = (devp->dmaringbuf->dma.dma_size)*sizeof(int);
      testD_dmarb_setBufferFull(devp->dmaringbuf);
      spin_unlock_irq(&devp->ring.lock);
      dt = get_cycles();
      DEBUG("testDriver: %llu cycles\n", dt-cc);
      wake_up_interruptible(&devp->waiting_on_read);
    }
  }
  if (val & TEST_INTCSR_IRQ2) {
    volatile unsigned int *dmasr = (unsigned int *) 
      devp->pciBase[1] + TEST_DMASR;
    volatile unsigned int *dmaadr = (unsigned int *)
      devp->pciBase[1] + TEST_DMAADR;
    volatile unsigned int *dmalen = (unsigned int *)
      devp->pciBase[1] + TEST_DMALEN;
    volatile unsigned int *dmastart = (unsigned int *)
      devp->pciBase[1] + TEST_DMASTART;
    volatile unsigned int *txlen = (unsigned int *)
      devp->pciBase[1] + TEST_TXLEN;

    unsigned int dma_status;
    //
    // Data is ready...
    // 
    DEBUG("testDriver: local IRQ2\n");
    cc = get_cycles();
    // Check that DMA is not currently in progress before we
    // do anything.
    dma_status = readl(dmasr);
    if (dma_status & 0x80) {
      DEBUG("testDriver: LIRQ2 (DATA_READY) but DMA in progress?\n");
    } else {
      unsigned int nb_tx;
      nb_tx = readl(txlen);

      // Get DMA ringbuffer
      spin_lock_irq(&devp->ring.lock);      
      DEBUG("testD_irq.c: Trying to fill buffer!\n"); //Kurtis
      devp->dmaringbuf = testD_dmarb_getAvailableBuffer(&devp->ring);
      spin_unlock_irq(&devp->ring.lock);
      if (!devp->dmaringbuf) {
	printk(KERN_ERR 
	       "testDriver: testD_dmarb_getAvailableBuffer returned NULL!");
      } else {
	DEBUG("testDriver: DMA of %d words requested\n", nb_tx);
	if (nb_tx & 0x1) devp->dmaringbuf->dma.dma_size = (nb_tx >> 1) + 1;
	else devp->dmaringbuf->dma.dma_size = (nb_tx >> 1);
	// The buffer's page aligned in size.
	// The DMA transfer doesn't have to be: it'll just stop early.
	DEBUG("testDriver: actually doing DMA of %d ints\n",
	      devp->dmaringbuf->dma.dma_size);

	// Map the DMA buffer...
	testDriver_dma_map(devp->dev, &devp->dmaringbuf->dma);
	DEBUG("testDriver: dma_address is 0x%X\n", 
	      devp->dmaringbuf->dma.dma_address);
	writel(devp->dmaringbuf->dma.dma_address, dmaadr);
	writel(devp->dmaringbuf->dma.dma_size, dmalen);
	// bit 0 starts, bit 1 aborts
	writel(1, dmastart);
      }
    }
  }
 if (val & TEST_INTCSR_IRQ3) {
    DEBUG("testDriver: local IRQ3\n");
  }
  if (val & TEST_INTCSR_IRQ4) {
    DEBUG("testDriver: local IRQ4\n");
  }
  if (val & TEST_INTCSR_IRQ5) {
    DEBUG("testDriver: local IRQ5\n");
  }
  if (val & TEST_INTCSR_IRQ6) {
    DEBUG("testDriver: local IRQ6\n");
  }
  if (val & TEST_INTCSR_IRQ7) {
    DEBUG("testDriver: local IRQ7\n");
  }
//  writel(val, intcsr);
//  val = readl(intcsr);
  return IRQ_HANDLED;
}
