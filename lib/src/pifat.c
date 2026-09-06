#include "pifat.h"
#include "emmc.h"
#include "mem.h"
#include "cstr.h"
#define DEBUG_ALLOW_PRINT_FAT 1
#define FS_SECTOR_SIZE 512
#define ENABLE_LFN 0

#define ENABLE_FS_CACHE 1

/// begin cache
#define FS_CACHE_SLOTS 2048 * 16 // 16MB cache
#define FS_CACHE_MASK (FS_CACHE_SLOTS - 1)

static uint8_t *fs_cache_data = 0;  // kalloc'd flat buffer: FS_CACHE_SLOTS * 512
static uint32_t *fs_cache_tag = 0;  // kalloc'd: FS_CACHE_SLOTS * 4
static uint8_t *fs_cache_valid = 0; // kalloc'd: FS_CACHE_SLOTS * 1

/// end cache

void fat32_debug_dump_buffer(uint32_t offset, uint8_t *buf)
{
    for (int i = 0; i < FS_SECTOR_SIZE; i++)
    {
        if (i % 16 == 0)
            if (DEBUG_ALLOW_PRINT_FAT)
                printk("\n%x: ", i + offset);
        if (DEBUG_ALLOW_PRINT_FAT)
            printk("%x ", (unsigned char)buf[i]);
    }
    if (DEBUG_ALLOW_PRINT_FAT)
        printk("\n");
}
void fs_read_sector(uint8_t *buf, uint32_t sector)
{
    if (ENABLE_FS_CACHE)
    {
        uint32_t tag = fs_cache_tag[sector & FS_CACHE_MASK];
        if (fs_cache_valid[sector & FS_CACHE_MASK] && tag == sector)
        {
            memcpy(buf, fs_cache_data + ((sector & FS_CACHE_MASK) * FS_SECTOR_SIZE), FS_SECTOR_SIZE);
            return;
        }
    }
    emmc_seek(sector * FS_SECTOR_SIZE);
    emmc_read(buf, FS_SECTOR_SIZE);
    if (ENABLE_FS_CACHE)
    {
        memcpy(fs_cache_data + ((sector & FS_CACHE_MASK) * FS_SECTOR_SIZE), buf, FS_SECTOR_SIZE);
        fs_cache_tag[sector & FS_CACHE_MASK] = sector;
        fs_cache_valid[sector & FS_CACHE_MASK] = 1;
    }
}
void fs_write_sector(uint8_t *buf, uint32_t sector)
{
    if (ENABLE_FS_CACHE)
    {
        uint32_t tag = fs_cache_tag[sector & FS_CACHE_MASK];
        if (fs_cache_valid[sector & FS_CACHE_MASK] && tag == sector)
            fs_cache_valid[sector & FS_CACHE_MASK] = 0;
    }
    emmc_seek(sector * FS_SECTOR_SIZE);
    emmc_write(buf, FS_SECTOR_SIZE);
}

