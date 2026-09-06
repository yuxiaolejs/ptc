#include "piusb.h"
#include "pictl.h"
#include "mailbox.h"
#include "cstr.h"
#include "mem.h"

// ============================================================================
// Context initialization
// ============================================================================

// Initialize a new USB context with defaults
void usb_context_init(usb_context_t *ctx)
{
    ctx->device.speed = 1;
    ctx->device.address = 0;
    ctx->device.max_packet_size = 8;

    ctx->hid.endpoint = 1;
    ctx->hid.ep_mps = 8;
    ctx->hid.interval = 10;
    ctx->hid.interrupt_toggle = PID_DATA0;

    ctx->aux.endpoint = 0; // 0 = not found
    ctx->aux.ep_mps = 0;
    ctx->aux.interval = 0;
    ctx->aux.interrupt_toggle = PID_DATA0;

    ctx->dma.data_buf_idx = 0;
    ctx->dma.int_buf_idx = 0;

    ctx->debug.last_hcint = 0;
    ctx->debug.hctsiz_before = 0;
    ctx->debug.hctsiz_rem_before = 0;
}

// ============================================================================
// USB Power Control
// ============================================================================

bool usb_power_on(usb_context_t *ctx)
{
    ctx->dma.mbox_buf[0] = 8 * 4;
    ctx->dma.mbox_buf[1] = 0;
    ctx->dma.mbox_buf[2] = RPI_FIRMWARE_SET_POWER_STATE;
    ctx->dma.mbox_buf[3] = 8;
    ctx->dma.mbox_buf[4] = 0;
    ctx->dma.mbox_buf[5] = USB_DEVICE_ID;
    ctx->dma.mbox_buf[6] = POWER_STATE_ON | POWER_STATE_WAIT;
    ctx->dma.mbox_buf[7] = 0;

    dmb();
    uint32_t addr = ((uint32_t)ctx->dma.mbox_buf & ~0xF) | MAIL_TAGS;
    while (MAILBOX->status & MAIL_FULL)
        ;
    MAILBOX->write = addr;
    while (MAILBOX->status & MAIL_EMPTY)
        ;
    (void)MAILBOX->read;

    if (ctx->dma.mbox_buf[1] != 0x80000000)
    {
        if (PIUSB_DEBUG)
            printk("USB power mailbox failed: %x\n", ctx->dma.mbox_buf[1]);
        return false;
    }
    if (PIUSB_DEBUG)
        printk("USB power enabled\n");
    return true;
}

// ============================================================================
// DWC2 Core initialization
// ============================================================================

void usb_core_reset(void)
{
    // Wait for AHB idle
    while (!(readl(GRSTCTL) & GRSTCTL_AHBIDLE))
        ;
    // Soft reset
    writel(readl(GRSTCTL) | GRSTCTL_CSRST, GRSTCTL);
    while (readl(GRSTCTL) & GRSTCTL_CSRST)
        ;
    udelay(100000);
}

void usb_flush_tx_fifo(uint32_t fifo_num)
{
    writel((fifo_num << 6) | GRSTCTL_TXFFLSH, GRSTCTL);
    while (readl(GRSTCTL) & GRSTCTL_TXFFLSH)
        ;
    udelay(1000);
}

void usb_flush_rx_fifo(void)
{
    writel(GRSTCTL_RXFFLSH, GRSTCTL);
    while (readl(GRSTCTL) & GRSTCTL_RXFFLSH)
        ;
    udelay(1000);
}

void usb_force_host_mode(void)
{
    if (PIUSB_DEBUG)
        printk("Forcing host mode...\n");

    // Stop any activity first
    writel(0, GAHBCFG);
    udelay(100000);

    // Clear OTG control
    writel(0, GOTGCTL);
    udelay(100000);

    // Mask all interrupts
    writel(0, GINTMSK);
    udelay(10000);

    // Configure PHY - UTMI+, clear ULPI bits
    uint32_t gusbcfg = readl(GUSBCFG);
    gusbcfg &= ~(1u << 4);  // ULPI_UTMI_SEL = 0 (UTMI+)
    gusbcfg &= ~(1u << 3);  // PHYIF = 0 (8-bit UTMI)
    gusbcfg &= ~(1u << 17); // Clear ULPI_FSLS
    gusbcfg &= ~(1u << 19); // Clear ULPI_CLK_SUS_M
    gusbcfg &= ~(1u << 20); // Clear ULPI_EXT_VBUS_DRV
    gusbcfg &= ~(1u << 22); // Clear TERM_SEL_DL_PULSE
    gusbcfg &= ~(1u << 8);  // Clear SRP capable
    gusbcfg &= ~(1u << 9);  // Clear HNP capable
    writel(gusbcfg, GUSBCFG);
    udelay(50000);

    // Force host mode
    gusbcfg = readl(GUSBCFG);
    gusbcfg |= (1u << 29);  // ForceHostMode
    gusbcfg &= ~(1u << 30); // Clear ForceDeviceMode
    writel(gusbcfg, GUSBCFG);

    udelay(200000); // DWC2 needs 25ms+ to switch modes

    // Now reset the core
    if (PIUSB_DEBUG)
        printk("  Resetting core...\n");
    usb_core_reset();

    // Re-apply host mode after reset (reset clears it)
    gusbcfg = readl(GUSBCFG);
    gusbcfg &= ~(1u << 4);  // UTMI+
    gusbcfg &= ~(1u << 3);  // 8-bit
    gusbcfg &= ~(1u << 8);  // No SRP
    gusbcfg &= ~(1u << 9);  // No HNP
    gusbcfg |= (1u << 29);  // ForceHostMode
    gusbcfg &= ~(1u << 30); // Clear ForceDeviceMode
    writel(gusbcfg, GUSBCFG);

    udelay(500000); // Wait for mode switch

    if (PIUSB_DEBUG)
        printk("  GUSBCFG=%x GINTSTS=%x\n", readl(GUSBCFG), readl(GINTSTS));
    if (PIUSB_DEBUG)
        printk("  CURMODE bit = %d (expect 1 for host)\n", readl(GINTSTS) & 1);
}

