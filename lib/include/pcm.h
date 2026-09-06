#ifndef PCM_H
#define PCM_H
#include <stdint.h>

#define PCM_DOUT_PIN 21
#define PCM_CLK_PIN 18

#define MK_REG(addr) (*(volatile uint32_t *)(addr))

#define I2S_BASE 0x20203000
#define I2S_CS (I2S_BASE + 0x00)
#define I2S_FIFO (I2S_BASE + 0x04)
#define I2S_MODE (I2S_BASE + 0x08)
#define I2S_RXC (I2S_BASE + 0x0c)
#define I2S_TXC (I2S_BASE + 0x10)
#define I2S_DREQ (I2S_BASE + 0x14)

#define I2S_CS_SYNC (1 << 24)
#define I2S_CS_RXF (1 << 22)
#define I2S_CS_TXE (1 << 21)
#define I2S_CS_RXD (1 << 20)
#define I2S_CS_TXD (1 << 19)
#define I2S_CS_RXR (1 << 18)
#define I2S_CS_TXW (1 << 17)
#define I2S_CS_RXERR (1 << 16)
#define I2S_CS_TXERR (1 << 15)
#define I2S_CS_RXSYNC (1 << 14)
#define I2S_CS_TXSYNC (1 << 13)
#define I2S_CS_DMAEN (1 << 9)
#define I2S_CS_RXCLR (1 << 4)
#define I2S_CS_TXCLR (1 << 3)
#define I2S_CS_TXON (1 << 2)
#define I2S_CS_RXON (1 << 1)
#define I2S_CS_EN (1 << 0)

#define I2S_MODE_CLKM_SLAVE (1 << 23)
#define I2S_MODE_FSM_SLAVE (1 << 21)
#define I2S_MODE_FLEN(n) (((n) & 0x3ff) << 10)
#define I2S_MODE_FSLEN(n) (((n) & 0x3ff) << 0)

#define I2S_TXC_CH1WEX (1 << 31)
#define I2S_TXC_CH1EN (1 << 30)
#define I2S_TXC_CH1POS(n) (((n) & 0x3ff) << 20)
#define I2S_TXC_CH1WID(n) (((n) & 0xf) << 16)

#define CM_PCM_CTL 0x20101098
#define CM_PCM_DIV 0x2010109c
#define CM_PASSWD (0x5a << 24)
#define CM_CTL_MASH(n) (((n) & 3) << 9)
#define CM_CTL_BUSY (1 << 7)
#define CM_CTL_EN (1 << 4)
#define CM_CTL_SRC(n) ((n) & 0xf)
#define CM_DIV_DIVI(n) (((n) & 0xfff) << 12)
#define CM_DIV_DIVF(n) ((n) & 0xfff)

#define PCM_CLK_SRC 1
#define PCM_CLK_DIVI 77

void pcm_clock_init(void);
void pcm_dmx_init(void);
void pcm_dma_start(uint8_t *buf, uint32_t len);
void pcm_write_word(uint32_t w);
uint32_t pcm_fill_mark(void);
int pcm_underran(void);

#endif