void fs_read_mult_sector(uint8_t *buf, uint32_t sector, uint32_t count)
{
    if (ENABLE_FS_CACHE)
    {
        for (uint32_t s = 0; s < count; s++)
        {
            uint32_t cur_sector = sector + s;
            uint32_t tag = fs_cache_tag[cur_sector & FS_CACHE_MASK];
            if (fs_cache_valid[cur_sector & FS_CACHE_MASK] && tag == cur_sector)
            {
                memcpy(buf + (s * FS_SECTOR_SIZE), fs_cache_data + ((cur_sector & FS_CACHE_MASK) * FS_SECTOR_SIZE), FS_SECTOR_SIZE);
            }
            else
            {
                emmc_seek(cur_sector * FS_SECTOR_SIZE);
                emmc_read(buf + (s * FS_SECTOR_SIZE), FS_SECTOR_SIZE);
                memcpy(fs_cache_data + ((cur_sector & FS_CACHE_MASK) * FS_SECTOR_SIZE), buf + (s * FS_SECTOR_SIZE), FS_SECTOR_SIZE);
                fs_cache_tag[cur_sector & FS_CACHE_MASK] = cur_sector;
                fs_cache_valid[cur_sector & FS_CACHE_MASK] = 1;
            }
        }
    }
    else
    {
        emmc_seek(sector * FS_SECTOR_SIZE);
        emmc_read(buf, FS_SECTOR_SIZE * count);
    }
}
mbr_table_t *fs_parse_mbr_section(uint8_t *buf)
{
    mbr_table_t *t = (mbr_table_t *)(buf + 446);
    if (t->sig[0] != 0x55 || t->sig[1] != 0xaa)
        return 0;
    return t;
}
fat32_bpb *fat32_parse_bpb_section(uint8_t *buf)
{
    fat32_bpb *t = (fat32_bpb *)buf;
    if ((t->bytes_per_sec != FS_SECTOR_SIZE) || (t->root_ent_cnt != 0) || (t->fat_sz16 != 0) || (t->fat_sz32 == 0) || (t->root_clus < 2))
        return 0;
    if (buf[510] != 0x55 || buf[511] != 0xaa)
        return 0;
    return t;
}
uint32_t fat32_cluster_to_lba(fat_meta_t *fat_meta_t, uint32_t clus)
{
    return fat_meta_t->data_start_lba + (clus - 2) * fat_meta_t->sec_per_clus;
}
uint32_t fat32_fat_next_cluster(fat_meta_t *fat_meta_t, uint32_t clus)
{
    uint32_t fat_offset = clus * 4;
    uint32_t fat_sector = fat_meta_t->fat_start_lba + (fat_offset / fat_meta_t->bytes_per_sec);
    uint32_t fat_index = fat_offset % fat_meta_t->bytes_per_sec;

    uint8_t buf[FS_SECTOR_SIZE];
    fs_read_sector(buf, fat_sector);

    uint32_t val = read_u32(buf + fat_index);
    return val & 0x0FFFFFFF;
}
fat32_file_list_entry_t *fat32_list_data_lba(fat_meta_t *meta, uint32_t dlba)
{
    uint8_t buf[FS_SECTOR_SIZE];
    fat32_file_list_entry_t *head = kalloc(sizeof(fat32_file_list_entry_t));
    INIT_LIST_HEAD(&head->list);
    uint32_t total_files = 0;
    fat_lfn_entry_t *lfn_entries[20];
    uint32_t lfn_count = 0;
    for (uint32_t s = 0; s < meta->sec_per_clus; s++)
    {
        fs_read_sector(buf, dlba + s);
        for (int off = 0; off < FS_SECTOR_SIZE; off += 32)
        {
            fat_dirent_t *e = (fat_dirent_t *)(buf + off);
            if (e->name[0] == 0x00)
            {
                // if (DEBUG_ALLOW_PRINT_FAT) printk("Total files in data LBA %x: %u\n\n\n", dlba, total_files);
                return head;
            }
            if (e->name[0] == 0xE5)
            {
                lfn_count = 0;
                continue;
            }
            if (e->attr == FAT_ATTR_LFN)
            {
                lfn_entries[lfn_count++] = (fat_lfn_entry_t *)e;
                continue;
            }
            if (e->attr & FAT_ATTR_VOLUMEID)
            {
                lfn_count = 0;
                continue;
            }
            // char name[12];
            // memcpy(name, e->name, 11);
            // name[11] = '\0';
            // if (DEBUG_ALLOW_PRINT_FAT) printk("File: %s Attr=%x Clus=%x Size=%u\n",
            //        name,
            //        e->attr,
            //        ((uint32_t)e->fst_clus_hi << 16) | e->fst_clus_lo,
            //        e->file_size);
            fat32_file_list_entry_t *entry = kalloc(sizeof(fat32_file_list_entry_t));
            if (ENABLE_LFN)
                for (uint32_t i = 0; i < lfn_count; i++)
                {
                    entry->lfn_entries[i] = kalloc(sizeof(fat_lfn_entry_t));
                    memcpy(entry->lfn_entries[i], lfn_entries[i], sizeof(fat_lfn_entry_t));
                }
            entry->lfn_count = lfn_count;
            memcpy(&entry->dirent, e, sizeof(fat_dirent_t));
            list_add_tail(&entry->list, &head->list);
            total_files++;
            lfn_count = 0;
        }
    }
    // if (DEBUG_ALLOW_PRINT_FAT) printk("Total files in data LBA %x: %u\n\n\n", dlba, total_files);
    return head;
}