void usb_host_init(void)
{
    if (PIUSB_DEBUG)
        printk("Initializing host controller...\n");

    // Stop PHY clock
    writel(0, ARM_USB_POWER);
    udelay(10000);

    // Configure HCFG - 48MHz clock for FS/LS
    writel(1u << 0, HCFG);
    writel(48000, HFIR);

    // Configure FIFOs
    writel(1024, GRXFSIZ);
    writel((1024 << 16) | 1024, GNPTXFSIZ);
    writel((1024 << 16) | 2048, HPTXFSIZ);

    // Flush FIFOs
    usb_flush_tx_fifo(0x10);
    usb_flush_rx_fifo();

    // Enable DMA mode with proper burst length
    uint32_t ahbcfg = GAHBCFG_GLBLINTRMSK | GAHBCFG_DMAEN | GAHBCFG_HBSTLEN_INCR4;
    writel(ahbcfg, GAHBCFG);

    // Enable interrupts for channels 0, 1, 2, 3
    writel((1u << 0) | (1u << 1) | (1u << 2) | (1u << 3), HAINTMSK);

    // Enable host channel interrupt in global mask
    uint32_t gintmsk = (1u << 25) | (1u << 3) | (1u << 24); // HCINT, SOF, PORT
    writel(gintmsk, GINTMSK);

    udelay(50000);
    if (PIUSB_DEBUG)
        printk("Host controller initialized\n");
}

void usb_port_power_on(void)
{
    // Set port power bit (PPWR = bit 12)
    // HPRT is sensitive - don't RMW the W1C bits
    writel(1u << 12, HPRT);
    udelay(200000);
}

void usb_port_reset(usb_context_t *ctx)
{
    if (PIUSB_DEBUG)
        printk("Port reset...\n");

    uint32_t hprt = readl(HPRT);
    // Clear W1C bits (connected change, enable change, overcurrent change)
    hprt &= ~((1u << 1) | (1u << 2) | (1u << 3) | (1u << 5));
    // Set reset bit
    hprt |= (1u << 8);
    writel(hprt, HPRT);

    udelay(60000); // USB spec: 10-20ms, we use 60ms for safety

    // Clear reset
    hprt = readl(HPRT);
    hprt &= ~((1u << 1) | (1u << 2) | (1u << 3) | (1u << 5) | (1u << 8));
    writel(hprt, HPRT);

    udelay(50000);

    hprt = readl(HPRT);
    ctx->device.speed = (hprt >> 17) & 3;
    if (PIUSB_DEBUG)
        printk("Port reset complete, speed=%d (0=HS,1=FS,2=LS), enabled=%d\n",
               ctx->device.speed, (hprt >> 2) & 1);
}

// USB channel control

void usb_transfer_channel_halt(int ch)
{
    uint32_t hcchar = readl(HCCHAR(ch));

    // 1) Force halt: CHDIS=1 while CHENA=1 (always)
    writel(hcchar | HCCHAR_CHDIS | HCCHAR_CHENA, HCCHAR(ch));

    // 2) Wait for CHHLTD (reload-complete signal)
    int timeout = 100000;
    while (!(readl(HCINT(ch)) & HCINT_CHHLTD) && timeout--)
    {
        /* spin */
    }

    if (timeout <= 0)
    {
        if (PIUSB_DEBUG)
            printk("ERROR: CHHLTD timeout\n");
    }

    // 3) Wait for CHENA to clear
    timeout = 100000;
    while ((readl(HCCHAR(ch)) & HCCHAR_CHENA) && timeout--)
    {
        /* spin */
    }

    // 4) ACK the halt (THIS is when reload becomes legal)
    writel(HCINT_CHHLTD, HCINT(ch));

    dsb();
}

// Wait for a channel to complete or halt
// Returns: 0=success, -1=error, -2=stall, -3=NAK, -4=timeout
int usb_transfer_channel_wait(usb_context_t *ctx, int ch)
{
    uint32_t timeout = 5000000;

    while (timeout--)
    {
        uint32_t hcint = readl(HCINT(ch));

        // Check for completion with halt
        if (hcint & HCINT_CHHLTD)
        {
            ctx->debug.last_hcint = hcint; // Save for debug
            if (hcint & HCINT_XFERCOMP)
            {
                writel(hcint, HCINT(ch));
                return 0;
            }
            if (hcint & HCINT_STALL)
            {
                writel(hcint, HCINT(ch));
                return -2;
            }
            if (hcint & HCINT_NAK)
            {
                writel(hcint, HCINT(ch));
                return -3;
            }
            if (hcint & HCINT_FRMOVRUN)
            {
                // Frame overrun - not an error, just retry on next frame
                writel(hcint, HCINT(ch));
                return -5; // Special code for frame overrun
            }
            if (hcint & HCINT_DATATGLERR)
            {
                if (PIUSB_DEBUG)
                    printk("CH%d DATATGL ERROR: HCINT=%x\n", ch, hcint);
                writel(hcint, HCINT(ch));
                return -6; // Toggle error - serious!
            }
            if (hcint & (HCINT_XACTERR | HCINT_AHBERR | HCINT_BBLERR))
            {
                if (PIUSB_DEBUG)
                    printk("CH%d error: HCINT=%x\n", ch, hcint);
                writel(hcint, HCINT(ch));
                return -1;
            }
            if (hcint & HCINT_ACK)
            {
                // ACK without XFERCOMP - sometimes happens, treat as success
                writel(hcint, HCINT(ch));
                return 0;
            }
            // Halted for unknown reason
            writel(hcint, HCINT(ch));
            return -3; // Treat as NAK
        }

        // Check for transfer complete (should come with halt but handle it)
        if (hcint & HCINT_XFERCOMP)
        {
            ctx->debug.last_hcint = hcint;
            writel(hcint, HCINT(ch));
            return 0;
        }
    }

    ctx->debug.last_hcint = readl(HCINT(ch));
    if (PIUSB_DEBUG)
        printk("CH%d timeout, HCINT=%x HCCHAR=%x HCTSIZ=%x\n",
               ch, ctx->debug.last_hcint, readl(HCCHAR(ch)), readl(HCTSIZ(ch)));
    usb_transfer_channel_halt(ch);
    return -4;
}

