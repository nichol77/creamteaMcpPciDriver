/*
 *
 * testDriver: testDriver.h
 *
 * Main header file. Contains device structure definitions.
 *
 * PSA v1.0 03/21/08
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/cdev.h>

#include "testDriver_ioctl.h"
#include "testD_dma.h"
#include "testD_dma_ringbuffer.h"

#ifndef PCI_VENDOR_ID_XILINX
#define PCI_VENDOR_ID_XILINX 0x10ee
#endif

#define PCI_DEVICE_ID_MYID 0x1

#define TESTDRIVER_RINGBUFFER_DEPTH 16

struct testDriver_dev {
  dev_t devno;                        // device number (major/minor chardev)
  struct pci_dev *dev;                // PCI Device Structure
  unsigned int pciSpace[6];           // '1' if space is enabled, '0' if not
  volatile unsigned int *pciBase[6];  // PCI base addresses
  struct semaphore sema;              // mutex semaphore
  struct cdev cdev;                   // character device
  size_t dmabuf_size;
  struct tD_ringbuf *dmaringbuf;
  struct tD_ring ring;
  unsigned int irq_usage;
  wait_queue_head_t waiting_on_read;
};

// tioD_char.c
int testDriver_getmajor(void);
void testDriver_releasemajor(void);
int testDriver_registerTestCharacterDevice(struct testDriver_dev *dev);
void testDriver_unregisterTestCharacterDevice(struct testDriver_dev *dev);

// interrupts
int testDriver_registerInterrupt(struct testDriver_dev *devp);
void testDriver_unregisterInterrupt(struct testDriver_dev *devp);

/*
 * DEBUG...
 */
//#define DEBUG(format, ...)
#define DEBUG(format, ...) printk(KERN_INFO format, ## __VA_ARGS__)
