/*
 *
 * testDriver: tioD_char.c
 *
 * testDriver character interface.
 *
 * PSA v1.0 04/17/08
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/ioctl.h>
#include <linux/fcntl.h>
#include <linux/cdev.h>
#include <asm/msr.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include "testDriver.h"
#include "mcp_register_map.h"

#define VERBOSE_DEBUG 0

static int testDriver_major;

static int testDriver_open(struct inode *iptr, struct file *filp);
static int testDriver_release(struct inode *iptr, struct file *filp);
static ssize_t testDriver_read(struct file *filp, char __user *buff,
			size_t count, loff_t *offp);
static ssize_t testDriver_write(struct file *filp, const char __user *buff,
			size_t count, loff_t *offp);
static int testDriver_ioctl(struct inode *iptr, struct file *filp,
		      unsigned int cmd, unsigned long arg);


static struct file_operations testDriver_fops = {
  .owner = THIS_MODULE,
  .read = testDriver_read,
  .write = testDriver_write,
  .ioctl = testDriver_ioctl,
  .open = testDriver_open,
  .release = testDriver_release,
};

void __exit testDriver_releasemajor(void) {
   dev_t dev;
   if (!testDriver_major) return;
   dev = MKDEV(testDriver_major, 0);
   unregister_chrdev_region(dev, 1);
}


int __init testDriver_getmajor(void) {
  int result;
  dev_t dev;

  if (testDriver_major) {
    dev = MKDEV(testDriver_major, 0);
    result = register_chrdev_region(dev, 1, "testDriver");
  } else {
    result = alloc_chrdev_region(&dev, 0, 1, "testDriver");
    testDriver_major = MAJOR(dev);
  }
  if (result < 0) {
    printk(KERN_WARNING "testDriver: can't get major %d\n",
	   testDriver_major);
    return result;
  }
  DEBUG("testDriver: got major number %d\n", testDriver_major);
  return 0;
}

int testDriver_registerTestCharacterDevice(struct testDriver_dev *dev)
{
  int err=0;
  dev_t devno = MKDEV(testDriver_major, 0);

  DEBUG("testDriver_registerTestCharacterDevice: adding /dev/test\n");
  cdev_init(&dev->cdev, &testDriver_fops);
  dev->cdev.owner = THIS_MODULE;
  //  dev->cdev.ops = &testDriver_fops;
  dev->devno = devno;
  err = cdev_add(&dev->cdev, devno, 1);
  if (err)
    printk(KERN_NOTICE "testDriver: error %d adding /dev/test", err);

  return err;
}

void testDriver_unregisterTestCharacterDevice(struct testDriver_dev *dev) {
  cdev_del(&dev->cdev);
}

/*
 *  THE FOPS FUNCTIONS
 *
 *  These are the file operations (fops) functions which lock the TEST:
 *     testDriver_ioctl
 *     testDriver_read
 *     testDriver_write
 *  These do not:
 *     testDriver_open
 *     testDriver_release
 */
static int testDriver_open(struct inode *iptr, struct file *filp)
{
  /*
   * Get the testDriver_dev pointer by bouncing out
   * of the cdev structure.
   */
  struct testDriver_dev *devp = container_of(iptr->i_cdev,
					     struct testDriver_dev,
					     cdev);
  /*
   * Store it for easier access.
   */
  filp->private_data = devp;
  /*
   * Do we need to do anything here?
   */ 
  DEBUG("testDriver_open: /dev/test opened\n");
  return 0;
}
static int testDriver_release(struct inode *inode, struct file *filp)
{
  /*
   * uh, I don't care
   */
  return 0;
}