// Start a transfer on a channel
// The DMA buffer must be prepared before calling this
void usb_transfer_channel_start(usb_context_t *ctx, int ch, uint8_t devaddr, uint8_t ep, uint8_t eptype,
                                       bool dir_in, uint16_t mps, uint8_t pid,
                                       uint32_t xfersize, uint32_t pktcnt, uint32_t dma_addr)
{
    uint32_t hcchar;

    /* ACK previous halt if pending */
    if (readl(HCINT(ch)) & HCINT_CHHLTD)
        writel(HCINT_CHHLTD, HCINT(ch));

    /* Force halt (DWC2 halt dance) */
    hcchar = readl(HCCHAR(ch));
    writel(hcchar | HCCHAR_CHDIS | HCCHAR_CHENA, HCCHAR(ch));

    int timeout = 100000;
    while (!(readl(HCINT(ch)) & HCINT_CHHLTD) && timeout--)
    {
    }

    writel(HCINT_CHHLTD, HCINT(ch));

    timeout = 100000;
    while (readl(HCCHAR(ch)) & HCCHAR_CHENA && timeout--)
    {
    }

    dsb();

    /* Program transfer */
    writel(0xFFFFFFFF, HCINT(ch));
    writel(0x7FF, HCINTMSK(ch));
    writel(0, HCSPLT(ch));

    writel((pid << HCTSIZ_PID_SHIFT) |
               (pktcnt << HCTSIZ_PKTCNT_SHIFT) |
               (xfersize & HCTSIZ_XFERSIZE_MASK),
           HCTSIZ(ch));

    writel(dma_addr, HCDMA(ch));
    dsb();

    /* Enable channel */
    hcchar =
        (devaddr << HCCHAR_DEVADDR_SHIFT) |
        (ep << HCCHAR_EPNUM_SHIFT) |
        (eptype << HCCHAR_EPTYPE_SHIFT) |
        (1 << HCCHAR_MC_SHIFT) |
        (mps & HCCHAR_MPS_MASK);

    if (dir_in)
        hcchar |= HCCHAR_EPDIR_IN;
    if (ctx->device.speed == 2)
        hcchar |= HCCHAR_LSDEV;
    if (readl(HFNUM) & 1)
        hcchar |= HCCHAR_ODDFRM;

    writel(hcchar | HCCHAR_CHENA, HCCHAR(ch));
    dsb();
}

// ============================================================================
// Control Transfer Implementation (Proper state machine)
// ============================================================================

// Execute SETUP stage on CH_CONTROL_OUT
// Returns 0 on success
int usb_setup_stage_dma_xfer(usb_context_t *ctx, uint8_t devaddr, struct usb_setup *setup)
{
    // Copy setup packet to DMA buffer
    uint8_t *src = (uint8_t *)setup;
    for (int i = 0; i < 8; i++)
    {
        ctx->dma.setup_buf[i] = src[i];
    }

    // Clean cache so DMA sees the data

    uint32_t dma_addr = BUS_ADDRESS((uintptr_t)ctx->dma.setup_buf);

    // SETUP stage: PID=SETUP(3), OUT direction, 8 bytes, 1 packet
    usb_transfer_channel_start(ctx, CH_CONTROL_OUT, devaddr, 0, EP_TYPE_CONTROL,
                               false, ctx->device.max_packet_size, PID_SETUP, 8, 1, dma_addr);

    return usb_transfer_channel_wait(ctx, CH_CONTROL_OUT);
}