uint8_t *fat32_read_file(fat_meta_t *meta, uint32_t start_clus, uint32_t file_size)
{
    uint8_t *file_data = kalloc(file_size);
    uint32_t clus = start_clus;
    uint32_t bytes_read = 0;
    uint32_t MAX_READ_SECTOR_COUNT = meta->sec_per_clus; // at most read in one cluster at a time
    uint8_t buf[FS_SECTOR_SIZE * MAX_READ_SECTOR_COUNT]; // Read up to 4 sectors at a time
    if (DEBUG_ALLOW_PRINT_FAT)
        printk("Reading file of size %u bytes starting at cluster %x\n", file_size, start_clus);
    // if (DEBUG_ALLOW_PRINT_FAT)
    // printk("Sectors per cluster: %u, Bytes per sector: %u\n", meta->sec_per_clus, meta->bytes_per_sec);
    while (clus < 0x0FFFFFF8 && bytes_read < file_size)
    {
        uint32_t dlba = fat32_cluster_to_lba(meta, clus);
        uint32_t read_for_this_clus = 0;
        while (read_for_this_clus < meta->sec_per_clus * meta->bytes_per_sec && bytes_read < file_size)
        {
            fs_read_mult_sector(buf, dlba + (read_for_this_clus / meta->bytes_per_sec), MAX_READ_SECTOR_COUNT);
            uint32_t to_copy = meta->bytes_per_sec * MAX_READ_SECTOR_COUNT;
            // if (DEBUG_ALLOW_PRINT_FAT) printk("EMMC READ RETURNED %u bytes from sector %x\n", to_copy, dlba + (read_for_this_clus / meta->bytes_per_sec));
            if (bytes_read + to_copy > file_size)
                to_copy = file_size - bytes_read;
            memcpy(&file_data[bytes_read], buf, to_copy);
            // for (uint32_t i = 0; i < to_copy; i++)
            //     file_data[bytes_read + i] = buf[i];
            // if (DEBUG_ALLOW_PRINT_FAT) printk("%c", buf[i]);
            read_for_this_clus += to_copy;
            bytes_read += to_copy;
        }
        // if (DEBUG_ALLOW_PRINT_FAT) printk("Cluster: %x, data LBA: %x\n", clus, dlba);
        clus = fat32_fat_next_cluster(meta, clus);
    }
    return file_data;
}

