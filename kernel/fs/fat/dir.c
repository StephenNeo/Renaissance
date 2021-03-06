#include "dir.h"
#include <zjunix/fs/fscache.h>
#include "fat.h"
#include "utils.h"
#include <driver/sd.h>
#include <driver/vga.h>

/* used to find or create a directory entry */
#define DIR_DATA_BUF_NUM 4
FILE dir_find;
extern BUF_512 dir_data_buf[DIR_DATA_BUF_NUM];
extern u32 dir_data_clock_head;
extern struct fs_info fat_info;

/* open directory */
u32 fs_open_dir(FS_FAT_DIR *dir, u8 *filename) {
    u32 index;
    u32 i;

    if (filename[0] != '/')
        goto fs_open_dir_err;

    dir->cur_sector = fs_dataclus2sec(2);
    dir->loc = 0;
    dir->sec = 1;

    //kernel_printf("HELLO1\n");
    /* Open root directory */
    index = fs_read_512(dir_data_buf, dir->cur_sector, &dir_data_clock_head, DIR_DATA_BUF_NUM);
    //kernel_printf("HELLO2\n");
    if (index == 0xffffffff)
        goto fs_open_dir_err;

    /* if not root directory */
    if (filename[1] != 0) {
        for (i = 0; i < 256; i++)
            dir_find.path[i] = 0;
        for (i = 0; i < 256 && filename[i] != 0; i++)
            dir_find.path[i] = filename[i];

        kernel_printf("dir_find.path %s\n", dir_find.path);

        if (fs_find(&dir_find) == 1)
            goto fs_open_dir_err;

         kernel_printf("Find sth\n");
        /* If file not exists */
        if (dir_find.dir_entry_pos == 0xFFFFFFFF)
            goto fs_open_dir_err;

        /* If not a sub directory */
        if ((dir_find.entry.data[11] & 0x10) == 0)
            goto fs_open_dir_err;

        dir->cur_sector = fs_dataclus2sec(get_start_cluster(&dir_find));

        /* open first sector */
        index = fs_read_512(dir_data_buf, dir->cur_sector, &dir_data_clock_head, DIR_DATA_BUF_NUM);
         kernel_printf("Open first sector\n");
        if (index == 0xffffffff)
            goto fs_open_dir_err;
    }

    return 0;

fs_open_dir_err:
    return 1;
}

/* read dir */
u32 fs_read_dir(FS_FAT_DIR *dir, u8 *buf) {
    u32 sec;
    u32 i;
    u32 index;
    u32 k;
    u32 next_clus;

    index = fs_read_512(dir_data_buf, dir->cur_sector, &dir_data_clock_head, DIR_DATA_BUF_NUM);
    if (index == 0xffffffff)
        goto fs_read_dir_err;
    //kernel_printf("I am in fs_read_dir\n");
    while (1) {
        for (sec = dir->sec; sec <= fat_info.BPB.attr.sectors_per_cluster; sec++) {
            /* Find directory entry in current cluster */
            for (i = dir->loc; i < 512; i += 32) {
                if (*(dir_data_buf[index].buf + i) == 0)
                    goto after_fs_read_dir_nomore;

                /* Ignore long path and deleted file */
                if (*(dir_data_buf[index].buf + i) != 0xE5 && (*(dir_data_buf[index].buf + i + 11) & 0x08) == 0) {
                    

                    for (k = 0; k < 32; k++)
                        buf[k] = *(dir_data_buf[index].buf + i + k);
                    dir->loc = i + 32;
                    goto after_fs_read_dir;
                }
            }
            /* next sector in current cluster */
            if (sec < fat_info.BPB.attr.sectors_per_cluster) {
                dir->sec = sec + 1;
                dir->loc = 0;
                dir->cur_sector = dir_data_buf[index].cur + 1;

                index = fs_read_512(dir_data_buf, dir->cur_sector, &dir_data_clock_head, DIR_DATA_BUF_NUM);
                if (index == 0xffffffff)
                    goto fs_read_dir_err;
            } 
            else 
            {
                /* Read next cluster of current directory */
                u32 tmp = fs_sec2dataclus(dir_data_buf[index].cur - fat_info.BPB.attr.sectors_per_cluster + 1);
                if (get_fat_entry_value(tmp, &next_clus) == 1)
                    goto fs_read_dir_err;

                if (next_clus <= fat_info.total_data_clusters + 1) {
                    dir->sec = 1;
                    dir->loc = 0;
                    dir->cur_sector = fs_dataclus2sec(next_clus);

                    index = fs_read_512(dir_data_buf, dir->cur_sector, &dir_data_clock_head, DIR_DATA_BUF_NUM);
                    if (index == 0xffffffff)
                        goto fs_read_dir_err;
                } else
                    goto after_fs_read_dir_nomore;
            }
        }
    }

after_fs_read_dir:
    return 0;
after_fs_read_dir_nomore:
    return 0xffffffff;
fs_read_dir_err:
    return 1;
}