// Execute DATA IN stage on CH_CONTROL_IN
// Packet-by-packet approach with rotating buffers for cache coherency
int usb_setup_stage_dma_data_in(usb_context_t *ctx, uint8_t devaddr, void *buf, uint32_t len)
{
    if (len == 0)
        return 0;

    static int dbg_data_stage = 0;
    dbg_data_stage++;
    int dbg_this = (dbg_data_stage <= 3); // Debug first 3 DATA stages

    uint8_t *dst = (uint8_t *)buf;
    uint32_t offset = 0;
    uint8_t pid = PID_DATA1; // First DATA after SETUP is DATA1
    int pkt_num = 0;

    if (dbg_this)
        if (PIUSB_DEBUG)
            printk("  DATA_IN: len=%d mps=%d\n", len, ctx->device.max_packet_size);

    while (offset < len)
    {
        uint32_t chunk = len - offset;
        if (chunk > ctx->device.max_packet_size)
            chunk = ctx->device.max_packet_size;
        pkt_num++;

        // Use rotating buffer to avoid cache issues
        uint8_t *pkt_buf = ctx->dma.data_buf[ctx->dma.data_buf_idx];
        ctx->dma.data_buf_idx = (ctx->dma.data_buf_idx + 1) & 15;

        // Clear buffer and flush from cache (clean + invalidate)
        for (int i = 0; i < 64; i++)
            pkt_buf[i] = 0xCC;
        dsb();
        dmb();

        uint32_t dma_addr = BUS_ADDRESS((uintptr_t)pkt_buf);

        if (dbg_this)
            if (PIUSB_DEBUG)
                printk("    PKT[%d]: buf=%x dma=%x pid=%d\n",
                       pkt_num, (uint32_t)pkt_buf, dma_addr, pid);

        // Start single packet IN transfer
        usb_transfer_channel_start(ctx, CH_CONTROL_IN, devaddr, 0, EP_TYPE_CONTROL,
                                   true, ctx->device.max_packet_size, pid, chunk, 1, dma_addr);

        int ret = usb_transfer_channel_wait(ctx, CH_CONTROL_IN);

        // Memory barrier after DMA completes
        dsb();
        dmb();

        // Read HCINT before it gets cleared by next operation
        uint32_t hctsiz_after = readl(HCTSIZ(CH_CONTROL_IN));

        if (ret != 0)
        {
            if (dbg_this)
                if (PIUSB_DEBUG)
                    printk("    -> FAILED ret=%d\n", ret);
            return ret;
        }

        // Invalidate cache to see DMA data (do this AFTER transfer completes)
        dsb();
        dmb();

        // Check how much was actually received
        uint32_t remaining = hctsiz_after & HCTSIZ_XFERSIZE_MASK;
        uint32_t received = chunk - remaining;

        // Read bytes directly with volatile to bypass cache
        volatile uint8_t *vbuf = (volatile uint8_t *)pkt_buf;
        if (dbg_this)
            if (PIUSB_DEBUG)
                printk("    -> recv=%d data=%x %x %x %x %x %x %x %x\n",
                       received,
                       vbuf[0], vbuf[1], vbuf[2], vbuf[3],
                       vbuf[4], vbuf[5], vbuf[6], vbuf[7]);

        // Copy to output buffer using volatile reads
        for (uint32_t i = 0; i < received; i++)
        {
            dst[offset + i] = vbuf[i];
        }

        offset += received;

        // Toggle PID for next packet
        pid = (pid == PID_DATA0) ? PID_DATA1 : PID_DATA0;

        // Short packet means end of data
        if (received < ctx->device.max_packet_size)
        {
            break;
        }
    }

    if (dbg_this)
        if (PIUSB_DEBUG)
            printk("  DATA_IN complete: total=%d\n", offset);
    return 0;
}

// Execute STATUS OUT stage (after DATA IN) on CH_CONTROL_OUT
int do_status_out_stage(usb_context_t *ctx, uint8_t devaddr)
{
    // Clean cache for empty buffer

    uint32_t dma_addr = BUS_ADDRESS((uintptr_t)ctx->dma.status_buf);

    // STATUS OUT: PID=DATA1, OUT direction, 0 bytes (ZLP), 1 packet
    usb_transfer_channel_start(ctx, CH_CONTROL_OUT, devaddr, 0, EP_TYPE_CONTROL,
                               false, ctx->device.max_packet_size, PID_DATA1, 0, 1, dma_addr);

    return usb_transfer_channel_wait(ctx, CH_CONTROL_OUT);
}

// Execute STATUS IN stage (after SETUP with no data or DATA OUT) on CH_CONTROL_IN
int usb_setup_stage_dma_status_in(usb_context_t *ctx, uint8_t devaddr)
{
    // Invalidate cache for receive buffer

    uint32_t dma_addr = BUS_ADDRESS((uintptr_t)ctx->dma.status_buf);

    // STATUS IN: PID=DATA1, IN direction, 0 bytes (ZLP), 1 packet
    usb_transfer_channel_start(ctx, CH_CONTROL_IN, devaddr, 0, EP_TYPE_CONTROL,
                               true, ctx->device.max_packet_size, PID_DATA1, 0, 1, dma_addr);

    return usb_transfer_channel_wait(ctx, CH_CONTROL_IN);
}

// Full control transfer: device -> host (GET requests)
int usb_control_in(usb_context_t *ctx, uint8_t devaddr, struct usb_setup *setup, void *data, uint16_t len)
{
    int ret;
    int retries;

    // SETUP stage with retries
    for (retries = 0; retries < 100; retries++)
    {
        ret = usb_setup_stage_dma_xfer(ctx, devaddr, setup);
        if (ret == 0)
            break;
        if (ret != -3)
            return ret; // Not NAK, real error
        udelay(1000);
    }
    if (ret != 0)
    {
        if (PIUSB_DEBUG)
            printk("  SETUP failed after %d retries\n", retries);
        return ret;
    }

    // Small delay for device to process SETUP
    udelay(2000);

    // DATA IN stage with retries
    if (len > 0 && data)
    {
        for (retries = 0; retries < 500; retries++)
        {
            ret = usb_setup_stage_dma_data_in(ctx, devaddr, data, len);
            if (ret == 0)
                break;
            if (ret != -3)
                return ret;
            udelay(1000);
        }
        if (ret != 0)
        {
            if (PIUSB_DEBUG)
                printk("  DATA IN failed after %d retries: %d\n", retries, ret);
            return ret;
        }
    }

    // Small delay before STATUS
    udelay(2000);

    // STATUS OUT stage with retries
    for (retries = 0; retries < 100; retries++)
    {
        ret = do_status_out_stage(ctx, devaddr);
        if (ret == 0)
            break;
        if (ret != -3)
            return ret;
        udelay(1000);
    }
    if (ret != 0)
    {
        if (PIUSB_DEBUG)
            printk("  STATUS OUT failed after %d retries: %d\n", retries, ret);
        return ret;
    }

    return 0;
}

