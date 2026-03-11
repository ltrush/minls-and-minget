/* reads data from a single zone into buf, handles holes */
int read_zone(FILE *fp, uint32_t zone_num, long fs_base, uint32_t zonesize,
              uint8_t *buf, int bytes_remaining) {
    int to_read = MIN(zonesize, bytes_remaining);
    if (zone_num == 0) {
        memset(buf, 0, to_read);
    } else {
        fseek(fp, fs_base + (long)zone_num * zonesize, SEEK_SET);
        fread(buf, 1, to_read, fp);
    }
    return to_read;
}

/* reads a zone full of pointers into table, returns ptr count */
int read_indirect(FILE *fp, uint32_t zone_num, long fs_base,
                  uint32_t zonesize, uint32_t *table) {
    int ptrs_per_zone = zonesize / sizeof(uint32_t);
    fseek(fp, fs_base + (long)zone_num * zonesize, SEEK_SET);
    fread(table, sizeof(uint32_t), ptrs_per_zone, fp);
    return ptrs_per_zone;
}

void read_file(FILE *fp, struct inode *inode, long fs_base,
               uint32_t zonesize, uint8_t *buf) {
    int remaining = inode->size;
    int ptrs_per_zone = zonesize / sizeof(uint32_t);
    uint8_t *ptr = buf;

    /* direct zones */
    for (int i = 0; i < DIRECT_ZONES && remaining > 0; i++) {
        int n = read_zone(fp, inode->zone[i], fs_base, zonesize, ptr, remaining);
        ptr += n; remaining -= n;
    }

    /* single indirect */
    if (remaining > 0 && inode->indirect != 0) {
        uint32_t table[ptrs_per_zone];
        read_indirect(fp, inode->indirect, fs_base, zonesize, table);
        for (int i = 0; i < ptrs_per_zone && remaining > 0; i++) {
            int n = read_zone(fp, table[i], fs_base, zonesize, ptr, remaining);
            ptr += n; remaining -= n;
        }
    }

    /* double indirect */
    if (remaining > 0 && inode->two_indirect != 0) {
        uint32_t dbl_table[ptrs_per_zone];
        read_indirect(fp, inode->two_indirect, fs_base, zonesize, dbl_table);
        for (int i = 0; i < ptrs_per_zone && remaining > 0; i++) {
            if (dbl_table[i] == 0) continue;
            uint32_t table[ptrs_per_zone];
            read_indirect(fp, dbl_table[i], fs_base, zonesize, table);
            for (int j = 0; j < ptrs_per_zone && remaining > 0; j++) {
                int n = read_zone(fp, table[j], fs_base, zonesize, ptr, remaining);
                ptr += n; remaining -= n;
            }
        }
    }
}