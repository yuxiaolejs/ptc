#include "emmc.h"
#include "cstr.h"
#include "pictl.h"
#include "gpio.h"
#include "mem.h"
#include "pimath.h"
// A stolen file from https://github.com/rockytriton/LLD/blob/main/rpi_bm/part17/src/drivers/emmc/emmc.c

#define MAX_DEVS 10

static io_device *devices[MAX_DEVS] = {0};

bool io_device_register(io_device *dev)
{
    for (int i = 0; i < MAX_DEVS; i++)
    {
        if (devices[i] == 0)
        {
            devices[i] = dev;
            return true;
        }
    }

    return false;
}

io_device *io_device_find(char *name)
{
    for (int i = 0; i < MAX_DEVS; i++)
    {
        if (str_eq(devices[i]->name, name))
        {
            return devices[i];
        }
    }

    return 0;
}

bool emmc_setup_clock();
bool switch_clock_rate(uint32_t base_clock, uint32_t target_rate);

static emmc_device device = {0};

static const emmc_cmd INVALID_CMD = RES_CMD;

typedef struct
{
    reg32 read;
    reg32 res[5];
    reg32 status;
    reg32 config;
    reg32 write;
} mailbox_regs;

mailbox_regs *MBX()
{
    return (mailbox_regs *)(MMIO_BASE + 0xB880);
}

typedef struct
{
    uint32_t size;
    uint32_t code;
    uint8_t tags[0];
} property_buffer;

static uint32_t property_data[8192] __attribute__((aligned(16)));

#define MAIL_EMPTY 0x40000000
#define MAIL_FULL 0x80000000

#define MAIL_POWER 0x0   // Mailbox Channel 0: Power Management Interface
#define MAIL_FB 0x1      // Mailbox Channel 1: Frame Buffer
#define MAIL_VUART 0x2   // Mailbox Channel 2: Virtual UART
#define MAIL_VCHIQ 0x3   // Mailbox Channel 3: VCHIQ Interface
#define MAIL_LEDS 0x4    // Mailbox Channel 4: LEDs Interface
#define MAIL_BUTTONS 0x5 // Mailbox Channel 5: Buttons Interface
#define MAIL_TOUCH 0x6   // Mailbox Channel 6: Touchscreen Interface
#define MAIL_COUNT 0x7   // Mailbox Channel 7: Counter
// #define MAIL_TAGS 0x8    // Mailbox Channel 8: Tags (ARM to VC)

static void mailbox_write(uint8_t channel, uint32_t data)
{
    while (MBX()->status & MAIL_FULL)
        ;

    MBX()->write = ((data & 0xFFFFFFF0) | (channel & 0xF));
}

static uint32_t mailbox_read(uint8_t channel)
{
    while (true)
    {
        while (MBX()->status & MAIL_EMPTY)
            ;

        uint32_t data = MBX()->read;

        uint8_t read_channel = (uint8_t)(data & 0xF);

        if (read_channel == channel)
        {
            return data & 0xFFFFFFF0;
        }
    }
}

bool mailbox_process(mailbox_tag *tag, uint32_t tag_size)
{
    int buffer_size = tag_size + 12;

    memcpy(&property_data[2], tag, tag_size);

    if (EMMC_DEBUG)
        printk("Mailbox process tag id: %x\n", tag->id);

    property_buffer *buff = (property_buffer *)property_data;
    buff->size = buffer_size;
    buff->code = RPI_FIRMWARE_STATUS_REQUEST;
    property_data[math_div(tag_size + 12, 4) - 1] = RPI_FIRMWARE_PROPERTY_END;
    if (EMMC_DEBUG)
        printk("Mailbox write buffer size: %d\n", buff->size);
    mailbox_write(MAIL_TAGS, (uint32_t)(void *)property_data);
    if (EMMC_DEBUG)
        printk("Mailbox write done\n");
    delay_ms(100);

    int result = mailbox_read(MAIL_TAGS);

    memcpy(tag, property_data + 2, tag_size);

    return true;
}

bool mailbox_generic_command(uint32_t tag_id, uint32_t id, uint32_t *value)
{
    mailbox_generic mbx;
    mbx.tag.id = tag_id;
    mbx.tag.value_length = 0;
    mbx.tag.buffer_size = sizeof(mailbox_generic) - sizeof(mailbox_tag);
    mbx.id = id;
    mbx.value = *value;

    if (!mailbox_process((mailbox_tag *)&mbx, sizeof(mbx)))
    {
        printk("FAILED TO PROCESS: %x\n", tag_id);
        return false;
    }

    *value = mbx.value;

    return true;
}