// Execute DATA OUT stage on CH_CONTROL_OUT
int usb_dma_data_out(usb_context_t *ctx, uint8_t devaddr, void *buf, uint32_t len)
{
    if (len == 0)
        return 0;

    // Use a rotating buffer for DMA
    uint8_t *pkt_buf = ctx->dma.data_buf[ctx->dma.data_buf_idx];
    ctx->dma.data_buf_idx = (ctx->dma.data_buf_idx + 1) & 15;

    // Copy data to DMA buffer
    uint8_t *src = (uint8_t *)buf;
    for (uint32_t i = 0; i < len; i++)
    {
        pkt_buf[i] = src[i];
    }
    dsb();
    dmb();

    uint32_t dma_addr = BUS_ADDRESS((uintptr_t)pkt_buf);

    // DATA OUT uses DATA1 after SETUP
    usb_transfer_channel_start(ctx, CH_CONTROL_OUT, devaddr, 0, EP_TYPE_CONTROL,
                               false, ctx->device.max_packet_size, PID_DATA1, len, 1, dma_addr);

    int ret = usb_transfer_channel_wait(ctx, CH_CONTROL_OUT);
    return ret;
}

// Full control transfer: host -> device with data
int usb_control_out_data(usb_context_t *ctx, uint8_t devaddr, struct usb_setup *setup, void *data, uint16_t len)
{
    int ret;
    int retries;

    // SETUP stage with retries
    for (retries = 0; retries < 100; retries++)
    {
        ret = usb_setup_stage_dma_xfer(ctx, devaddr, setup);
        if (ret == 0)
            break;
        if (ret != -3)
            return ret;
        udelay(1000);
    }
    if (ret != 0)
    {
        if (PIUSB_DEBUG)
            printk("SETUP failed after %d retries\n", retries);
        return ret;
    }

    udelay(2000);

    // DATA OUT stage
    for (retries = 0; retries < 100; retries++)
    {
        ret = usb_dma_data_out(ctx, devaddr, data, len);
        if (ret == 0)
            break;
        if (ret != -3)
            return ret;
        udelay(1000);
    }
    if (ret != 0)
    {
        if (PIUSB_DEBUG)
            printk("DATA OUT failed\n");
        return ret;
    }

    udelay(2000);

    // STATUS IN stage
    for (retries = 0; retries < 100; retries++)
    {
        ret = usb_setup_stage_dma_status_in(ctx, devaddr);
        if (ret == 0)
            break;
        if (ret != -3 && ret != -2)
            return ret;
        udelay(2000);
    }

    return 0;
}

// Full control transfer: host -> device (SET requests with no data)
int usb_control_out_nodata(usb_context_t *ctx, uint8_t devaddr, struct usb_setup *setup)
{
    int ret;
    int retries;

    // SETUP stage with retries
    for (retries = 0; retries < 100; retries++)
    {
        ret = usb_setup_stage_dma_xfer(ctx, devaddr, setup);
        if (ret == 0)
            break;
        if (ret != -3)
            return ret;
        udelay(1000);
    }
    if (ret != 0)
    {
        if (PIUSB_DEBUG)
            printk("SETUP failed after %d retries\n", retries);
        return ret;
    }

    // Delay for device to process
    udelay(5000);

    // STATUS IN stage with retries
    for (retries = 0; retries < 100; retries++)
    {
        ret = usb_setup_stage_dma_status_in(ctx, devaddr);
        if (ret == 0)
        {
            if (PIUSB_DEBUG)
                printk("  STATUS IN OK\n");
            break;
        }
        if (ret != -3 && ret != -2)
        {
            if (PIUSB_DEBUG)
                printk("  STATUS IN error: %d\n", ret);
            return ret;
        }
        udelay(2000);
    }

    // For some requests (SET_ADDRESS, SET_CONFIGURATION), failure to get
    // STATUS is not fatal - device may have already switched state
    if (ret != 0)
    {
        if (PIUSB_DEBUG)
            printk("  STATUS IN failed after retries: %d (may be OK)\n", ret);
    }

    return 0;
}

// ============================================================================
// USB Standard Requests
// ============================================================================

int usb_get_device_descriptor(usb_context_t *ctx, uint8_t devaddr, struct usb_device_descriptor *desc, uint8_t len)
{
    struct usb_setup setup = {
        .bmRequestType = 0x80,
        .bRequest = 6,    // GET_DESCRIPTOR
        .wValue = 0x0100, // Device descriptor
        .wIndex = 0,
        .wLength = len};
    return usb_control_in(ctx, devaddr, &setup, desc, len);
}

int usb_get_config_descriptor(usb_context_t *ctx, uint8_t devaddr, uint8_t *buf, uint16_t len)
{
    struct usb_setup setup = {
        .bmRequestType = 0x80,
        .bRequest = 6,    // GET_DESCRIPTOR
        .wValue = 0x0200, // Configuration descriptor
        .wIndex = 0,
        .wLength = len};
    return usb_control_in(ctx, devaddr, &setup, buf, len);
}