static ssize_t testDriver_read(struct file *filp, char __user *buff,
			size_t count, loff_t *offp) {
  struct testDriver_dev *devp = filp->private_data;
  struct tD_ringbuf curbuf;
  struct tD_ringbuf *rbuf;
  struct tD_dma dma;
  unsigned long flags;
  unsigned int nbytes;
  unsigned int *p;
  unsigned int temp;
  unsigned int tempnext;
  unsigned int i;
  int retval;

  if (VERBOSE_DEBUG) {
    DEBUG("Called testDriver_read\n");
    DEBUG("Current semaphore count: %i\n",atomic_read(&devp->sema.count));
  }

  if (filp->f_flags & O_NONBLOCK) {
    if (down_trylock(&devp->sema)) {
      if (VERBOSE_DEBUG)
        DEBUG("Returning EAGAIN on line 152\n");
      return -EAGAIN;
    }
    if (testD_dmarb_fastRingEmpty(&devp->ring)) {
      up(&devp->sema);
      if (VERBOSE_DEBUG)
        DEBUG("Returning EAGAIN on line 157\n");
      return -EAGAIN;
    } 
  } else {
      if (down_interruptible(&devp->sema))
	return -ERESTARTSYS;
  }


  // OK, at this point if we're O_NONBLOCK
  // we should have data. So it's worth allocating the buffer.
  dma.dma_buffer = vmalloc_32(devp->dmabuf_size);
  dma.buf_size = devp->dmabuf_size;
  if (!dma.dma_buffer) {
    printk(KERN_ERR "testDriver: unable to allocate replacement DMA buffer\n");
    return -ENOMEM;
  }
  if (testDriver_dma_init(&dma)) {
    printk(KERN_ERR "testDriver: error initializing replacement DMA buffer\n");
    return -ENOMEM;
  }

  // If we're not, we still could block, but in this case
  // we won't be enabling/disabling interrupts crazy-often.
  spin_lock_irqsave(&devp->ring.lock, flags);
  rbuf = testD_dmarb_getFirstBuffer(&devp->ring);
  while (!rbuf->active) {
    spin_unlock_irqrestore(&devp->ring.lock, flags);
    up(&devp->sema);
    if (filp->f_flags & O_NONBLOCK) {
      testDriver_dma_finish(&dma);
      vfree(dma.dma_buffer);
      if (VERBOSE_DEBUG)
        DEBUG("Returning EAGAIN on line 189\n");
      return -EAGAIN;
    }
    DEBUG("testDriver: sleeping\n");
    if (wait_event_interruptible(devp->waiting_on_read, 
				 !testD_dmarb_fastRingEmpty(&devp->ring))) {
      testDriver_dma_finish(&dma);
      vfree(dma.dma_buffer);
      return -ERESTARTSYS;
    }
    if (filp->f_flags & O_NONBLOCK) {
      if (down_trylock(&devp->sema)) {
	testDriver_dma_finish(&dma);
	vfree(dma.dma_buffer);
        if (VERBOSE_DEBUG)
       	  DEBUG("Returning EAGAIN on line 203\n");
	return -EAGAIN;
      }
    } else if (down_interruptible(&devp->sema)) {
      testDriver_dma_finish(&dma);
      vfree(dma.dma_buffer);
      return -ERESTARTSYS;
    }
    spin_lock_irqsave(&devp->ring.lock, flags);
    rbuf = testD_dmarb_getFirstBuffer(&devp->ring);
  }
  // OK, we have data. Replace the ring with our buffer - someone else
  // will vfree it.
  curbuf = *rbuf;
  testD_dmarb_setBufferEmpty(&devp->ring,
			     &dma);
  // Unlock. At this point we're done with the ring: we only lock the ring
  // to make sure between when we get the buffer and when we replace it, no
  // one else tries to access it.
  // 
  spin_unlock_irqrestore(&devp->ring.lock, flags);
   
  // curbuf.size is in bytes
  DEBUG("testDriver: buffer is %d bytes\n", curbuf.size);

  // Whew, all that bloody work, but we've got the buffer!
  testDriver_dma_finish(&curbuf.dma);

  // MCP copying function
  p = (unsigned int *) curbuf.dma.dma_buffer;
  /*
  for (i=0;i<(curbuf.size/sizeof(int));i++) {
    DEBUG("%8.8d %8.8X\n", i, p[i]);
  }
  */
  if (curbuf.size < 4) {
    printk(KERN_WARNING "testDriver: buffer is far too small?\n");
    vfree(curbuf.dma.dma_buffer);
    up(&devp->sema);
    return -EIO;
  }
  if (p[0] != 0x12341234) {
    printk(KERN_WARNING "testDriver: improper header (%4.4X %4.4X)\n",
	   p[0] & 0xFFFF,(p[0] & 0xFFFF0000 >> 16));
    vfree(curbuf.dma.dma_buffer);
    up(&devp->sema);
    return -EIO;
  }
  if (unlikely(put_user(p[0], (unsigned int *) buff) < 0)) {
    vfree(curbuf.dma.dma_buffer);
    up(&devp->sema);
    return -EFAULT;
  }
  buff += sizeof(unsigned int);
  nbytes = sizeof(unsigned int);
  for (i=2;i<(curbuf.size >> 1);i++) {
    // Data is packed as
    // 0x00010000 0x00020003
    // i is an index of words, so...
    if (!(i & 0x1)) {
      // if even, we want the low word
      temp = p[i>>1] & 0xFFFF;
      tempnext = (p[i>>1] & 0xFFFF0000)>>16;
    } else {
      temp = (p[i>>1] & 0xFFFF0000) >> 16;
      if (i+1 == (curbuf.size >> 1)) {
	tempnext = 0;
      } else {
	tempnext = p[(i+1)>>1] & 0xFFFF;
      }
    }
    if (temp == 0xFFFF) temp = 0xFFFFFFFF;
    if (temp == 0x4321 && tempnext == 0x4321) temp = 0x43214321;
    if (unlikely(put_user(temp, (unsigned int *) buff) < 0)) {
      vfree(curbuf.dma.dma_buffer);
      up(&devp->sema);
      return -EFAULT;
    }
    nbytes += sizeof(unsigned int);
    if (temp == 0x43214321) {
      vfree(curbuf.dma.dma_buffer);
      up(&devp->sema);
      return nbytes;
    }
    if (nbytes+sizeof(unsigned int) > count) {
      printk(KERN_WARNING "testDriver: warning - user buffer too small\n");
      vfree(curbuf.dma.dma_buffer);
      up(&devp->sema);
      return nbytes;
    }
    buff += sizeof(unsigned int);
  }
  
  /*
  if (count < curbuf.size) {
    // too small...!
    // fix this later
    DEBUG("testDriver: buffer too small\n");
    if (copy_to_user(buff, curbuf.dma.dma_buffer, count)) {
      vfree(curbuf.dma.dma_buffer);
      up(&devp->sema);

      return -EFAULT;
    }
    nbytes = count;
  } else {
    if (copy_to_user(buff, 
		     curbuf.dma.dma_buffer, 
		     curbuf.size)) {
      vfree(curbuf.dma.dma_buffer);
      up(&devp->sema);
      return -EFAULT;
    }
    nbytes = curbuf.size;
  }
  */
  // no kfree-ing
  printk(KERN_WARNING "testDriver: warning - no trailer seen\n");
  vfree(curbuf.dma.dma_buffer);
  up(&devp->sema);
  return nbytes;
}
      