uint32_t mailbox_clock_rate(clock_type ct)
{
    mailbox_clock c;
    c.tag.id = RPI_FIRMWARE_GET_CLOCK_RATE;
    c.tag.value_length = 0;
    c.tag.buffer_size = sizeof(c) - sizeof(c.tag);
    c.id = ct;

    mailbox_process((mailbox_tag *)&c, sizeof(c));

    return c.rate;
}

bool mailbox_power_check(uint32_t type)
{
    mailbox_power p;
    p.tag.id = RPI_FIRMWARE_GET_DOMAIN_STATE;
    p.tag.value_length = 0;
    p.tag.buffer_size = sizeof(p) - sizeof(p.tag);
    p.id = type;
    p.state = ~0;

    mailbox_process((mailbox_tag *)&p, sizeof(p));

    return p.state && p.state != ~0;
}

bool wait_reg_mask(reg32 *reg, uint32_t mask, bool set, uint32_t timeout)
{
    for (int cycles = 0; cycles <= timeout; cycles++)
    {
        if ((*reg & mask) ? set : !set)
        {
            return true;
        }

        delay_ms(1);
    }

    return false;
}

static const emmc_cmd commands[] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    RES_CMD,
    {0, 0, 0, 0, 0, 0, RT136, 0, 1, 0, 0, 0, 2, 0},
    {0, 0, 0, 0, 0, 0, RT48, 0, 1, 0, 0, 0, 3, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0},
    {0, 0, 0, 0, 0, 0, RT136, 0, 0, 0, 0, 0, 5, 0},
    {0, 0, 0, 0, 0, 0, RT48, 0, 1, 0, 0, 0, 6, 0},
    {0, 0, 0, 0, 0, 0, RT48Busy, 0, 1, 0, 0, 0, 7, 0},
    {0, 0, 0, 0, 0, 0, RT48, 0, 1, 0, 0, 0, 8, 0},
    {0, 0, 0, 0, 0, 0, RT136, 0, 1, 0, 0, 0, 9, 0},
    RES_CMD,
    RES_CMD,
    RES_CMD,
    {0, 0, 0, 0, 0, 0, RT48, 0, 1, 0, 0, 0, 13, 0},
    RES_CMD,
    RES_CMD,
    {0, 0, 0, 0, 0, 0, RT48, 0, 1, 0, 0, 0, 16, 0},
    {0, 0, 0, 1, 0, 0, RT48, 0, 1, 0, 1, 0, 17, 0},
    {0, 1, 1, 1, 1, 0, RT48, 0, 1, 0, 1, 0, 18, 0},
    RES_CMD,
    RES_CMD,
    RES_CMD,
    RES_CMD,
    RES_CMD,
    {0, 0, 0, 0, 0, 0, RT48, 0, 1, 0, 1, 0, 24, 0},
    RES_CMD,
    RES_CMD,
    RES_CMD,
    RES_CMD,
    RES_CMD,
    RES_CMD,
    RES_CMD,
    RES_CMD,
    RES_CMD,
    RES_CMD,
    RES_CMD,
    RES_CMD,
    RES_CMD,
    RES_CMD,
    RES_CMD,
    RES_CMD,
    {0, 0, 0, 0, 0, 0, RT48, 0, 0, 0, 0, 0, 41, 0},
    RES_CMD,
    RES_CMD,
    RES_CMD,
    RES_CMD,
    RES_CMD,
    RES_CMD,
    RES_CMD,
    RES_CMD,
    RES_CMD,
    {0, 0, 0, 1, 0, 0, RT48, 0, 1, 0, 1, 0, 51, 0},
    RES_CMD,
    RES_CMD,
    RES_CMD,
    {0, 0, 0, 0, 0, 0, RT48, 0, 1, 0, 0, 0, 55, 0},
};
#define EMMC_MAX_RESET_ATTEMPTS 1000
static void emmc_host_reset_cmd_dat(void)
{
    // Clear pending ints first (optional)
    uint32_t att = 0;
    EMMC->int_flags = 0xFFFFFFFF;

    // SDHCI software reset: CMD line
    EMMC->control2 |= (1 << 1);
    while (EMMC->control2 & (1 << 1))
        if (++att > EMMC_MAX_RESET_ATTEMPTS)
        {
            printk("EMMC_ERR: CMD line reset timeout\n");
            delay_ms(1);
            break;
        }

    // SDHCI software reset: DAT line
    att = 0;
    EMMC->control2 |= (1 << 2);
    while (EMMC->control2 & (1 << 2))
        if (++att > EMMC_MAX_RESET_ATTEMPTS)
        {
            printk("EMMC_ERR: DAT line reset timeout\n");
            delay_ms(1);
            break;
        }

    // Clear again after reset
    EMMC->int_flags = 0xFFFFFFFF;
}

