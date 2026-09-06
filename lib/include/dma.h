#ifndef DMA_H
#define DMA_H
#include <stdint.h>
#define ARM_TO_DMA_BUS(x) ((uint32_t)(x) | 0x40000000)

#define DMA_BASE_ARM 0x20007000
#define DMA_CHAN_BASE(n) (DMA_BASE_ARM + 0x100 * (n))

#define DMA_CS_REG(n) (DMA_CHAN_BASE(n) + 0x00)
#define DMA_CONBLK_AD_REG(n) (DMA_CHAN_BASE(n) + 0x04)

#define DMA_CS_ACTIVE (1 << 0)
#define DMA_CS_END (1 << 1)
#define DMA_CS_RESET (1 << 31)

#define DMA_TI_INTEN (1 << 0)
#define DMA_TI_TDMODE (1 << 1)
#define DMA_TI_WAIT_RESP (1 << 3)
#define DMA_TI_DEST_INC (1 << 4)
#define DMA_TI_DEST_WIDTH (1 << 5)
#define DMA_TI_DEST_DREQ (1 << 6)
#define DMA_TI_DEST_IGNORE (1 << 7)
#define DMA_TI_SRC_INC (1 << 8)
#define DMA_TI_SRC_WIDTH (1 << 9)
#define DMA_TI_SRC_DREQ (1 << 10)
#define DMA_TI_SRC_IGNORE (1 << 11)

#define DMA_TI_PERMAP(x) ((x) << 16)
#define DMA_TI_NO_WIDE_BURSTS (1 << 26)

#define DMA_PERMAP_SPI0_TX 6
#define DMA_PERMAP_SPI0_RX 7
#define DMA_PERMAP_PCM_TX 2
#define DMA_TI_SPI_TX (DMA_TI_SRC_INC | DMA_TI_DEST_DREQ | DMA_TI_WAIT_RESP | DMA_TI_PERMAP(DMA_PERMAP_SPI0_TX))
#define DMA_TI_SPI_RX (DMA_TI_SRC_DREQ | DMA_TI_DEST_IGNORE | DMA_TI_WAIT_RESP | DMA_TI_PERMAP(DMA_PERMAP_SPI0_RX))
#define DMA_TI_PCM_TX (DMA_TI_SRC_INC | DMA_TI_DEST_DREQ | DMA_TI_WAIT_RESP | DMA_TI_PERMAP(DMA_PERMAP_PCM_TX))

#define MK_REG(addr) (*(volatile uint32_t *)(addr))

__attribute__((aligned(32))) typedef struct
{
    uint32_t ti;
    uint32_t source_ad;
    uint32_t dest_ad;
    uint32_t txfr_len;
    uint32_t stride;
    uint32_t nextconbk;
    uint32_t reserved1;
    uint32_t reserved2;
} dma_cb_t;

#endif