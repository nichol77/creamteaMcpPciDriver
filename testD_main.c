/*
 *
 * mcpPciDriver: testD_main.c
 *
 * Main module file. Contains startup/shutdown, driver registration, all
 * that jazz.
 *
 * PSA v0.1
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/pci.h>

#include "mcpPciDriver.h"
#include "blk_functions.h"

#define TESTDRIVER_VERSION "0.1"

/*
 * This should be turned into a module parameter soon.
 */
static const struct pci_device_id pci_ids[] = {
  { PCI_VENDOR_ID_XILINX, PCI_DEVICE_ID_MYID, PCI_ANY_ID, PCI_ANY_ID, },
  { 0,0,0,0 }
};


/*
 * PCI Driver functions and structure
 */
static int mcpPciDriver_probe(struct pci_dev *dev, const struct pci_device_id *id);
static void mcpPciDriver_remove(struct pci_dev *dev);

static struct pci_driver pci_driver = {
  .name = "mcpPciDriver",
  .id_table = pci_ids,
  .probe = mcpPciDriver_probe,
  .remove = mcpPciDriver_remove,
};

/*
 * Module parameters
 *
 * "device" is the location of the test device (e.g. "device=0A:0A")
 *
 * It only takes one.
 *
 */
unsigned short testBusSlot=(0xA << 8) | PCI_DEVFN(0xA, 0x0);
char *device = NULL;
module_param(device, charp, S_IRUSR);

// TEST device structure
struct mcpPciDriver_dev test;

// Initialization function
static int __init mcpPciDriver_init(void)
{
  int result;
  
  printk(KERN_INFO "mcpPciDriver: Kernel drive for PCI test device\n");
  printk(KERN_INFO "mcpPciDriver: Author: Patrick Allison\n");
  printk(KERN_INFO "mcpPciDriver: Version: %s\n", TESTDRIVER_VERSION);

  memset(&test, 0, sizeof(struct mcpPciDriver_dev));

  // Get the device ID
  if (device != NULL) {
    unsigned short bus, slot;
    if (sscanf(device, "%hx:%hx", &bus, &slot) < 2) {
      printk(KERN_ERR "mcpPciDriver: did not understand %s as a PCI bus:slot pair\n", device);
      return -1;
    }
    printk(KERN_INFO "mcpPciDriver: Test device is at %2.2X:%2.2X\n",
	   bus, slot);
    testBusSlot = (bus << 8) | PCI_DEVFN(slot, 0);
  }

  // Get the character device major number first,
  // and then the /sysfs class. These need to be done FIRST
  // before the PCI driver is registered, because the probe()
  // functions will be called inside pci_register_device.
  result = mcpPciDriver_getmajor();
  if (result) {
    printk(KERN_ERR "mcpPciDriver: error %d in mcpPciDriver_getmajor\n", result);
    return result;
  }

  result = pci_register_driver(&pci_driver);
  if (result) {
    printk("mcpPciDriver: error %d in pci_register_driver\n", result);
    return result;
  }
   
  return 0;
}

/*
 * PCI Probe/Remove functions
 */