static uint32_t sd_error_mask(sd_error err)
{
    return 1 << (16 + (uint32_t)err);
}

static void set_last_error(uint32_t intr_val)
{
    device.last_error = intr_val & 0xFFFF0000;
    device.last_interrupt = intr_val;
}

static bool do_data_transfer(emmc_cmd cmd)
{
    if (EMMC_DEBUG)
        printk("DATA XFER: CMD%d start, status=%x int=%x\n",
               cmd.index, EMMC->status, EMMC->int_flags);
    uint32_t wrIrpt = 0;
    bool write = false;

    if (cmd.direction)
    {
        wrIrpt = 1 << 5;
    }
    else
    {
        wrIrpt = 1 << 4;
        write = true;
    }

    uint32_t *data = (uint32_t *)device.buffer;

    for (int block = 0; block < device.transfer_blocks; block++)
    {
        wait_reg_mask(&EMMC->int_flags, wrIrpt | 0x8000, true, 20000);
        uint32_t intr_val = EMMC->int_flags;
        EMMC->int_flags = wrIrpt | 0x8000;

        bool is_write =
            (cmd.index == 24) || // WRITE_SINGLE_BLOCK
            (cmd.index == 25);

        if (EMMC_DEBUG)
            printk("DATA XFER: CMD%d waiting for %s\n",
                   cmd.index,
                   is_write ? "BUFFER_WRITE_READY" : "BUFFER_READ_READY");

        if ((intr_val & (0xffff0000 | wrIrpt)) != wrIrpt)
        {
            set_last_error(intr_val);
            return false;
        }

        uint32_t length = device.block_size;

        if (write)
        {
            for (; length > 0; length -= 4)
            {
                EMMC->data = *data++;
            }
        }
        else
        {
            for (; length > 0; length -= 4)
            {
                *data++ = EMMC->data;
            }
        }
    }

    return true;
}

static bool emmc_issue_command(emmc_cmd cmd, uint32_t arg, uint32_t timeout)
{
    device.last_command_value = TO_REG(&cmd);
    reg32 command_reg = device.last_command_value;

    if (EMMC_DEBUG)
        printk("CMD %d PREP: cmd_xfer_mode=%x blocks=%u blksize=%u arg=%x\n",
               cmd.index,
               command_reg,
               device.transfer_blocks,
               device.block_size,
               arg);

    if (device.transfer_blocks > 0xFFFF)
    {
        printk("EMMC_ERR: transferBlocks too large: %d\n", device.transfer_blocks);
        return false;
    }

    // Wait for controller ready
    int t = 0;
    while (EMMC->status & 0x3)
    { // CMD_INHIBIT | DAT_INHIBIT
        delay_ms(1);
        if (++t > 2000)
        {
            printk("++++++++++EMMC_ERR: inhibit stuck before CMD%d, status=%x\n",
                   cmd.index, EMMC->status);
            emmc_host_reset_cmd_dat(); // reset command and data lines
            return false;
        }
    }

    EMMC->block_size_count = device.block_size | (device.transfer_blocks << 16);
    EMMC->arg1 = arg;

    if (EMMC_DEBUG)
        printk("CMD%d ISSUE: status=%x int_flags=%x\n",
               cmd.index, EMMC->status, EMMC->int_flags);
    EMMC->cmd_xfer_mode = command_reg;

    delay_ms(2);

    int times = 0;

    while (times < timeout)
    {
        uint32_t reg = EMMC->int_flags;

        if (reg & 0x8001)
        {
            if (reg & 0x8001)
            {
                if (EMMC_DEBUG)
                    printk("CMD%d DONE: int_flags=%x status=%x\n",
                           cmd.index, reg, EMMC->status);
                break;
            }
            break;
        }

        delay_ms(1);
        times++;
    }

    if (times >= timeout)
    {
        // just doing a warn for this because sometimes it's ok.
        printk("EMMC_WARN: emmc_issue_command timed out\n");
        device.last_success = false;
        return false;
    }

    uint32_t intr_val = EMMC->int_flags;

    EMMC->int_flags = 0xFFFF0001;

    if ((intr_val & 0xFFFF0001) != 1)
    {

        if (EMMC_DEBUG)
            printk("EMMC_DEBUG: Error waiting for command interrupt complete: %d\n", cmd.index);

        set_last_error(intr_val);

        if (EMMC_DEBUG)
            printk("EMMC_DEBUG: IRQFLAGS: %x - %x - %x\n", EMMC->int_flags, EMMC->status, intr_val);

        device.last_success = false;
        return false;
    }

    switch (cmd.response_type)
    {
    case RT48:
    case RT48Busy:
        device.last_response[0] = EMMC->response[0];
        break;

    case RT136:
        device.last_response[0] = EMMC->response[0];
        device.last_response[1] = EMMC->response[1];
        device.last_response[2] = EMMC->response[2];
        device.last_response[3] = EMMC->response[3];
        break;
    }

    if (cmd.is_data)
    {
        if (EMMC_DEBUG)
            printk("CMD%d DATA: entering do_data_transfer (status=%x int=%x)\n",
                   cmd.index, EMMC->status, EMMC->int_flags);
        do_data_transfer(cmd);
    }

    if (cmd.response_type == RT48Busy || cmd.is_data)
    {
        wait_reg_mask(&EMMC->int_flags, 0x8002, true, 2000);
        intr_val = EMMC->int_flags;

        EMMC->int_flags = 0xFFFF0002;

        if ((intr_val & 0xFFFF0002) != 2 && (intr_val & 0xFFFF0002) != 0x100002)
        {
            set_last_error(intr_val);
            return false;
        }

        EMMC->int_flags = 0xFFFF0002;
    }

    device.last_success = true;

    return true;
}