int usb_set_address(usb_context_t *ctx, uint8_t new_addr)
{
    struct usb_setup setup = {
        .bmRequestType = 0x00,
        .bRequest = 5, // SET_ADDRESS
        .wValue = new_addr,
        .wIndex = 0,
        .wLength = 0};

    if (PIUSB_DEBUG)
        printk("SET_ADDRESS(%d)...\n", new_addr);

    int ret;
    int retries;

    // SETUP stage at address 0
    for (retries = 0; retries < 100; retries++)
    {
        ret = usb_setup_stage_dma_xfer(ctx, 0, &setup);
        if (ret == 0)
            break;
        if (ret != -3)
            return ret;
        udelay(1000);
    }
    if (ret != 0)
    {
        if (PIUSB_DEBUG)
            printk("SET_ADDRESS SETUP failed\n");
        return ret;
    }
    if (PIUSB_DEBUG)
        printk("SET_ADDRESS SETUP OK\n");

    // Delay - USB spec says device must complete STATUS before changing address
    udelay(5000);

    // Try STATUS IN at address 0 first
    for (retries = 0; retries < 20; retries++)
    {
        ret = usb_setup_stage_dma_status_in(ctx, 0);
        if (ret == 0)
        {
            if (PIUSB_DEBUG)
                printk("STATUS IN OK at addr 0\n");
            // Wait SetAddressDelay (2ms minimum per USB spec)
            udelay(5000);
            return 0;
        }
        if (ret == -2)
            break; // STALL - device may have switched
        if (ret != -3 && ret != -1)
            break;
        udelay(1000);
    }

    // Device might have already switched address - try at new address
    if (PIUSB_DEBUG)
        printk("STATUS IN at addr 0 failed (%d), trying addr %d...\n", ret, new_addr);
    udelay(10000);

    for (retries = 0; retries < 20; retries++)
    {
        ret = usb_setup_stage_dma_status_in(ctx, new_addr);
        if (ret == 0)
        {
            if (PIUSB_DEBUG)
                printk("STATUS IN OK at new addr\n");
            return 0;
        }
        if (ret != -3 && ret != -1)
            break;
        udelay(1000);
    }

    // Assume it worked - some devices are quirky
    if (PIUSB_DEBUG)
        printk("STATUS IN failed everywhere, assuming address changed\n");
    udelay(5000);
    return 0;
}

int usb_set_configuration(usb_context_t *ctx, uint8_t devaddr, uint8_t config)
{
    struct usb_setup setup = {
        .bmRequestType = 0x00,
        .bRequest = 9, // SET_CONFIGURATION
        .wValue = config,
        .wIndex = 0,
        .wLength = 0};

    if (PIUSB_DEBUG)
        printk("SET_CONFIGURATION(%d)...\n", config);
    int ret = usb_control_out_nodata(ctx, devaddr, &setup);
    if (PIUSB_DEBUG)
        printk("  SET_CONFIGURATION result: %d\n", ret);
    return ret;
}

int usb_hid_set_idle(usb_context_t *ctx, uint8_t devaddr, uint8_t interface)
{
    struct usb_setup setup = {
        .bmRequestType = 0x21, // Class, Interface
        .bRequest = 0x0A,      // SET_IDLE
        .wValue = 0,           // Duration=indefinite, Report=0
        .wIndex = interface,
        .wLength = 0};

    if (PIUSB_DEBUG)
        printk("SET_IDLE...\n");
    (void)usb_control_out_nodata(ctx, devaddr, &setup);
    // SET_IDLE failure is not fatal
    return 0;
}

int usb_hid_set_protocol(usb_context_t *ctx, uint8_t devaddr, uint8_t interface, uint8_t protocol)
{
    struct usb_setup setup = {
        .bmRequestType = 0x21,
        .bRequest = 0x0B,   // SET_PROTOCOL
        .wValue = protocol, // 0=boot, 1=report
        .wIndex = interface,
        .wLength = 0};

    if (PIUSB_DEBUG)
        printk("SET_PROTOCOL(%d)...\n", protocol);
    (void)usb_control_out_nodata(ctx, devaddr, &setup);
    return 0;
}

// SET_REPORT - used to set LED state
int usb_hid_set_report(usb_context_t *ctx, uint8_t devaddr, uint8_t interface, uint8_t report_type,
                              uint8_t report_id, void *data, uint16_t len)
{
    struct usb_setup setup = {
        .bmRequestType = 0x21, // Class, Interface, Out
        .bRequest = 0x09,      // SET_REPORT
        .wValue = (report_type << 8) | report_id,
        .wIndex = interface,
        .wLength = len};

    if (PIUSB_DEBUG)
        printk("SET_REPORT(type=%d, id=%d, len=%d)...\n", report_type, report_id, len);
    int ret = usb_control_out_data(ctx, devaddr, &setup, data, len);
    if (ret == 0)
    {
        if (PIUSB_DEBUG)
            printk("  SET_REPORT OK\n");
    }
    return ret;
}

// ============================================================================
// Interrupt Transfer (for HID keyboard polling)
// ============================================================================

// Invalidate DMA buffer cache lines so CPU sees fresh DMA data
static void dma_buf_invalidate(void *buf, uint32_t len)
{
    uint32_t addr = (uint32_t)buf & ~31u;
    uint32_t end = (uint32_t)buf + len;
    for (; addr < end; addr += 32)
        asm volatile("mcr p15, 0, %0, c7, c14, 1" :: "r"(addr)); // DCCIMVAC
    dsb();
}