uint32_t fat32_read_file_with_offset(fat_meta_t *meta, uint32_t start_clus, uint8_t *output, uint32_t size, uint32_t offset)
{
    while (offset >= meta->sec_per_clus * meta->bytes_per_sec)
    {
        start_clus = fat32_fat_next_cluster(meta, start_clus);
        offset -= meta->sec_per_clus * meta->bytes_per_sec;
    }
    uint32_t MAX_READ_SECTOR_COUNT = meta->sec_per_clus; // at most read in one cluster at a time
    uint32_t clus = start_clus;
    uint32_t bytes_read = 0;
    uint8_t buf[FS_SECTOR_SIZE * MAX_READ_SECTOR_COUNT]; // Read up to 4 sectors at a time
    while (clus < 0x0FFFFFF8 && bytes_read < size)
    {
        uint32_t dlba = fat32_cluster_to_lba(meta, clus);
        fs_read_mult_sector(buf, dlba, MAX_READ_SECTOR_COUNT);
        uint32_t to_copy = meta->bytes_per_sec * MAX_READ_SECTOR_COUNT - offset;
        if (bytes_read + to_copy > size)
            to_copy = size - bytes_read;
        // Adjust by final remaining offset
        memcpy(&output[bytes_read], buf + offset, to_copy);
        bytes_read += to_copy;
        offset = 0; // Only the first cluster may have an offset
        clus = fat32_fat_next_cluster(meta, clus);
    }
    return bytes_read;
}
fat32_file_list_entry_t *fat32_list_fat(fat_meta_t *meta, uint32_t start_clus)
{
    fat32_file_list_entry_t *head = kalloc(sizeof(fat32_file_list_entry_t));
    INIT_LIST_HEAD(&head->list);
    uint32_t clus = start_clus;
    while (clus < 0x0FFFFFF8)
    {
        uint32_t dlba = fat32_cluster_to_lba(meta, clus);
        // if (DEBUG_ALLOW_PRINT_FAT) printk("Cluster: %x, data LBA: %x\n", clus, dlba);
        fat32_file_list_entry_t *entries = fat32_list_data_lba(meta, dlba);
        list_splice_tail(&entries->list, &head->list);
        clus = fat32_fat_next_cluster(meta, clus);
    }
    return head;
}

fat_meta_t *fat32_init()
{
    if (ENABLE_FS_CACHE && !fs_cache_data)
    {
        fs_cache_data = kalloc(FS_CACHE_SLOTS * FS_SECTOR_SIZE);
        fs_cache_tag = kalloc(FS_CACHE_SLOTS * sizeof(uint32_t));
        fs_cache_valid = kalloc(FS_CACHE_SLOTS);
        memset(fs_cache_valid, 0, FS_CACHE_SLOTS);
    }
    uint8_t buf[512];
    uint8_t buf2[512];
    fs_read_sector(buf, 0);
    mbr_table_t *t = fs_parse_mbr_section(buf);
    if (!t)
    {
        if (DEBUG_ALLOW_PRINT_FAT)
            printk("THIS IS NOT MBR\n");
        return 0;
    }
    for (int pt = 0; pt < 4; pt++)
    {
        if (t->part[pt].type == 0x0B)
        {
            if (DEBUG_ALLOW_PRINT_FAT)
                printk("Part %d is CHS, starting: %x LBA, length: %x sectors\n", pt, t->part[pt].lba_first, t->part[pt].sectors);
            fs_read_sector(buf2, t->part[pt].lba_first);
            fat32_bpb *bpb = fat32_parse_bpb_section(buf2);
            if (!bpb)
            {
                if (DEBUG_ALLOW_PRINT_FAT)
                    printk("THIS IS NOT BPB\n");
                return 0;
            }
            if (DEBUG_ALLOW_PRINT_FAT)
                printk("Number of fats: %u\n", bpb->num_fats);
            if (DEBUG_ALLOW_PRINT_FAT)
                printk("FAT size (sectors): %u\n", bpb->fat_sz32);
            if (DEBUG_ALLOW_PRINT_FAT)
                printk("Root cluster: %u\n", bpb->root_clus);
            if (DEBUG_ALLOW_PRINT_FAT)
                printk("t->part[pt].lba_first = %x\n", t->part[pt].lba_first);
            fat_meta_t *f = kalloc(sizeof(fat_meta_t));
            f->fat_start_lba = t->part[pt].lba_first + bpb->rsvd_sec_cnt;
            f->data_start_lba = f->fat_start_lba + bpb->num_fats * bpb->fat_sz32;
            f->bytes_per_sec = bpb->bytes_per_sec;
            f->sec_per_clus = bpb->sec_per_clus;
            memcpy(&f->bpb, bpb, sizeof(fat32_bpb));
            memcpy(&f->mbr, t, sizeof(mbr_table_t));
            return f;
        }
        else if (t->part[pt].type == 0x0C)
        {
            if (DEBUG_ALLOW_PRINT_FAT)
                printk("Part %d is LBA, starting: %d LBA, length: %d sectors\nThis is not supported yet", pt, t->part[pt].lba_first, t->part[pt].sectors);
        }
    }
    return 0;
}