static bool emmc_command(uint32_t command, uint32_t arg, uint32_t timeout)
{
    if (command & 0x80000000)
    {
        // The app command flag is set, shoudl use emmc_app_command instead.
        printk("EMMC_ERR: COMMAND ERROR NOT APP\n");
        return false;
    }

    device.last_command = commands[command];

    if (EMMC_DEBUG)
        printk("IS THIS CURRENT A DATA COMMAND? CMD%d\n", device.last_command.is_data);

    if (TO_REG(&device.last_command) == TO_REG(&INVALID_CMD))
    {
        printk("EMMC_ERR: INVALID COMMAND!\n");
        return false;
    }

    return emmc_issue_command(device.last_command, arg, timeout);
}

static bool reset_command()
{
    EMMC->control[1] |= EMMC_CTRL1_RESET_CMD;

    for (int i = 0; i < 10000; i++)
    {
        if (!(EMMC->control[1] & EMMC_CTRL1_RESET_CMD))
        {
            return true;
        }

        delay_ms(1);
    }

    printk("EMMC_ERR: Command line failed to reset properly: %x\n", EMMC->control[1]);

    return false;
}

bool emmc_app_command(uint32_t command, uint32_t arg, uint32_t timeout)
{

    if (commands[command].index >= 60)
    {
        printk("EMMC_ERR: INVALID APP COMMAND\n");
        return false;
    }

    device.last_command = commands[CTApp];

    uint32_t rca = 0;

    if (device.rca)
    {
        rca = device.rca << 16;
    }

    if (emmc_issue_command(device.last_command, rca, 2000))
    {
        device.last_command = commands[command];

        return emmc_issue_command(device.last_command, arg, 2000);
    }

    return false;
}

static bool check_v2_card()
{
    bool v2Card = false;

    if (!emmc_command(CTSendIfCond, 0x1AA, 200))
    {
        if (device.last_error == 0)
        {
            // timeout.
            printk("EMMC_ERR: SEND_IF_COND Timeout\n");
        }
        else if (device.last_error & (1 << 16))
        {
            // timeout command error
            if (!reset_command())
            {
                return false;
            }

            EMMC->int_flags = sd_error_mask(SDECommandTimeout);
            printk("EMMC_ERR: SEND_IF_COND CMD TIMEOUT\n");
        }
        else
        {
            printk("EMMC_ERR: Failure sending SEND_IF_COND\n");
            return false;
        }
    }
    else
    {
        if ((device.last_response[0] & 0xFFF) != 0x1AA)
        {
            printk("EMMC_ERR: Unusable SD Card: %x\n", device.last_response[0]);
            return false;
        }

        v2Card = true;
    }

    return v2Card;
}