// Poll interrupt endpoint on a given channel
// ch: DWC2 channel, buf_idx: index into ctx->dma.int_buf for DMA
// ep_state: endpoint state (endpoint number, MPS, toggle)
// Returns: 0=got data, -3=NAK (no data), other=error
int usb_interrupt_poll_ch(usb_context_t *ctx, int ch, int buf_idx,
                          uint8_t devaddr, usb_hid_state_t *ep_state,
                          void *buf, uint16_t len)
{
    uint32_t hcint = readl(HCINT(ch));

    if (!(hcint & HCINT_CHHLTD))
        return -3;

    if (hcint & (HCINT_STALL | HCINT_XACTERR | HCINT_AHBERR))
        return -1;

    if (hcint & HCINT_DATATGLERR)
    {
        writel(hcint, HCINT(ch));
        ep_state->interrupt_toggle ^= 2;
        dma_buf_invalidate(ctx->dma.int_buf[buf_idx], 64);
        usb_transfer_channel_start(
            ctx, ch, devaddr, ep_state->endpoint, EP_TYPE_INTERRUPT,
            true, ep_state->ep_mps, ep_state->interrupt_toggle,
            len, 1, BUS_ADDRESS((uintptr_t)ctx->dma.int_buf[buf_idx]));
        return -6;
    }

    if (hcint & HCINT_XFERCOMP)
    {
        // Invalidate cache lines so CPU reads fresh DMA data from memory
        dma_buf_invalidate(ctx->dma.int_buf[buf_idx], 64);

        uint8_t *src = (uint8_t *)ctx->dma.int_buf[buf_idx];
        uint8_t *dst = buf;

        for (int i = 0; i < len; i++)
            dst[i] = src[i];

        ep_state->interrupt_toggle ^= 2;

        dma_buf_invalidate(ctx->dma.int_buf[buf_idx], 64);

        usb_transfer_channel_start(
            ctx, ch, devaddr, ep_state->endpoint, EP_TYPE_INTERRUPT,
            true, ep_state->ep_mps, ep_state->interrupt_toggle,
            len, 1, BUS_ADDRESS((uintptr_t)ctx->dma.int_buf[buf_idx]));

        return 0;
    }

    /* Halt without data → re-arm */
    dma_buf_invalidate(ctx->dma.int_buf[buf_idx], 64);

    usb_transfer_channel_start(
        ctx, ch, devaddr, ep_state->endpoint, EP_TYPE_INTERRUPT,
        true, ep_state->ep_mps, ep_state->interrupt_toggle,
        len, 1, BUS_ADDRESS((uintptr_t)ctx->dma.int_buf[buf_idx]));

    return -3;
}

void usb_interrupt_init_ch(usb_context_t *ctx, int ch, int buf_idx,
                           uint8_t devaddr, usb_hid_state_t *ep_state)
{
    uint32_t dma_addr = BUS_ADDRESS((uintptr_t)ctx->dma.int_buf[buf_idx]);

    dsb();
    dmb();

    usb_transfer_channel_start(
        ctx, ch, devaddr, ep_state->endpoint, EP_TYPE_INTERRUPT,
        true, ep_state->ep_mps, ep_state->interrupt_toggle,
        ep_state->ep_mps, 1, dma_addr);
}

// ============================================================================
// Full HID initialization
// ============================================================================

