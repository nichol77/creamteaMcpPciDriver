#include <linux/kernel.h>
// The scatterlist API changed at 2.6.24, so we have to include the
// version.h here to be able to #if the changes
#include <linux/version.h>
#include <linux/pci.h>
#include "mcpPciDriver.h"

unsigned int mcpPciDriver_dma_next_segment(unsigned long adr,
					 unsigned int cur_page,
					 unsigned int n_pages);
unsigned int mcpPciDriver_dma_calc_segments(struct tD_dma *dma);

void mcpPciDriver_dma_unmap(struct pci_dev *dev, struct tD_dma *dma)
{
  DEBUG("mcpPciDriver_dma_unmap: dev %#x dma %#x\n",dev,dma);
  if (dma->dma_sglist_buffer) {
    DEBUG("mcpPciDriver_dma_unmap: unamppiing dma_sglist_buffer: %8.8X\n",
	  dma->dma_sglist_buffer);
    dma_unmap_single(&dev->dev,
		     dma->dma_address,
		     dma->n_dma_segments*(sizeof(unsigned int)*2),
		     DMA_TO_DEVICE);
  }
  if (dma->sglist) {
    DEBUG("mcpPciDriver_dma_unmap: unamppiing sglist: %8.8X\n",
	  dma->sglist);
     pci_unmap_sg(dev,
		 dma->sglist,
		 dma->n_segments,
		 PCI_DMA_FROMDEVICE);
  }
}

void mcpPciDriver_dma_finish(struct tD_dma *dma)
{
  DEBUG("mcpPciDriver_dma_finish: dma %#x\n",dma);
  if (dma->dma_sglist_buffer) {
    DEBUG("mcpPciDriver_dma_finish: freeing dma_sglist_buffer: %8.8X\n",
	  (unsigned int) dma->dma_sglist_buffer);
    kfree(dma->dma_sglist_buffer);
    dma->dma_sglist_buffer = 0;
  }
  if (dma->sglist) {
    DEBUG("mcpPciDriver_dma_finish: freeing sglist: %8.8X\n",
	  (unsigned int) dma->sglist);
    vfree(dma->sglist);
    dma->sglist = 0;
  }
}

int mcpPciDriver_dma_init(struct tD_dma *dma) 
{
  DEBUG("mcpPciDriver_dma_init: dma %#x\n",dma);
  unsigned int n_pages;
  unsigned int cur_page;
  unsigned int next_page;
  unsigned int cur_segment;
  unsigned long va;
  int i;
  dma->dma_sglist_buffer = NULL;
  dma->dma_address = 0;
  dma->n_dma_segments = 0;
   
  dma->n_segments = mcpPciDriver_dma_calc_segments(dma);
  if (!dma->n_segments) {
     printk(KERN_ERR "mcpPciDriver: cannot initialize 0-byte DMA buffer\n");
     return -EFAULT;
  }
  dma->sglist = vmalloc(dma->n_segments*sizeof(*(dma->sglist)));
  if (!dma->sglist) {
     printk(KERN_ERR "mcpPciDriver: unable to allocate sglist buffer\n");
     return -ENOMEM;
  }
  DEBUG("mcpPciDriver_dma_init: allocated sglist buffer: %8.8X\n",
	(unsigned int) dma->sglist);
  // OK, we've got an sglist buffer
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)  
  // 2.6.24
  sg_init_table(dma->sglist, dma->n_segments);
#else
  // non-2.6.24
  memset(dma->sglist, 0, dma->n_segments*(sizeof(*(dma->sglist))));
#endif

  // Set up the sglist
  n_pages = (dma->buf_size) >> PAGE_SHIFT;
  DEBUG("mcpPciDriver: buffer is at logical address %8.8X\n", dma->dma_buffer);
  DEBUG("mcpPciDriver: %d pages in %d segments\n", n_pages, dma->n_segments);

  va = (unsigned long) dma->dma_buffer;
  cur_page = 0;
  cur_segment = 0;
  while ((next_page = mcpPciDriver_dma_next_segment(va, cur_page, n_pages))
	 != n_pages) {

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)  
    sg_set_page(dma->sglist + cur_segment,
		vmalloc_to_page((void *) va),
		PAGE_SIZE*(next_page-cur_page),
		0);
#else
    dma->sglist[cur_segment].page = vmalloc_to_page((void *) va);
    dma->sglist[cur_segment].length = PAGE_SIZE*(next_page - cur_page);
    dma->sglist[cur_segment].offset = 0;