static bool check_usable_card()
{
    if (!emmc_command(CTIOSetOpCond, 0, 1000))
    {
        if (device.last_error == 0)
        {
            // timeout.
            printk("EMMC_ERR: CTIOSetOpCond Timeout\n");
        }
        else if (device.last_error & (1 << 16))
        {
            // timeout command error
            // this is a normal expected error and calling the reset command will fix it.
            if (!reset_command())
            {
                return false;
            }

            EMMC->int_flags = sd_error_mask(SDECommandTimeout);
        }
        else
        {
            printk("EMMC_ERR: SDIO Card not supported\n");
            return false;
        }
    }

    return true;
}

static bool check_sdhc_support(bool v2_card)
{
    bool card_busy = true;

    while (card_busy)
    {
        uint32_t v2_flags = 0;

        if (v2_card)
        {
            v2_flags |= (1 << 30); // SDHC Support
        }

        if (!emmc_app_command(CTOcrCheck, 0x00FF8000 | v2_flags, 2000))
        {
            printk("EMMC_ERR: APP CMD 41 FAILED 2nd\n");
            return false;
        }

        if (device.last_response[0] >> 31 & 1)
        {
            device.ocr = (device.last_response[0] >> 8 & 0xFFFF);
            device.sdhc = ((device.last_response[0] >> 30) & 1) != 0;
            card_busy = false;
        }
        else
        {
            if (EMMC_DEBUG)
                printk("EMMC_DEBUG: SLEEPING: %x\n", device.last_response[0]);
            delay_ms(500);
        }
    }

    return true;
}

static bool check_ocr()
{
    bool passed = false;

    for (int i = 0; i < 5; i++)
    {
        if (!emmc_app_command(CTOcrCheck, 0, 2000))
        {
            printk("EMMC_WARN: APP CMD OCR CHECK TRY %d FAILED\n", i + 1);
            passed = false;
        }
        else
        {
            passed = true;
        }

        if (passed)
        {
            break;
        }

        return false;
    }

    if (!passed)
    {
        printk("EMMC_ERR: APP CMD 41 FAILED\n");
        return false;
    }

    device.ocr = (device.last_response[0] >> 8 & 0xFFFF);

    if (EMMC_DEBUG)
        printk("MEMORY OCR: %x\n", device.ocr);

    return true;
}

static bool check_rca()
{
    if (!emmc_command(CTSendCide, 0, 2000))
    {
        printk("EMMC_ERR: Failed to send CID\n");

        return false;
    }

    if (EMMC_DEBUG)
        printk("EMMC_DEBUG: CARD ID: %x.%x.%x.%x\n", device.last_response[0], device.last_response[1], device.last_response[2], device.last_response[3]);

    if (!emmc_command(CTSendRelativeAddr, 0, 2000))
    {
        printk("EMMC_ERR: Failed to send Relative Addr\n");

        return false;
    }

    device.rca = (device.last_response[0] >> 16) & 0xFFFF;

    if (EMMC_DEBUG)
    {
        printk("EMMC_DEBUG: RCA: %x\n", device.rca);

        printk("EMMC_DEBUG: CRC_ERR: %d\n", (device.last_response[0] >> 15) & 1);
        printk("EMMC_DEBUG: CMD_ERR: %d\n", (device.last_response[0] >> 14) & 1);
        printk("EMMC_DEBUG: GEN_ERR: %d\n", (device.last_response[0] >> 13) & 1);
        printk("EMMC_DEBUG: STS_ERR: %d\n", (device.last_response[0] >> 9) & 1);
        printk("EMMC_DEBUG: READY  : %d\n", (device.last_response[0] >> 8) & 1);
    }

    if (!((device.last_response[0] >> 8) & 1))
    {
        printk("EMMC_ERR: Failed to read RCA\n");
        return false;
    }

    return true;
}

static bool select_card()
{
    if (!emmc_command(CTSelectCard, device.rca << 16, 2000))
    {
        printk("EMMC_ERR: Failed to select card\n");
        return false;
    }

    if (EMMC_DEBUG)
        printk("EMMC_DEBUG: Selected Card\n");

    uint32_t status = (device.last_response[0] >> 9) & 0xF;

    if (status != 3 && status != 4)
    {
        printk("EMMC_ERR: Invalid Status: %d\n", status);
        return false;
    }

    if (EMMC_DEBUG)
        printk("EMMC_DEBUG: Status: %d\n", status);

    return true;
}