int usb_hid_full_init(usb_context_t *ctx)
{
    if (PIUSB_DEBUG)
        printk("\n=== USB Keyboard Driver (Fixed) ===\n");

    // Initialize context with defaults
    usb_context_init(ctx);

    // Power on USB controller
    if (!usb_power_on(ctx))
    {
        if (PIUSB_DEBUG)
            printk("Failed to power on USB!\n");
        while (1)
            ;
    }
    udelay(100000);

    // Initialize host mode
    usb_force_host_mode();

    // Note: CURMODE bit may not reflect host mode immediately on BCM2835
    // We'll verify host mode by checking if we can access host registers
    if (PIUSB_DEBUG)
        printk("CURMODE = %s (continuing anyway)\n",
               (readl(GINTSTS) & 1) ? "HOST" : "DEVICE");

    // Initialize host controller
    usb_host_init();

    // Power on port
    usb_port_power_on();

    // Wait for device connection
    if (PIUSB_DEBUG)
        printk("Waiting for device...\n");
    while (!(readl(HPRT) & 1))
    {
        udelay(100000);
    }
    if (PIUSB_DEBUG)
        printk("Device connected!\n");
    udelay(200000); // Debounce

    // Reset port
    usb_port_reset(ctx);

    // Default MPS is 8 for control EP0 before we know the real value
    ctx->device.max_packet_size = 8;

    // ========== Device Enumeration ==========
    if (PIUSB_DEBUG)
        printk("\n--- Enumeration ---\n");

    // 1. Set address (to address 1)
    if (usb_set_address(ctx, 1) != 0)
    {
        if (PIUSB_DEBUG)
            printk("SET_ADDRESS failed!\n");
        while (1)
            ;
    }
    ctx->device.address = 1;
    if (PIUSB_DEBUG)
        printk("Address set to %d\n", ctx->device.address);

    // 2. Get device descriptor to learn actual MPS
    static struct usb_device_descriptor dev_desc __attribute__((aligned(32)));
    // Zero the descriptor first
    for (int i = 0; i < sizeof(dev_desc); i++)
        ((uint8_t *)&dev_desc)[i] = 0;

    int ret = usb_get_device_descriptor(ctx, ctx->device.address, &dev_desc, 8);
    if (PIUSB_DEBUG)
        printk("GET_DESCRIPTOR(8) result: %d\n", ret);
    if (PIUSB_DEBUG)
        printk("  Raw bytes: %x %x %x %x %x %x %x %x\n",
               ((uint8_t *)&dev_desc)[0], ((uint8_t *)&dev_desc)[1],
               ((uint8_t *)&dev_desc)[2], ((uint8_t *)&dev_desc)[3],
               ((uint8_t *)&dev_desc)[4], ((uint8_t *)&dev_desc)[5],
               ((uint8_t *)&dev_desc)[6], ((uint8_t *)&dev_desc)[7]);
    if (ret == 0)
    {
        ctx->device.max_packet_size = dev_desc.bMaxPacketSize0;
        if (PIUSB_DEBUG)
            printk("Device MPS: %d (bLength=%d, bDescType=%d)\n",
                   ctx->device.max_packet_size, dev_desc.bLength, dev_desc.bDescriptorType);
    }

    // 3. Get full device descriptor
    for (int i = 0; i < sizeof(dev_desc); i++)
        ((uint8_t *)&dev_desc)[i] = 0;
    ret = usb_get_device_descriptor(ctx, ctx->device.address, &dev_desc, 18);
    if (PIUSB_DEBUG)
        printk("GET_DESCRIPTOR(18) result: %d\n", ret);
    if (ret == 0)
    {
        if (PIUSB_DEBUG)
            printk("  Raw: %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n",
                   ((uint8_t *)&dev_desc)[0], ((uint8_t *)&dev_desc)[1],
                   ((uint8_t *)&dev_desc)[2], ((uint8_t *)&dev_desc)[3],
                   ((uint8_t *)&dev_desc)[4], ((uint8_t *)&dev_desc)[5],
                   ((uint8_t *)&dev_desc)[6], ((uint8_t *)&dev_desc)[7],
                   ((uint8_t *)&dev_desc)[8], ((uint8_t *)&dev_desc)[9],
                   ((uint8_t *)&dev_desc)[10], ((uint8_t *)&dev_desc)[11],
                   ((uint8_t *)&dev_desc)[12], ((uint8_t *)&dev_desc)[13],
                   ((uint8_t *)&dev_desc)[14], ((uint8_t *)&dev_desc)[15],
                   ((uint8_t *)&dev_desc)[16], ((uint8_t *)&dev_desc)[17]);
        if (PIUSB_DEBUG)
            printk("Device: VID=0x%x PID=0x%x\n", dev_desc.idVendor, dev_desc.idProduct);
    }

    // 4. Get configuration descriptor header
    static uint8_t config_buf[256] __attribute__((aligned(32)));
    if (usb_get_config_descriptor(ctx, ctx->device.address, config_buf, 9) == 0)
    {
        uint16_t total_len = config_buf[2] | (config_buf[3] << 8);
        if (PIUSB_DEBUG)
            printk("Config total length: %d\n", total_len);

        // Get full config
        if (total_len <= 256)
        {
            usb_get_config_descriptor(ctx, ctx->device.address, config_buf, total_len);

            // Parse for HID keyboard endpoint (protocol=1 is boot keyboard)
            uint8_t *ptr = config_buf;
            uint8_t *end = config_buf + total_len;
            int current_iface_is_keyboard = 0;
            int current_iface_is_hid = 0;
            int current_iface_protocol = 0;
            int found_keyboard_ep = 0;
            int found_aux_ep = 0;

            while (ptr < end)
            {
                uint8_t len = ptr[0];
                uint8_t type = ptr[1];
                if (len == 0)
                    break;

                if (type == 4 && len >= 9)
                { // Interface descriptor
                    uint8_t iface_class = ptr[5];
                    uint8_t iface_subclass = ptr[6];
                    uint8_t iface_protocol = ptr[7];
                    if (PIUSB_DEBUG)
                        printk("Interface %d: class=%d subclass=%d protocol=%d\n",
                               ptr[2], iface_class, iface_subclass, iface_protocol);

                    // HID class=3, subclass=1 (boot), protocol=1 (keyboard)
                    current_iface_is_keyboard = (iface_class == 3 &&
                                                 iface_subclass == 1 &&
                                                 iface_protocol == 1);
                    current_iface_is_hid = (iface_class == 3);
                    current_iface_protocol = iface_protocol;
                }
                if (type == 5 && len >= 7)
                { // Endpoint descriptor
                    uint8_t ep_addr = ptr[2];
                    uint8_t ep_attr = ptr[3];
                    uint16_t ep_mps = ptr[4] | (ptr[5] << 8);
                    uint8_t ep_int = ptr[6];

                    if (PIUSB_DEBUG)
                        printk("Endpoint %x: attr=%x mps=%d interval=%d%s\n",
                               ep_addr, ep_attr, ep_mps, ep_int,
                               current_iface_is_keyboard ? " [KEYBOARD]" : "");

                    // Primary: first boot keyboard interrupt IN endpoint
                    if (!found_keyboard_ep && current_iface_is_keyboard &&
                        (ep_attr & 3) == 3 && (ep_addr & 0x80))
                    {
                        ctx->hid.endpoint = ep_addr & 0x7F;
                        ctx->hid.ep_mps = ep_mps;
                        ctx->hid.interval = ep_int;
                        found_keyboard_ep = 1;
                        if (PIUSB_DEBUG)
                            printk(">>> Using keyboard endpoint %d\n", ctx->hid.endpoint);
                    }
                    // Aux: first non-boot, non-mouse HID interrupt IN endpoint
                    // (e.g. extended keyboard/status on 2.4G transceivers)
                    else if (!found_aux_ep && current_iface_is_hid &&
                             !current_iface_is_keyboard &&
                             current_iface_protocol != 2 && // skip mouse
                             (ep_attr & 3) == 3 && (ep_addr & 0x80))
                    {
                        ctx->aux.endpoint = ep_addr & 0x7F;
                        ctx->aux.ep_mps = ep_mps;
                        ctx->aux.interval = ep_int;
                        ctx->aux.interrupt_toggle = PID_DATA0;
                        found_aux_ep = 1;
                        if (PIUSB_DEBUG)
                            printk(">>> Using aux endpoint %d\n", ctx->aux.endpoint);
                    }
                }
                ptr += len;
            }
        }
    }

    // 5. Set configuration
    usb_set_configuration(ctx, ctx->device.address, 1);
    udelay(50000);

    // 6. Set boot protocol and idle
    usb_hid_set_protocol(ctx, ctx->device.address, 0, 0); // Boot protocol
    // usb_hid_set_idle(ctx, ctx->device.address, 0);
    udelay(50000);

    // 7. Turn on NumLock LED (like Linux does)
    // SET_REPORT: type=2 (Output), id=0, data=0x01 (NumLock)
    uint8_t led_state = 0x01; // NumLock LED on
    usb_hid_set_report(ctx, ctx->device.address, 0, 2, 0, &led_state, 1);
    udelay(50000);

    if (ctx->aux.endpoint)
        printk("Aux HID endpoint %d (MPS=%d) found\n",
               ctx->aux.endpoint, ctx->aux.ep_mps);

    return 0;
}