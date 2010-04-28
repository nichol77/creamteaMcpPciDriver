#ifndef TESTD_DMA_H_
#define TESTD_DMA_H_
struct tD_dma 
{
  unsigned int *dma_buffer;
  unsigned int *dma_sglist_buffer;
  dma_addr_t dma_address;
  struct scatterlist *sglist;
  unsigned int n_segments;
  unsigned int n_dma_segments;
  size_t dma_size;
  size_t buf_size;
};

void testDriver_dma_finish(struct tD_dma *dma);
int testDriver_dma_init(struct tD_dma *dma);
int testDriver_dma_map(struct pci_dev *dev, struct tD_dma *dma);
void testDriver_dma_unmap(struct pci_dev *dev, struct tD_dma *dma);
#endif