static bool set_scr()
{
    if (!device.sdhc)
    {
        if (!emmc_command(CTSetBlockLen, 512, 2000))
        {
            printk("EMMC_ERR: Failed to set block len\n");
            return false;
        }
    }

    uint32_t bsc = EMMC->block_size_count;
    bsc &= ~0xFFF; // mask off bottom bits
    bsc |= 0x200;  // set bottom bits to 512
    EMMC->block_size_count = bsc;

    device.buffer = &device.scr.scr[0];
    device.block_size = 8;
    device.transfer_blocks = 1;

    if (!emmc_app_command(CTSendSCR, 0, 30000))
    {
        printk("EMMC_ERR: Failed to send SCR\n");
        return false;
    }

    if (EMMC_DEBUG)
        printk("EMMC_DEBUG: GOT SRC: SCR0: %x SCR1: %x BWID: %x\n", device.scr.scr[0], device.scr.scr[1], device.scr.bus_widths);

    device.block_size = 512;

    uint32_t scr0 = BSWAP32(device.scr.scr[0]);
    device.scr.version = 0xFFFFFFFF;
    uint32_t spec = (scr0 >> (56 - 32)) & 0xf;
    uint32_t spec3 = (scr0 >> (47 - 32)) & 0x1;
    uint32_t spec4 = (scr0 >> (42 - 32)) & 0x1;

    if (spec == 0)
    {
        device.scr.version = 1;
    }
    else if (spec == 1)
    {
        device.scr.version = 11;
    }
    else if (spec == 2)
    {

        if (spec3 == 0)
        {
            device.scr.version = 2;
        }
        else if (spec3 == 1)
        {
            if (spec4 == 0)
            {
                device.scr.version = 3;
            }
            if (spec4 == 1)
            {
                device.scr.version = 4;
            }
        }
    }

    if (EMMC_DEBUG)
        printk("EMMC_DEBUG: SCR Version: %d\n", device.scr.version);

    return true;
}

static bool emmc_card_reset()
{
    EMMC->control[1] = EMMC_CTRL1_RESET_HOST;

    if (EMMC_DEBUG)
        printk("EMMC_DEBUG: Card resetting...\n");

    if (!wait_reg_mask(&EMMC->control[1], EMMC_CTRL1_RESET_ALL, false, 2000))
    {
        printk("EMMC_ERR: Card reset timeout!\n");
        return false;
    }

#if (RPI_VERSION == 4)
    // This enabled VDD1 bus power for SD card, needed for RPI 4.
    uint32_t c0 = EMMC->control[0];
    c0 |= 0x0F << 8;
    EMMC->control[0] = c0;
    delay_ms(3);
#endif

    if (!emmc_setup_clock())
    {
        return false;
    }

    // All interrupts go to interrupt register.
    EMMC->int_enable = 0;
    EMMC->int_flags = 0xFFFFFFFF;
    EMMC->int_mask = 0xFFFFFFFF;

    delay_ms(203);

    device.transfer_blocks = 0;
    device.last_command_value = 0;
    device.last_success = false;
    device.block_size = 0;

    if (!emmc_command(CTGoIdle, 0, 2000))
    {
        printk("EMMC_ERR: NO GO_IDLE RESPONSE\n");
        return false;
    }

    bool v2_card = check_v2_card();

    if (!check_usable_card())
    {
        return false;
    }

    if (!check_ocr())
    {
        return false;
    }

    if (!check_sdhc_support(v2_card))
    {
        return false;
    }

    switch_clock_rate(device.base_clock, SD_CLOCK_HIGH);

    delay_ms(10);

    if (!check_rca())
    {
        return false;
    }

    if (!select_card())
    {
        return false;
    }

    if (!set_scr())
    {
        return false;
    }

    // enable all interrupts
    EMMC->int_flags = 0xFFFFFFFF;

    if (EMMC_DEBUG)
        printk("EMMC_DEBUG: Card reset!\n");

    return true;
}

int emmc_io_read(io_device *dev, void *b, uint32_t size)
{
    return emmc_read((uint8_t *)b, size);
}

void emmc_io_seek(io_device *dev, uint64_t offset)
{
    return emmc_seek(offset);
}