int fat32_find_free_clusters(fat_meta_t *meta, uint32_t *free_clusters, uint32_t number_of_clusters)
{
    uint32_t found_clusters = 0;
    uint32_t total_clusters = (meta->bpb.tot_sec32 - (meta->data_start_lba - (meta->bpb.hidd_sec + meta->bpb.rsvd_sec_cnt + meta->bpb.num_fats * meta->bpb.fat_sz32))) / meta->sec_per_clus;
    uint8_t cache_buf[FS_SECTOR_SIZE];
    uint32_t cache_sector = 0xFFFFFFFF;
    for (uint32_t clus = 2; clus < total_clusters + 2; clus++)
    {
        uint32_t fat_offset = clus * 4;
        uint32_t fat_sector = meta->fat_start_lba + (fat_offset / meta->bytes_per_sec);
        uint32_t fat_index = fat_offset % meta->bytes_per_sec;

        if (fat_sector != cache_sector)
        {
            fs_read_sector(cache_buf, fat_sector);
            cache_sector = fat_sector;
        }

        uint32_t val = read_u32(cache_buf + fat_index);
        val &= 0x0FFFFFFF;

        if (val == 0x00000000) // Free cluster
        {
            free_clusters[found_clusters] = clus;
            found_clusters++;
            if (found_clusters == number_of_clusters)
                return 0; // Success
        }
    }
    return -1; // Not enough free clusters
}

uint32_t fat32_give_me_next_free_cluster_please(fat_meta_t *meta, uint32_t current_clus)
{
    uint32_t total_clusters = (meta->bpb.tot_sec32 - (meta->data_start_lba - (meta->bpb.hidd_sec + meta->bpb.rsvd_sec_cnt + meta->bpb.num_fats * meta->bpb.fat_sz32))) / meta->sec_per_clus;
    uint8_t cache_buf[FS_SECTOR_SIZE];
    uint32_t cache_sector = 0xFFFFFFFF;
    for (uint32_t clus = 2; clus < total_clusters + 2; clus++)
    {
        uint32_t fat_offset = clus * 4;
        uint32_t fat_sector = meta->fat_start_lba + (fat_offset / meta->bytes_per_sec);
        uint32_t fat_index = fat_offset % meta->bytes_per_sec;

        if (fat_sector != cache_sector)
        {
            fs_read_sector(cache_buf, fat_sector);
            cache_sector = fat_sector;
        }

        uint32_t val = read_u32(cache_buf + fat_index);
        val &= 0x0FFFFFFF;

        if (val == 0x00000000) // Free cluster
        {
            // clus is the next cluster
            // update current to point to this new cluster
            if (current_clus != 0)
                fat32_write_fat_cluster_entry(meta, current_clus, clus);
            fat32_write_fat_cluster_entry(meta, clus, 0x0FFFFFFF); // EoC
            return clus;
        }
    }
    return -1; // Not enough free clusters
}

int fat32_write_fat_cluster_entry(fat_meta_t *meta, uint32_t clus, uint32_t value)
{
    // if (DEBUG_ALLOW_PRINT_FAT) printk("Writing FAT entry: Cluster %u -> Value %x\n", clus, value);
    uint32_t fat_offset = clus * 4;
    uint32_t fat_sector = meta->fat_start_lba + (fat_offset / meta->bytes_per_sec);
    uint32_t fat_index = fat_offset % meta->bytes_per_sec;

    uint8_t buf[FS_SECTOR_SIZE];
    fs_read_sector(buf, fat_sector);

    uint32_t old = read_u32(buf + fat_index);
    write_u32(buf + fat_index, (old & 0xF0000000) | (value & 0x0FFFFFFF));

    fs_write_sector(buf, fat_sector);

    return 0;
}