int mcpPciDriver_probe(struct pci_dev *dev, const struct pci_device_id *id) {
  unsigned long startAddress;
  unsigned short busSlot;
  int i;

  busSlot = (dev->bus->number << 8) | (dev->devfn);
  if (busSlot != testBusSlot) return -ENODEV;

  DEBUG("mcpPciDriver: mcpPciDriver_probe\n");  
  DEBUG("mcpPciDriver: mcpPciDriver_probe %x:%x.%x\n", dev->bus->number, PCI_SLOT(dev->devfn), PCI_FUNC(dev->devfn));

  if (pci_enable_device(dev))
      return -1;

  // Copy PCI device pointer
  test.dev = dev;

  /*
   * Loop through and find all of the active BARs.
   */
  for (i=0;i<7;i++) {
    if (pci_resource_len(dev, i) != 0) {
      startAddress = pci_resource_start(dev, i);
      if (check_mem_region(startAddress, pci_resource_len(dev,i))) {
	printk(KERN_ERR 
	       "mcpPciDriver: address region for BAR%d is in use\n",i);
	continue;
      }
      request_mem_region(startAddress, pci_resource_len(dev,i), "mcpPciDriver");
      test.pciBase[i] = ioremap(startAddress, pci_resource_len(dev,i));
      DEBUG("mcpPciDriver: BAR%d (0x%X) at 0x%X\n", 
	    i,
	    (unsigned int) startAddress,
	    (unsigned int) test.pciBase[i]);
      test.pciSpace[i] = 1;
    }
  }
  // BLK Identify
  blk_identify("mcpPciDriver", test.pciBase[1]);
  // Enable bus-mastering
  pci_set_master(test.dev);

  // Initialize semaphore
  init_MUTEX(&test.sema);

  // Initialize waitqueue
  init_waitqueue_head(&test.waiting_on_read);

  // Allocate ring buffer
  test.dmabuf_size = PAGE_ALIGN(150000*sizeof(int));
  testD_dmarb_initRing(&test.ring,test.dmabuf_size,16);

  /*
  // Allocate DMA buffer 
  test.dma.dma_buffer = vmalloc_32(PAGE_ALIGN(150000*sizeof(int)));
  if (!test.dma.dma_buffer) {
    printk(KERN_ERR "mcpPciDriver: unable to allocate DMA buffer\n");
  } else {
    // Sigh, this is different in 2.6.24 and previous...
    //
    unsigned int n_bytes = PAGE_ALIGN(150000*sizeof(int));
    test.dma.buf_size = n_bytes;
    mcpPciDriver_dma_init(dev, &test.dma);
  }
  */
  /*
  test.dma_buffer = (unsigned int *) kmalloc(4096, GFP_DMA);
  if (!test.dma_buffer) {
    printk(KERN_ERR "mcpPciDriver: unable to allocate DMA buffer\n");
  } else {
    for (i=0;i<16;i++) {
      printk(KERN_INFO "buffer[%d] = 0x%X\n", i, test.dma_buffer[i]);
    }
    test.dma_address = dma_map_single(&dev->dev, test.dma_buffer, 4096,
				      DMA_FROM_DEVICE);
    printk(KERN_INFO "mcpPciDriver: dma_address is 0x%X\n", test.dma_address);
  }
  */
  // Test register interrupt handler...
  test.irq_usage = 0;
  mcpPciDriver_registerInterrupt(&test);
				      
  // Sysfs is not implemented yet
  //if (mcpPciDriver_addTestDevice(&test)) {
  //printk(KERN_ERR "mcpPciDriver: unable to register sysfs entry\n");
  //}

  // Register character device
  mcpPciDriver_registerTestCharacterDevice(&test);

  return 0;
}

static void mcpPciDriver_remove(struct pci_dev *dev) {
  int i;

  down(&test.sema);
  mcpPciDriver_unregisterTestCharacterDevice(&test);
  mcpPciDriver_unregisterInterrupt(&test);

  //
  // We need to abort in-progress DMA transfers here
  //
  // release DMA buffer...
  testD_dmarb_deleteRing(&test.ring);
  /*
  mcpPciDriver_dma_finish(dev, &test.dma);
  if (test.dma.dma_buffer) 
     {
	vfree(test.dma.dma_buffer);
	test.dma.dma_buffer = 0x0;
     }  
  */
  /*
  if (test.dma_buffer) {
    dma_unmap_single(&dev->dev, test.dma_address, 4096, DMA_FROM_DEVICE);
    for (i=0;i<128;i++) {
      printk(KERN_INFO "buffer[%d] = 0x%X\n", i, test.dma_buffer[i]);
    }
    kfree(test.dma_buffer);
    test.dma_buffer = 0x0;
  }
  */
  for (i=0;i<7;i++) {
    if (test.pciSpace[i]) {
      iounmap(test.pciBase[i]);
      release_mem_region(pci_resource_start(dev,i),
			 pci_resource_len(dev,i));
      test.pciSpace[i] = 0;
      test.pciBase[i] = 0x0;
    }
  }
  /*
   * Disable the device...
   */
  pci_disable_device(dev);
  /*
   * Remove sysfs entry
   */
  // not implemented yet
  //mcpPciDriver_removeTestDevice(&test);


  test.dev = 0x0;
}

// Shutdown
static void __exit mcpPciDriver_exit(void)
{
  DEBUG("mcpPciDriver: mcpPciDriver_exit\n");
  mcpPciDriver_releasemajor();
  // sysfs not implemented yet
  // mcpPciDriver_removeTestClass();
  pci_unregister_driver(&pci_driver);
}

module_init(mcpPciDriver_init);
module_exit(mcpPciDriver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Patrick Allison <patrick@phys.hawaii.edu>");
MODULE_DESCRIPTION("PCI Test Driver");
MODULE_VERSION(TESTDRIVER_VERSION);
MODULE_DEVICE_TABLE(pci, pci_ids);