// static int emmc_wait_cmd_done()
// {
//     printk("--->>EMMC_DEBUG: Waiting for CMD Done\n");
//     while (1)
//     {
//         uint32_t f = EMMC->int_flags;
//         printk("EMMC_DEBUG: int_flags=%x\n", f);

//         if (f & INT_CMD_DONE)
//         {
//             EMMC->int_flags = INT_CMD_DONE; // W1C
//             return 0;
//         }

//         if (f & INT_CMD_ERROR_MASK)
//         {
//             printk("CMD error: int_flags=%x\n", f);
//             EMMC->int_flags = f;
//             return -1;
//         }
//     }
// }

bool do_data_command(bool write, uint8_t *b, uint32_t bsize, uint32_t block_no)
{
    if (!device.sdhc)
    {
        block_no *= 512;
    }

    if (bsize < device.block_size)
    {
        printk("EMMC_ERR: INVALID BLOCK SIZE: \n", bsize, device.block_size);
        return false;
    }

    device.transfer_blocks = math_div(bsize, device.block_size);

    if (math_mod(bsize, device.block_size))
    {
        printk("EMMC_ERR: BAD BLOCK SIZE\n");
        return false;
    }

    device.buffer = b;

    cmd_type command = CTReadBlock;

    if (write && device.transfer_blocks > 1)
    {
        command = CTWriteMultiple;
    }
    else if (write)
    {
        command = CTWriteBlock;
    }
    else if (!write && device.transfer_blocks > 1)
    {
        command = CTReadMultiple;
    }

    int retry_count = 0;
    int max_retries = 3;

    if (EMMC_DEBUG)
        printk("EMMC_DEBUG: Sending command: %d\n", command);

    while (retry_count < max_retries)
    {
        if (EMMC_DEBUG)
            printk("BEFORE CALLING EMMC COMMAND CMD%d BLOCKNO: %d\n", command, block_no);
        if (emmc_command(command, block_no, 5000))
        {
            break;
        }

        if (++retry_count < max_retries)
        {
            printk("EMMC_WARN: Retrying data command\n");
        }
        else
        {
            printk("EMMC_ERR: Giving up data command\n");
            return false;
        }
    }
    return true;
}

int do_read(uint8_t *b, uint32_t bsize, uint32_t block_no)
{
    // TODO ENSURE DATA MODE...

    if (!do_data_command(false, b, bsize, block_no))
    {
        printk("EMMC_ERR: do_data_command failed\n");
        return -1;
    }

    return bsize;
}

int do_write(uint8_t *b, uint32_t bsize, uint32_t block_no)
{
    if (!do_data_command(true, b, bsize, block_no))
    {
        printk("EMMC_ERR: do_data_command failed\n");
        return -1;
    }

    return bsize;
}

int emmc_read(uint8_t *buffer, uint32_t size)
{
    if (device.offset % 512 != 0)
    {
        printk("EMMC_ERR: INVALID OFFSET: %d\n", device.offset);
        return -1;
    }

    uint32_t block = device.offset / 512;

    int r = do_read(buffer, size, block);

    if (r != size)
    {
        printk("EMMC_ERR: READ FAILED: %d\n", r);
        return -1;
    }

    return size;
}

int emmc_write(uint8_t *buffer, uint32_t size)
{
    if (device.offset % 512 != 0)
    {
        printk("EMMC_ERR: INVALID OFFSET: %d\n", device.offset);
        return -1;
    }

    uint32_t block = device.offset / 512;

    int r = do_write(buffer, size, block);

    if (r != size)
    {
        printk("EMMC_ERR: WRITE FAILED: %d\n", r);
        return -1;
    }

    return size;
}

void emmc_seek(uint64_t _offset)
{
    device.offset = _offset;
}

static io_device emmc_io_device = {
    .name = "disk",
    .data = &device,
    .read = emmc_io_read,
    .seek = emmc_io_seek};

uint32_t emmc_get_status()
{
    return EMMC->status;
}

uint32_t emmc_get_card_status()
{
    emmc_issue_command(commands[13], device.rca << 16, 2000);
    if (EMMC_DEBUG)
        printk("EMMC_DEBUG: resp: %d\n", device.last_response[0]);
    if (EMMC_DEBUG)
        printk("EMMC_DEBUG: Card Status: %x\n", device.last_response[0]);
    uint32_t r1_status;
    asm volatile("mov %0, r1" : "=r"(r1_status));
    if (EMMC_DEBUG)
        printk("EMMC_DEBUG: R1 Status: %x\n", r1_status);
    return device.last_response[0];
}

