#ifndef PIUSB_H
#define PIUSB_H
#include "types.h"
#include "mem.h"

#define PIUSB_DEBUG 0

// ARM physical to bus address for DMA
// 0xC0000000 = uncached (bypasses L2, but then ARM reads from L2 = stale!)
// 0x40000000 = L2 coherent (GPU and ARM share L2 cache - this is what we want!)
#define BUS_ADDRESS(addr) (((uint32_t)(addr) & ~0xC0000000) | 0x40000000)

// For L2-coherent DMA, we can read from normal cached address
#define UNCACHED_ADDR(addr) ((void *)(addr))

// ============================================================================
// DWC2 USB Controller Registers
// ============================================================================

#define USB_BASE 0x20980000u
#define USB_CORE_BASE (USB_BASE + 0x0000u)
#define USB_HOST_BASE (USB_BASE + 0x0400u)

// Core registers
#define GOTGCTL (USB_CORE_BASE + 0x00)
#define GAHBCFG (USB_CORE_BASE + 0x08)
#define GUSBCFG (USB_CORE_BASE + 0x0C)
#define GRSTCTL (USB_CORE_BASE + 0x10)
#define GINTSTS (USB_CORE_BASE + 0x14)
#define GINTMSK (USB_CORE_BASE + 0x18)
#define GRXSTSR (USB_CORE_BASE + 0x1C)
#define GRXSTSP (USB_CORE_BASE + 0x20)
#define GRXFSIZ (USB_CORE_BASE + 0x24)
#define GNPTXFSIZ (USB_CORE_BASE + 0x28)
#define GNPTXSTS (USB_CORE_BASE + 0x2C)
#define HPTXFSIZ (USB_CORE_BASE + 0x100)

// Host registers
#define HCFG (USB_HOST_BASE + 0x00)
#define HFIR (USB_HOST_BASE + 0x04)
#define HFNUM (USB_HOST_BASE + 0x08)
#define HAINT (USB_HOST_BASE + 0x14)
#define HAINTMSK (USB_HOST_BASE + 0x18)
#define HPRT (USB_HOST_BASE + 0x40)

// Host channel registers - macro for any channel
#define HCCHAR(n) (USB_HOST_BASE + 0x100 + (n) * 0x20)
#define HCSPLT(n) (USB_HOST_BASE + 0x104 + (n) * 0x20)
#define HCINT(n) (USB_HOST_BASE + 0x108 + (n) * 0x20)
#define HCINTMSK(n) (USB_HOST_BASE + 0x10C + (n) * 0x20)
#define HCTSIZ(n) (USB_HOST_BASE + 0x110 + (n) * 0x20)
#define HCDMA(n) (USB_HOST_BASE + 0x114 + (n) * 0x20)

// Power register
#define ARM_USB_POWER (USB_BASE + 0xE00)

// GAHBCFG bits
#define GAHBCFG_GLBLINTRMSK (1u << 0)
#define GAHBCFG_HBSTLEN_INCR4 (3u << 1)
#define GAHBCFG_DMAEN (1u << 5)

// GRSTCTL bits
#define GRSTCTL_CSRST (1u << 0)
#define GRSTCTL_AHBIDLE (1u << 31)
#define GRSTCTL_TXFFLSH (1u << 5)
#define GRSTCTL_RXFFLSH (1u << 4)

// HCCHAR bits
#define HCCHAR_CHENA (1u << 31)
#define HCCHAR_CHDIS (1u << 30)
#define HCCHAR_ODDFRM (1u << 29)
#define HCCHAR_DEVADDR_SHIFT 22
#define HCCHAR_MC_SHIFT 20
#define HCCHAR_EPTYPE_SHIFT 18
#define HCCHAR_LSDEV (1u << 17)
#define HCCHAR_EPDIR_IN (1u << 15)
#define HCCHAR_EPNUM_SHIFT 11
#define HCCHAR_MPS_MASK 0x7FF

// HCINT bits
#define HCINT_XFERCOMP (1u << 0)
#define HCINT_CHHLTD (1u << 1)
#define HCINT_AHBERR (1u << 2)
#define HCINT_STALL (1u << 3)
#define HCINT_NAK (1u << 4)
#define HCINT_ACK (1u << 5)
#define HCINT_NYET (1u << 6)
#define HCINT_XACTERR (1u << 7)
#define HCINT_BBLERR (1u << 8)
#define HCINT_FRMOVRUN (1u << 9)
#define HCINT_DATATGLERR (1u << 10)

// HCTSIZ bits
#define HCTSIZ_PID_SHIFT 29
#define HCTSIZ_PKTCNT_SHIFT 19
#define HCTSIZ_XFERSIZE_MASK 0x7FFFF

// DWC2 PID values
#define PID_DATA0 0
#define PID_DATA1 2
#define PID_SETUP 3

// Endpoint types
#define EP_TYPE_CONTROL 0
#define EP_TYPE_ISO 1
#define EP_TYPE_BULK 2
#define EP_TYPE_INTERRUPT 3

// Channel assignments (CRITICAL: use dedicated channels)
#define CH_CONTROL_OUT 0 // SETUP, DATA OUT, STATUS OUT
#define CH_CONTROL_IN 1  // DATA IN, STATUS IN
#define CH_INTERRUPT 2   // Interrupt IN for HID (keyboard)
#define CH_INTERRUPT2 3  // Interrupt IN for aux HID (status/extended)

// ============================================================================
// USB structures
// ============================================================================

struct usb_setup
{
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} __attribute__((packed, aligned(4)));