static ssize_t testDriver_write(struct file *filp, const char __user *buff,
			 size_t count, loff_t *offp) {
  struct testDriver_dev *devp = filp->private_data;
  volatile unsigned int *cmdptr = BLK_MCP(devp->pciBase[1], BLK_MCP_COMMAND);
  unsigned int command;
  int i;

  if (VERBOSE_DEBUG) {
    DEBUG("Called testDriver_write\n");
    DEBUG("Current semaphore count: %i\n",atomic_read(&devp->sema.count));
  }

  DEBUG("testDriver_write: %d bytes\n", count);
  if (count & 0x3) {
    // WTF, I only want integers)
    printk(KERN_ERR "testDriver: writes should be integer-length only\n");
    return -EFAULT;
  }
  for (i=0;i<(count >> 2);i++) {
    if (get_user(command, buff) < 0)
      return -EFAULT;
    buff += sizeof(unsigned int);
    DEBUG("testDriver: cmd %8.8X\n", command);
    writel(command, cmdptr);
  }

  return count;
}


/*
 * test I/O controls
 *
 *
 */

static int testDriver_ioctl(struct inode *iptr, struct file *filp,
			     unsigned int cmd, unsigned long arg)
{

  int err = 0;
  unsigned int gpioval;
  struct testDriverRead req;

  /*
   * Get testDriver_dev pointer.
   */
  struct testDriver_dev *devp = filp->private_data;

  if (VERBOSE_DEBUG) {
    DEBUG("Called testDriver_ioctl\n");
    DEBUG("Current semaphore count: %i\n",atomic_read(&devp->sema.count));
  }

  if (_IOC_TYPE(cmd) != TEST_IOC_MAGIC) return -ENOTTY;
  if (_IOC_NR(cmd) < TEST_IOC_MINNR) return -ENOTTY;
  if (_IOC_NR(cmd) > TEST_IOC_MAXNR) return -ENOTTY;
  if (_IOC_DIR(cmd) & _IOC_READ)
    err = !access_ok(VERIFY_WRITE, (void __user *) arg, _IOC_SIZE(cmd));
  if (_IOC_DIR(cmd) & _IOC_WRITE)
    err = !access_ok(VERIFY_READ, (void __user *) arg, _IOC_SIZE(cmd));
  if (err) return -EFAULT;

  if (!devp) {
    printk(KERN_ERR "testDriver: no testDriver_dev pointer?\n");
    return -EFAULT;
  }
  
  /*
   * Get exclusive access to the device.
   *
   * Don't block if O_NONBLOCK is set. I really should make this
   * some sort of a macro or something.
   */
  if (filp->f_flags & O_NONBLOCK) {
    if (down_trylock(&devp->sema)) {
      return -EAGAIN;
    }
  } else if (down_interruptible(&devp->sema)) {
    return -ERESTARTSYS;
  }

  switch (cmd) {  
      case TEST_IOCREAD:
	if (copy_from_user(&req, (void __user *) arg,
			   sizeof(struct testDriverRead))) {
	  up(&devp->sema);
	  return -EFAULT;
	}
	if (req.barno > 6) {
	  up(&devp->sema);
	  return -EFAULT;
	}
	if (!devp->pciSpace[req.barno]) {
	  up(&devp->sema);
	  return -ENOTTY;
	}
//	DEBUG("testDriver: read from %8.8X, BAR%d\n",
//	      req.address, req.barno);
	req.value = readl(devp->pciBase[req.barno]+req.address);
	if (copy_to_user((void __user *) arg, &req, sizeof(struct testDriverRead))) {
	  up(&devp->sema);
	  return -EFAULT;
	}
	break;
      case TEST_IOCWRITE:
	if (copy_from_user(&req, (void __user *) arg,
			   sizeof(struct testDriverRead))) {
	  up(&devp->sema);
	  return -EFAULT;
	}
	if (req.barno > 6) {
	  up(&devp->sema);
	  return -EFAULT;
	}
	if (!devp->pciSpace[req.barno]) {
	  up(&devp->sema);
	  return -ENOTTY;
	}
	DEBUG("testDriver: write %8.8X to %8.8X, BAR%d\n",
	      req.value, req.address, req.barno);
	writel(req.value, devp->pciBase[req.barno]+req.address);
	// DO NOT READ BACK HERE!!!
	// After the command's been written, the device could start
	// Doing Something, and reading back could affect that.
	// If the user wants to read back, they should just do another
	// ioctl call.
	//	req.value = readl(devp->pciBase[req.barno]+req.address);
	if (copy_to_user((void __user *) arg, &req, sizeof(struct testDriverRead))) {
	  up(&devp->sema);
	  return -EFAULT;
	}
	break;
  default:
    err=-ENOTTY;
    break;
  }
  // All proper exits pass through here
  // I really should clean this up so there's one up for each down.
  up(&devp->sema);
  return err;
}