bool emmc_init()
{
    io_device_register(&emmc_io_device);

    gpio_pin_set_func(34, GPIO_INPUT);
    gpio_pin_set_func(35, GPIO_INPUT);
    gpio_pin_set_func(36, GPIO_INPUT);
    gpio_pin_set_func(37, GPIO_INPUT);
    gpio_pin_set_func(38, GPIO_INPUT);
    gpio_pin_set_func(39, GPIO_INPUT);

    gpio_pin_set_func(48, GPIO_ALT3);
    gpio_pin_set_func(49, GPIO_ALT3);
    gpio_pin_set_func(50, GPIO_ALT3);
    gpio_pin_set_func(51, GPIO_ALT3);
    gpio_pin_set_func(52, GPIO_ALT3);

    device.transfer_blocks = 0;
    device.last_command_value = 0;
    device.last_success = false;
    device.block_size = 0;
    device.sdhc = false;
    device.ocr = 0;
    device.rca = 0;
    device.offset = 0;
    device.base_clock = 0;

    bool success = false;
    for (int i = 0; i < 10; i++)
    {
        success = emmc_card_reset();

        if (success)
        {
            break;
        }

        delay_ms(100);
        printk("EMMC_WARN: Failed to reset card, trying again...\n");
    }

    if (!success)
    {
        return false;
    }

    return true;
}

bool wait_reg_mask(reg32 *reg, uint32_t mask, bool set, uint32_t timeout);

uint32_t get_clock_divider(uint32_t base_clock, uint32_t target_rate)
{
    uint32_t target_div = 1;

    if (target_rate <= base_clock)
    {
        target_div = math_div(base_clock, target_rate);

        if (math_mod(base_clock, target_rate))
        {
            target_div = 0;
        }
    }

    int div = -1;
    for (int fb = 31; fb >= 0; fb--)
    {
        uint32_t bt = (1 << fb);

        if (target_div & bt)
        {
            div = fb;
            target_div &= ~(bt);

            if (target_div)
            {
                div++;
            }

            break;
        }
    }

    if (div == -1)
    {
        div = 31;
    }

    if (div >= 32)
    {
        div = 31;
    }

    if (div != 0)
    {
        div = (1 << (div - 1));
    }

    if (div >= 0x400)
    {
        div = 0x3FF;
    }

    uint32_t freqSel = div & 0xff;
    uint32_t upper = (div >> 8) & 0x3;
    uint32_t ret = (freqSel << 8) | (upper << 6) | (0 << 5);

    return ret;
}

bool switch_clock_rate(uint32_t base_clock, uint32_t target_rate)
{
    uint32_t divider = get_clock_divider(base_clock, target_rate);

    while ((EMMC->status & (EMMC_STATUS_CMD_INHIBIT | EMMC_STATUS_DAT_INHIBIT)))
    {
        delay_ms(1);
    }

    uint32_t c1 = EMMC->control[1] & ~EMMC_CTRL1_CLK_ENABLE;

    EMMC->control[1] = c1;

    delay_ms(3);

    // Clear old divider bits (15:6) and set new divider
    c1 &= ~0xFFC0;
    EMMC->control[1] = c1 | divider;

    delay_ms(3);

    EMMC->control[1] = (c1 | divider) | EMMC_CTRL1_CLK_ENABLE;

    delay_ms(3);

    return true;
}

bool emmc_setup_clock()
{
    EMMC->control2 = 0;

    uint32_t rate = mailbox_clock_rate(CT_EMMC);
    device.base_clock = rate;  // Store for switch_clock_rate later

    uint32_t n = EMMC->control[1];
    n |= EMMC_CTRL1_CLK_INT_EN;
    n |= get_clock_divider(rate, SD_CLOCK_ID);
    n &= ~(0xf << 16);
    n |= (11 << 16);

    EMMC->control[1] = n;

    if (!wait_reg_mask(&EMMC->control[1], EMMC_CTRL1_CLK_STABLE, true, 2000))
    {
        printk("Current SD CLOCK Status: %p\n", EMMC->control);
        printk("Current EMMC->control[1]: %x\n", EMMC->control[1]);
        printk("EMMC_ERR: SD CLOCK NOT STABLE\n");
        return false;
    }

    delay_ms(30);

    // enabling the clock
    EMMC->control[1] |= 4;

    delay_ms(30);

    return true;
}