struct usb_device_descriptor
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdUSB;
    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t iManufacturer;
    uint8_t iProduct;
    uint8_t iSerialNumber;
    uint8_t bNumConfigurations;
} __attribute__((packed));

struct usb_config_descriptor
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wTotalLength;
    uint8_t bNumInterfaces;
    uint8_t bConfigurationValue;
    uint8_t iConfiguration;
    uint8_t bmAttributes;
    uint8_t bMaxPower;
} __attribute__((packed));

struct usb_interface_descriptor
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
} __attribute__((packed));

struct usb_endpoint_descriptor
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bEndpointAddress;
    uint8_t bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t bInterval;
} __attribute__((packed));

struct hid_keyboard_report
{
    uint8_t modifiers;
    uint8_t reserved;
    uint8_t keys[6];
} __attribute__((packed));

// ============================================================================
// Structs for state management (no globals)
// ============================================================================

// USB device state
typedef struct usb_device_state
{
    uint32_t speed;          // 0=HS, 1=FS, 2=LS
    uint8_t address;         // Current device address
    uint8_t max_packet_size; // Control EP0 max packet size
} usb_device_state_t;

// HID endpoint state
typedef struct usb_hid_state
{
    uint8_t endpoint;         // Interrupt endpoint number
    uint16_t ep_mps;          // Endpoint max packet size
    uint8_t interval;         // Polling interval
    uint8_t interrupt_toggle; // PID toggle for interrupt transfers
} usb_hid_state_t;

// DMA buffers - must be cache-line aligned and stable during transfers
typedef struct usb_dma_buffers
{
    uint8_t __attribute__((aligned(32))) setup_buf[32];
    uint8_t __attribute__((aligned(32))) data_buf[16][64];
    uint8_t __attribute__((aligned(32))) status_buf[32];
    uint8_t __attribute__((aligned(32))) int_buf[64][64];
    uint32_t __attribute__((aligned(16))) mbox_buf[32];
    int data_buf_idx;
    int int_buf_idx;
} usb_dma_buffers_t;

// Debug/diagnostic state
typedef struct usb_debug_state
{
    uint32_t last_hcint;
    uint32_t hctsiz_before;
    uint32_t hctsiz_rem_before;
} usb_debug_state_t;

// Combined USB context - pass this around
typedef struct usb_context
{
    usb_device_state_t device;
    usb_hid_state_t hid;
    usb_hid_state_t aux; // Auxiliary HID endpoint (status/extended keyboard)
    usb_dma_buffers_t dma;
    usb_debug_state_t debug;
} usb_context_t;

// Functions
void usb_context_init(usb_context_t *ctx);
bool usb_power_on(usb_context_t *ctx);
void usb_core_reset(void);
void usb_flush_tx_fifo(uint32_t fifo_num);
void usb_flush_rx_fifo(void);
void usb_force_host_mode(void);
void usb_host_init(void);
void usb_port_power_on(void);
void usb_port_reset(usb_context_t *ctx);
void usb_transfer_channel_halt(int ch);
int usb_transfer_channel_wait(usb_context_t *ctx, int ch);
void usb_transfer_channel_start(usb_context_t *ctx, int ch, uint8_t devaddr, uint8_t ep, uint8_t eptype,
                                       bool dir_in, uint16_t mps, uint8_t pid,
                                       uint32_t xfersize, uint32_t pktcnt, uint32_t dma_addr);
int usb_setup_stage_dma_xfer(usb_context_t *ctx, uint8_t devaddr, struct usb_setup *setup);
int usb_setup_stage_dma_data_in(usb_context_t *ctx, uint8_t devaddr, void *buf, uint32_t len);
int do_status_out_stage(usb_context_t *ctx, uint8_t devaddr);
int usb_setup_stage_dma_status_in(usb_context_t *ctx, uint8_t devaddr);
int usb_control_in(usb_context_t *ctx, uint8_t devaddr, struct usb_setup *setup, void *data, uint16_t len);
int usb_dma_data_out(usb_context_t *ctx, uint8_t devaddr, void *buf, uint32_t len);
int usb_control_out_data(usb_context_t *ctx, uint8_t devaddr, struct usb_setup *setup, void *data, uint16_t len);
int usb_control_out_nodata(usb_context_t *ctx, uint8_t devaddr, struct usb_setup *setup);
int usb_get_device_descriptor(usb_context_t *ctx, uint8_t devaddr, struct usb_device_descriptor *desc, uint8_t len);
int usb_get_config_descriptor(usb_context_t *ctx, uint8_t devaddr, uint8_t *buf, uint16_t len);
int usb_set_address(usb_context_t *ctx, uint8_t new_addr);
int usb_set_configuration(usb_context_t *ctx, uint8_t devaddr, uint8_t config);
int usb_hid_set_idle(usb_context_t *ctx, uint8_t devaddr, uint8_t interface);
int usb_hid_set_protocol(usb_context_t *ctx, uint8_t devaddr, uint8_t interface, uint8_t protocol);
int usb_hid_set_report(usb_context_t *ctx, uint8_t devaddr, uint8_t interface, uint8_t report_type,
                              uint8_t report_id, void *data, uint16_t len);
int usb_interrupt_poll_ch(usb_context_t *ctx, int ch, int buf_idx,
                         uint8_t devaddr, usb_hid_state_t *ep_state,
                         void *buf, uint16_t len);
void usb_interrupt_init_ch(usb_context_t *ctx, int ch, int buf_idx,
                           uint8_t devaddr, usb_hid_state_t *ep_state);
int usb_hid_full_init(usb_context_t *ctx);
#endif