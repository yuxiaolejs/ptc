#include "pcm.h"
#include "dma.h"

void gpio_pin_set_func(uint32_t pin, uint32_t func);
#define GPIO_ALT0 4

#define SYS_CLO 0x20003004u

static void pcm_delay_us(uint32_t us)
{
    uint32_t deadline = MK_REG(SYS_CLO) + us;

    while ((int32_t)(MK_REG(SYS_CLO) - deadline) < 0)
        ;
}

void pcm_clock_init(void)
{
    MK_REG(CM_PCM_CTL) = CM_PASSWD;
    while (MK_REG(CM_PCM_CTL) & CM_CTL_BUSY)
        ;

    MK_REG(CM_PCM_DIV) = CM_PASSWD | CM_DIV_DIVI(PCM_CLK_DIVI) | CM_DIV_DIVF(0);

    MK_REG(CM_PCM_CTL) = CM_PASSWD | CM_CTL_MASH(0) | CM_CTL_SRC(PCM_CLK_SRC);
    MK_REG(CM_PCM_CTL) = CM_PASSWD | CM_CTL_MASH(0) | CM_CTL_SRC(PCM_CLK_SRC) | CM_CTL_EN;

    while (!(MK_REG(CM_PCM_CTL) & CM_CTL_BUSY))
        ;
}

void pcm_dmx_init(void)
{
    pcm_clock_init();

    gpio_pin_set_func(PCM_DOUT_PIN, GPIO_ALT0);

    MK_REG(I2S_CS) = I2S_CS_EN;
    pcm_delay_us(100);

    MK_REG(I2S_MODE) = I2S_MODE_FLEN(31) | I2S_MODE_FSLEN(1);
    MK_REG(I2S_TXC) = I2S_TXC_CH1EN | I2S_TXC_CH1WEX |
                      I2S_TXC_CH1WID(8) | I2S_TXC_CH1POS(0);

    MK_REG(I2S_CS) |= I2S_CS_TXCLR;
    pcm_delay_us(100); /* datasheet wants 2 PCM clocks; this is ~25 of them */

    MK_REG(I2S_CS) |= I2S_CS_TXERR; /* write 1 to clear a stale underrun */

    while (MK_REG(I2S_CS) & I2S_CS_TXD)
        MK_REG(I2S_FIFO) = 0xFFFFFFFFu;

    MK_REG(I2S_CS) |= I2S_CS_TXON;
}

__attribute__((aligned(32))) static dma_cb_t pcm_dma_cb;

void pcm_dma_start(uint8_t *buf, uint32_t len)
{
    gpio_pin_set_func(PCM_DOUT_PIN, GPIO_ALT0);
    MK_REG(I2S_CS) = I2S_CS_EN;
    pcm_delay_us(100);
    MK_REG(I2S_MODE) = I2S_MODE_FLEN(31) | I2S_MODE_FSLEN(1);
    MK_REG(I2S_TXC) = I2S_TXC_CH1EN | I2S_TXC_CH1WEX |
                      I2S_TXC_CH1WID(8) | I2S_TXC_CH1POS(0);

    MK_REG(I2S_CS) |= I2S_CS_TXCLR;
    pcm_delay_us(100); /* datasheet wants 2 PCM clocks; this is ~25 of them */

    MK_REG(I2S_CS) |= I2S_CS_TXERR; /* write 1 to clear a stale underrun */

    MK_REG(I2S_DREQ) = (0x10 << 24) | (0x30 << 8);

   
    // printk("Priming FIFO with mark before starting DMA\n");
    // while (MK_REG(I2S_CS) & I2S_CS_TXD)
    //     MK_REG(I2S_FIFO) = 0xFFFFFFFFu;

    // DMA_ENABLE
    MK_REG(I2S_CS) |= I2S_CS_DMAEN;
    printk("DMA enabled, starting transfer of %u bytes\n", len);

    pcm_dma_cb.ti = DMA_TI_PCM_TX;
    pcm_dma_cb.source_ad = ARM_TO_DMA_BUS(buf);
    pcm_dma_cb.dest_ad = 0x7E000000 | (I2S_FIFO & 0xFFFFFF);
    pcm_dma_cb.txfr_len = len;
    pcm_dma_cb.stride = 0;
    pcm_dma_cb.nextconbk = ARM_TO_DMA_BUS(&pcm_dma_cb);
    pcm_dma_cb.reserved1 = 0;
    pcm_dma_cb.reserved2 = 0;

    printk("DMA starting, %u bytes\n", len);

    MK_REG(I2S_CS) |= I2S_CS_TXON;
    MK_REG(DMA_CONBLK_AD_REG(0)) = ARM_TO_DMA_BUS(&pcm_dma_cb);
    MK_REG(DMA_CS_REG(0)) = DMA_CS_ACTIVE;


    // while ((MK_REG(DMA_CS_REG(0)) & DMA_CS_ACTIVE))
    // {
    //     pcm_delay_us(100);
    // }

    printk("DMA RUNNING, returning control\n");
}

void pcm_write_word(uint32_t w)
{
    while (!(MK_REG(I2S_CS) & I2S_CS_TXD))
        ;
    MK_REG(I2S_FIFO) = w;
}


uint32_t pcm_fill_mark(void)
{
    uint32_t n = 0;

    while (MK_REG(I2S_CS) & I2S_CS_TXD)
    {
        MK_REG(I2S_FIFO) = 0xFFFFFFFFu;
        n++;
    }
    return n;
}

int pcm_underran(void)
{
    if (!(MK_REG(I2S_CS) & I2S_CS_TXERR))
        return 0;

    MK_REG(I2S_CS) |= I2S_CS_TXERR;
    return 1;
}