int fat32_free_cluster_chain(fat_meta_t *meta, uint32_t start_clus)
{
    uint32_t clus = start_clus;
    while (clus < 0x0FFFFFF8 && clus != 0)
    {
        uint32_t next_clus = fat32_fat_next_cluster(meta, clus);
        fat32_write_fat_cluster_entry(meta, clus, 0x00000000); // Mark as free
        clus = next_clus;
    }
    return 0;
}

int fat32_write_create_directory_entry(fat_meta_t *meta, uint32_t dir_clus, fat_dirent_t *new_entry)
{
    uint32_t clus = dir_clus;
    uint32_t last_clus = dir_clus;
    while (clus < 0x0FFFFFF8 && clus != 0)
    {
        // check every cluster in the directory
        uint32_t dlba = fat32_cluster_to_lba(meta, clus);
        for (uint32_t s = 0; s < meta->sec_per_clus; s++)
        {
            // check every sector in the cluster
            uint8_t buf[FS_SECTOR_SIZE];
            fs_read_sector(buf, dlba + s);
            for (int off = 0; off < FS_SECTOR_SIZE; off += 32)
            {
                fat_dirent_t *e = (fat_dirent_t *)(buf + off);
                if (e->name[0] == 0x00 || e->name[0] == 0xE5)
                {
                    // Found free entry
                    memcpy(e, new_entry, sizeof(fat_dirent_t));
                    fs_write_sector(buf, dlba + s);
                    return 0;
                }
            }
        }
        last_clus = clus;
        clus = fat32_fat_next_cluster(meta, clus);
    }
    clus = last_clus;
    // No free entry found... we will need to extend the directory...
    if (DEBUG_ALLOW_PRINT_FAT)
        printk("Risky new logic: extending directory cluster chain...\n");
    uint32_t *free_cluster = kalloc(sizeof(uint32_t));
    if (fat32_find_free_clusters(meta, free_cluster, 1) != 0)
    {
        if (DEBUG_ALLOW_PRINT_FAT)
            printk("No free cluster available to extend directory!\n");
        return 2;
    }
    // Link the new cluster to the directory's cluster chain
    fat32_write_fat_cluster_entry(meta, clus, *free_cluster);
    fat32_write_fat_cluster_entry(meta, *free_cluster, 0x0FFFFFFF); // End of Chain
    // Clear the new cluster
    uint32_t new_dlba = fat32_cluster_to_lba(meta, *free_cluster);
    uint8_t clear_buf[FS_SECTOR_SIZE];
    memset(clear_buf, 0, FS_SECTOR_SIZE);
    for (uint32_t s = 0; s < meta->sec_per_clus; s++)
    {
        fs_write_sector(clear_buf, new_dlba + s);
    }
    // Now write the new entry into the new cluster
    fat_dirent_t *e = (fat_dirent_t *)clear_buf;
    memcpy(e, new_entry, sizeof(fat_dirent_t));
    fs_write_sector(clear_buf, new_dlba);
    return 0;
}

