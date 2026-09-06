#ifndef PIFS_H
#define PIFS_H
#include "types.h"
#include "linkedlist.h"

#define FAT_ATTR_READONLY 0x01
#define FAT_ATTR_HIDDEN 0x02
#define FAT_ATTR_SYSTEM 0x04
#define FAT_ATTR_VOLUMEID 0x08
#define FAT_ATTR_DIRECTORY 0x10
#define FAT_ATTR_ARCHIVE 0x20
#define FAT_ATTR_LFN 0x0F

typedef struct __attribute__((packed))
{
    uint8_t status;       // 0x80 = bootable, 0x00 = normal
    uint8_t chs_first[3]; // obsolete, ignore
    uint8_t type;         // partition type
    uint8_t chs_last[3];  // obsolete, ignore
    uint32_t lba_first;   // starting LBA
    uint32_t sectors;     // length in sectors
} mbr_part_entry_t;

typedef struct __attribute__((packed))
{
    mbr_part_entry_t part[4];
    uint8_t sig[2];
} mbr_table_t;

typedef struct __attribute__((packed))
{
    uint8_t jmp[3];         // 0x00
    char oem[8];            // 0x03
    uint16_t bytes_per_sec; // 0x0B (must be 512)
    uint8_t sec_per_clus;   // 0x0D (power of 2)
    uint16_t rsvd_sec_cnt;  // 0x0E
    uint8_t num_fats;       // 0x10
    uint16_t root_ent_cnt;  // 0x11 (must be 0 for FAT32)
    uint16_t tot_sec16;     // 0x13 (must be 0 for FAT32)
    uint8_t media;          // 0x15
    uint16_t fat_sz16;      // 0x16 (must be 0 for FAT32)
    uint16_t sec_per_trk;   // 0x18 (ignore)
    uint16_t num_heads;     // 0x1A (ignore)
    uint32_t hidd_sec;      // 0x1C
    uint32_t tot_sec32;     // 0x20

    // FAT32 extended BPB
    uint32_t fat_sz32;    // 0x24
    uint16_t ext_flags;   // 0x28
    uint16_t fs_ver;      // 0x2A
    uint32_t root_clus;   // 0x2C
    uint16_t fs_info;     // 0x30
    uint16_t bk_boot_sec; // 0x32
} fat32_bpb;

typedef struct
{
    uint32_t bytes_per_sec;
    uint32_t sec_per_clus;
    uint32_t fat_start_lba;
    uint32_t data_start_lba;
    fat32_bpb bpb;
    mbr_table_t mbr;
} fat_meta_t;

typedef struct __attribute__((packed))
{
    uint8_t name[11];       // 8.3 name, space-padded
    uint8_t attr;           // attribute flags
    uint8_t ntres;          // NT reserved
    uint8_t crt_time_tenth; // creation time (10ms units)
    uint16_t crt_time;      // creation time
    uint16_t crt_date;      // creation date
    uint16_t lst_acc_date;  // last access date
    uint16_t fst_clus_hi;   // high 16 bits of first cluster
    uint16_t wrt_time;      // write time
    uint16_t wrt_date;      // write date
    uint16_t fst_clus_lo;   // low 16 bits of first cluster
    uint32_t file_size;     // file size in bytes
} fat_dirent_t;

// One Long File Name directory entry (32 bytes)
typedef struct __attribute__((packed))
{
    uint8_t ord;          // 0x00: Sequence number (1..n, last has 0x40)
    uint16_t name1[5];    // 0x01–0x0A: First 5 UTF-16 chars
    uint8_t attr;         // 0x0B: Attribute = 0x0F
    uint8_t type;         // 0x0C: Type (always 0x00)
    uint8_t checksum;     // 0x0D: Checksum of short name
    uint16_t name2[6];    // 0x0E–0x19: Next 6 UTF-16 chars
    uint16_t fst_clus_lo; // 0x1A–0x1B: Always 0x0000
    uint16_t name3[2];    // 0x1C–0x1F: Final 2 UTF-16 chars
} fat_lfn_entry_t;

struct fat32_file_list_entry
{
    fat_dirent_t dirent;
    uint32_t lfn_count;
    fat_lfn_entry_t *lfn_entries[20];
    struct list_head list;
};
typedef struct fat32_file_list_entry fat32_file_list_entry_t;

void fat32_debug_dump_buffer(uint32_t offset, uint8_t *buf);
void fs_read_sector(uint8_t *buf, uint32_t sector);
mbr_table_t *fs_parse_mbr_section(uint8_t *buf);
fat32_bpb *fat32_parse_bpb_section(uint8_t *buf);
uint32_t fat32_cluster_to_lba(fat_meta_t *fat_meta_t, uint32_t clus);
uint32_t fat32_fat_next_cluster(fat_meta_t *fat_meta_t, uint32_t clus);
fat32_file_list_entry_t *fat32_list_data_lba(fat_meta_t *meta, uint32_t dlba);
uint8_t *fat32_read_file(fat_meta_t *meta, uint32_t start_clus, uint32_t file_size);
uint32_t fat32_read_file_with_offset(fat_meta_t *meta, uint32_t start_clus, uint8_t *output, uint32_t size, uint32_t offset);
fat32_file_list_entry_t *fat32_list_fat(fat_meta_t *meta, uint32_t start_clus);
fat_meta_t *fat32_init();
int fat32_write_file_create(fat_meta_t *meta, uint32_t dir_clus, uint8_t *data, uint32_t size, uint8_t *filename);
int fat32_unlink_file(fat_meta_t *meta, uint32_t dir_clus, char *filename);
void free_fat32_file_list_entry_t_list(fat32_file_list_entry_t *head);
uint32_t fat32_give_me_next_free_cluster_please(fat_meta_t *meta, uint32_t current_clus);
int fat32_write_with_offset(fat_meta_t *meta, uint32_t start_clus, uint8_t *output, uint32_t size, uint32_t offset);
int fat32_update_dirent(fat_meta_t *meta, uint32_t dir_clus, char *filename, fat_dirent_t *new_diren);

#endif