#endif

    cur_segment++;
    va = (unsigned long) dma->dma_buffer + (next_page << PAGE_SHIFT);
    cur_page = next_page;
  }
  // last segment...
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)  
    sg_set_page(dma->sglist + cur_segment,
		vmalloc_to_page((void *) va),
		PAGE_SIZE*(n_pages-cur_page),
		0);
#else
    dma->sglist[cur_segment].page = vmalloc_to_page((void *) va);
    dma->sglist[cur_segment].length = PAGE_SIZE*(n_pages - cur_page);
    dma->sglist[cur_segment].offset = 0;
#endif
  return 0;
}

int mcpPciDriver_dma_map(struct pci_dev *dev, struct tD_dma *dma) {
  DEBUG("mcpPciDriver_dma_map: dev %#x dma %#x\n",dev,dma);
  unsigned int *p;
  unsigned int lenflags;
  unsigned int sgaddr;
  int i;

  dma->n_dma_segments = pci_map_sg(dev,
				   dma->sglist,
				   dma->n_segments,
				   PCI_DMA_FROMDEVICE);

  // OK, now allocate a hwbuffer
  dma->dma_sglist_buffer = 
    kmalloc(dma->n_dma_segments*(sizeof(unsigned int)*2), GFP_DMA);
  if (!dma->dma_sglist_buffer) {
    printk(KERN_ERR "mcpPciDriver: unable to allocate sglist hwbuffer\n");
     pci_unmap_sg(dev,
		 dma->sglist,
		 dma->n_segments,
		 PCI_DMA_FROMDEVICE);
    return -1;
  }
  DEBUG("mcpPciDriver_dma_map: allocated dma_sglist_buffer: %8.8X\n",
	dma->dma_sglist_buffer);
  memset(dma->dma_sglist_buffer, 
	 0, 
	 dma->n_dma_segments*(sizeof(unsigned int)*2));
  p = dma->dma_sglist_buffer;
  for (i=0;i<dma->n_dma_segments;i++) {	
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
    lenflags = sg_dma_len(dma->sglist+i)>>2;
    sgaddr = sg_dma_address(dma->sglist+i);
#else
    lenflags = dma->sglist[i].length >> 2;
    sgaddr = dma->sglist[i].dma_address;
#endif
    if (i==0) lenflags |= 0x40000000;
    else if ((i+1)==dma->n_dma_segments) lenflags |= 0x80000000;

    DEBUG("seg %d: %8.8X %8.8X\n", i, sgaddr, lenflags);
    *p++ = sgaddr;
    *p++ = lenflags;
  }
  dma->dma_address = 
    dma_map_single(&dev->dev,
		   dma->dma_sglist_buffer,
		   dma->n_dma_segments*(sizeof(unsigned int)*2),
		   DMA_TO_DEVICE);
  return 0;
}
  

// will return n_pages if there is no next segment
// if call with cur_page = 0, n_pages = 1, loop doesn't enter
unsigned int mcpPciDriver_dma_next_segment(unsigned long adr_start,
					 unsigned int cur_page,
					 unsigned int n_pages) {
  unsigned int i;
  unsigned long adr;
  void *virt;
  void *virt_old;
  struct page *pgentry;

  pgentry = vmalloc_to_page((void *) adr_start);
  virt_old = page_address(pgentry);
  for (i=1;i<(n_pages-cur_page);i++) {
    adr = adr_start + (i<<PAGE_SHIFT);
    pgentry = vmalloc_to_page((void *) adr);
    virt = page_address(pgentry);
    if (virt_old + (1<<PAGE_SHIFT) != virt)
      return i+cur_page;
    virt_old = virt;
  }
  return n_pages;
}

unsigned int mcpPciDriver_dma_calc_segments(struct tD_dma *dma) {
  unsigned int n_segments;
  unsigned int n_pages = (dma->buf_size) >> PAGE_SHIFT;
  unsigned long va;
  unsigned int next_page;
  unsigned int cur_page;
  va = (unsigned long) dma->dma_buffer;
  cur_page = 0;
  n_segments = 0;
  while ((next_page = mcpPciDriver_dma_next_segment(va, cur_page, n_pages))
	 != n_pages) {
    n_segments++;
    cur_page = next_page;
    va = (unsigned long) dma->dma_buffer + (cur_page << PAGE_SHIFT);
  }
  n_segments++;
  return n_segments;
}