int fat32_write_file_create(fat_meta_t *meta, uint32_t dir_clus, uint8_t *data, uint32_t size, uint8_t *filename)
{
    uint32_t number_of_clusters = (size + (meta->sec_per_clus * meta->bytes_per_sec) - 1) / (meta->sec_per_clus * meta->bytes_per_sec);
    if (number_of_clusters == 0)
        number_of_clusters = 1; // Allocate at least one cluster for empty file to have a directory entry
    if (DEBUG_ALLOW_PRINT_FAT)
        printk("Number of clusters needed: %u\n", number_of_clusters);
    uint32_t *free_clusters = kalloc(sizeof(uint32_t) * number_of_clusters);
    // Find free clusters
    if (fat32_find_free_clusters(meta, free_clusters, number_of_clusters) != 0)
    {
        if (DEBUG_ALLOW_PRINT_FAT)
            printk("Not enough free clusters available!\n");
        return 2;
    }
    // Write FAT chain
    for (uint32_t i = 0; i < number_of_clusters; i++)
    {
        if (DEBUG_ALLOW_PRINT_FAT)
            printk("Cluster %u: %d, %p\n", i, free_clusters[i], &free_clusters[i]);
        if (i == number_of_clusters - 1)
            fat32_write_fat_cluster_entry(meta, free_clusters[i], 0x0FFFFFFF); // End of Chain
        else
            fat32_write_fat_cluster_entry(meta, free_clusters[i], free_clusters[i + 1]);
    }
    // Write data to clusters
    uint32_t bytes_written = 0;
    for (uint32_t i = 0; i < number_of_clusters; i++)
    {
        uint32_t dlba = fat32_cluster_to_lba(meta, free_clusters[i]);
        for (uint32_t s = 0; s < meta->sec_per_clus; s++)
        {
            uint8_t buf[FS_SECTOR_SIZE];
            uint32_t to_write = FS_SECTOR_SIZE;
            if (bytes_written + to_write > size)
                to_write = size - bytes_written;
            memset(buf, 0, FS_SECTOR_SIZE);
            memcpy(buf, data + bytes_written, to_write);
            fs_write_sector(buf, dlba + s);
            bytes_written += to_write;
            if (bytes_written >= size)
                break;
        }
    }
    if (bytes_written != size)
    {
        if (DEBUG_ALLOW_PRINT_FAT)
            printk("Error: Written bytes %u does not match file size %u\n", bytes_written, size);
        return 3;
    }
    // Create directory entry
    fat_dirent_t new_entry;
    memset(&new_entry, 0, sizeof(fat_dirent_t));
    memcpy(new_entry.name, filename, 11);
    new_entry.attr = FAT_ATTR_ARCHIVE;
    new_entry.fst_clus_lo = free_clusters[0] & 0xFFFF;
    new_entry.fst_clus_hi = (free_clusters[0] >> 16) & 0xFFFF;
    new_entry.file_size = size;
    return fat32_write_create_directory_entry(meta, dir_clus, &new_entry);
}

int fat32_write_with_offset(fat_meta_t *meta, uint32_t start_clus, uint8_t *input, uint32_t size, uint32_t offset)
{
    // skip forward
    while (offset >= meta->sec_per_clus * meta->bytes_per_sec)
    {
        start_clus = fat32_fat_next_cluster(meta, start_clus);
        offset -= meta->sec_per_clus * meta->bytes_per_sec;
    }
    uint32_t MAX_READ_SECTOR_COUNT = meta->sec_per_clus; // at most read in one cluster at a time
    uint32_t clus = start_clus;
    uint32_t bytes_written = 0;
    uint8_t buf[FS_SECTOR_SIZE * MAX_READ_SECTOR_COUNT];
    // if stuff to overwrite, we first overwrite
    while (clus < 0x0FFFFFF8 && bytes_written < size)
    {
        uint32_t dlba = fat32_cluster_to_lba(meta, clus);
        uint32_t to_copy = meta->bytes_per_sec * MAX_READ_SECTOR_COUNT - offset;
        if (bytes_written + to_copy > size)
            to_copy = size - bytes_written;
        if (offset > 0 || to_copy < meta->bytes_per_sec * MAX_READ_SECTOR_COUNT)
        {
            // Need to read-modify-write the first cluster
            // Last cluster with partial data, need read-modify-write
            fs_read_mult_sector(buf, dlba, MAX_READ_SECTOR_COUNT);
            // Adjust by final remaining offset
            memcpy(buf + offset, &input[bytes_written], to_copy);
        }
        else
        {
            // No offset, nuke the whole cluster
            memcpy(buf, &input[bytes_written], to_copy);
        }
        // write back (sect by sect)
        for (uint32_t s = 0; s < MAX_READ_SECTOR_COUNT; s++)
        {
            fs_write_sector(buf + s * FS_SECTOR_SIZE, dlba + s);
        }
        bytes_written += to_copy;
        offset = 0; // Only the first cluster may have an offset
        uint32_t tmp = clus;
        clus = fat32_fat_next_cluster(meta, clus); // not yet, continue
        // see if we are at the tail
        if (clus >= 0x0FFFFFF8 && bytes_written < size)
        {
            // well, we will need new clusters
            clus = fat32_give_me_next_free_cluster_please(meta, tmp);
            if (clus >= 0x0FFFFFF8)
            {
                if (DEBUG_ALLOW_PRINT_FAT)
                    printk("No free cluster available to extend file!\n");
                return bytes_written; // Return how much we managed to write
            }
        }
    }
    return bytes_written;
}

int fat32_unlink_file(fat_meta_t *meta, uint32_t dir_clus, char *filename)
{
    uint32_t clus = dir_clus;
    while (clus < 0x0FFFFFF8 && clus != 0)
    {
        // check every cluster in the directory
        uint32_t dlba = fat32_cluster_to_lba(meta, clus);
        for (uint32_t s = 0; s < meta->sec_per_clus; s++)
        {
            // check every sector in the cluster
            uint8_t buf[FS_SECTOR_SIZE];
            fs_read_sector(buf, dlba + s);
            for (int off = 0; off < FS_SECTOR_SIZE; off += 32)
            {
                fat_dirent_t *e = (fat_dirent_t *)(buf + off);
                if (e->name[0] == 0x00)
                {
                    return -1; // End of directory entries
                }
                if (e->name[0] == 0xE5)
                    continue; // Already deleted
                if (strn_eq((char *)e->name, filename, 11))
                {
                    // Found the file to delete
                    uint32_t start_clus = ((uint32_t)e->fst_clus_hi << 16) | e->fst_clus_lo;
                    fat32_free_cluster_chain(meta, start_clus);
                    e->name[0] = 0xE5; // Mark as deleted
                    fs_write_sector(buf, dlba + s);
                    return 0; // Success
                }
            }
        }
        clus = fat32_fat_next_cluster(meta, clus);
    }
    return -1; // File not found
}

int fat32_update_dirent(fat_meta_t *meta, uint32_t dir_clus, char *filename, fat_dirent_t *new_diren)
{
    uint32_t clus = dir_clus;
    while (clus < 0x0FFFFFF8 && clus != 0)
    {
        // check every cluster in the directory
        uint32_t dlba = fat32_cluster_to_lba(meta, clus);
        for (uint32_t s = 0; s < meta->sec_per_clus; s++)
        {
            // check every sector in the cluster
            uint8_t buf[FS_SECTOR_SIZE];
            fs_read_sector(buf, dlba + s);
            for (int off = 0; off < FS_SECTOR_SIZE; off += 32)
            {
                fat_dirent_t *e = (fat_dirent_t *)(buf + off);
                if (e->name[0] == 0x00)
                {
                    return -1; // End of directory entries
                }
                if (e->name[0] == 0xE5)
                    continue; // Deleted
                if (strn_eq((char *)e->name, filename, 11))
                {
                    // Found the file to update
                    memcpy(e, new_diren, sizeof(fat_dirent_t));
                    fs_write_sector(buf, dlba + s);
                    return 0; // Success
                }
            }
        }
        clus = fat32_fat_next_cluster(meta, clus);
    }
    return -1; // File not found
}

void free_fat32_file_list_entry_t_list(fat32_file_list_entry_t *head)
{
    struct list_head *pos, *tmp;
    pos = head->list.next;
    while (pos != &head->list.next)
    {
        fat32_file_list_entry_t *entry = container_of(pos, fat32_file_list_entry_t, list);
        pos = pos->next;
        kfree(entry);
    }
    kfree(head);
}
