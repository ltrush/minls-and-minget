#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>

/**
 * QUESTIONS:
 * does the order of args matter? for some reason -v using his minls doesnt work
 * should base_offset be a uint32 or uint64
 * question for ourselves: do we like the way print_superblock() works
*/

/**
 * TESTING:
 * ~pn-cs453/demos/minls ~pn-cs453/Given/Asgn5/Images/____
 * */


/* DISK CONSTANTS */
#define SECTOR_SIZE             512
#define MAX_PARTITION_NUM       3
#define NO_PARTITION            -1
#define PART_TAB_START_ADDR     0x1BE   /* relative to start disk/partition */
#define MINIX_PART_TYPE         0x81
#define PART_TAB_SIG_1          0x55    
#define PART_TAB_SIG_2          0xAA
#define SIG_1_OFFSET            510     /* relative to start of boot sector */
#define SIG_2_OFFSET            511     /* relative to start of boot sector */

/* FILESYSTEM CONSTANTS */
#define MINIX_MAGIC_NUM         0x4D5A
#define SUPERBLOCK_OFFSET       1024    /* relative to base of FS */
#define I_BLOCK_OFFSET          2       /* relative to base of FS */
#define DIRECT_ZONES            7
#define INODE_SIZE              64
#define ROOT_INODE_NUM          1
#define MAX_FILENAME_LEN        60

struct __attribute__((packed)) partition_entry {
    uint8_t  bootind;
    uint8_t  start_head;
    uint8_t  start_sec;
    uint8_t  start_cyl;
    uint8_t  type;
    uint8_t  end_head;
    uint8_t  end_sec;
    uint8_t  end_cyl;
    uint32_t lFirst;
    uint32_t size;
};

struct __attribute__((packed)) superblock {
    uint32_t ninodes; /* number of inodes in this filesystem */
    uint16_t pad1; /* make things line up properly */
    int16_t i_blocks; /* # of blocks used by inode bit map */
    int16_t z_blocks; /* # of blocks used by zone bit map */
    uint16_t firstdata; /* number of first data zone */
    int16_t log_zone_size; /* log2 of blocks per zone */
    int16_t pad2; /* make things line up again */
    uint32_t max_file; /* maximum file size */
    uint32_t zones; /* number of zones on disk */
    int16_t magic; /* magic number */
    int16_t pad3; /* make things line up again */
    uint16_t blocksize; /* block size in bytes */
    uint8_t subversion; /* filesystem sub–version */
};

struct __attribute__((packed)) inode {
    uint16_t mode; /* mode */
    uint16_t links; /* number or links */
    uint16_t uid;
    uint16_t gid;
    uint32_t size;
    int32_t atime;
    int32_t mtime;
    int32_t ctime;
    uint32_t zone[DIRECT_ZONES];
    uint32_t indirect;
    uint32_t two_indirect;
    uint32_t unused;
};

struct options {
    uint8_t verbose;
    int8_t partition;
    int8_t subpartition;
    char *imagefile;
    char *path;
};

struct __attribute__((packed)) dirent {
    uint32_t inode_num;
    char filename[MAX_FILENAME_LEN];
};

void get_options(int argc, char *argv[], struct options *my_options);
void canonicalize_path(char *);

uint32_t get_partition_lfirst(uint32_t, int8_t);
uint32_t find_base(int8_t partition, int8_t subpartition);
void get_inode_n(uint32_t inode_num, struct inode * my_inode);
int read_zone(uint32_t, uint8_t *, unsigned long);
int read_indirect(uint32_t, uint32_t *);
void read_file(struct inode *, uint8_t *);


FILE *disk;
uint32_t base_offset;           /* relative to start of disk */
uint32_t inode_table_offset;    /* relative to start of disk */
/* NOTE THAT I CHANGED THIS FROM OFFSET FROM BASE,
so now we just use inode_table_offset on its own if we want inode table */
uint32_t zone_size;             /* bytes */
int ptrs_per_zone;              /* used for indirect/double direct zones */

/**
 * get_partition_lfirst() validates a partition table by checking signatures,
 * and then finds the desired partition entry. After verifying it is a valid
 * minix partition, it returns the lFirst entry of the partition, which
 * is the absolute sector number for where the partition data begins.
*/
uint32_t get_partition_lfirst(uint32_t boot_sector_num, int8_t partition_num) {
    uint8_t sigs[2];
    uint32_t boot_sector_offset = (boot_sector_num * SECTOR_SIZE);
    fseek(disk, boot_sector_offset + SIG_1_OFFSET, SEEK_SET);
    fread(sigs, sizeof(uint8_t), 2, disk);
    if (sigs[0] != PART_TAB_SIG_1 || sigs[1] != PART_TAB_SIG_2) {
        printf("Sector is %d, Partition num is %d\n",boot_sector_num, partition_num);
        fprintf(stderr, "Invalid partition signature (%02x,%02x).\n", sigs[0], sigs[1]);
        exit(EXIT_FAILURE);
    }

    /* find offset to desired partition entry in partition table */
    uint32_t partition_entry_offset = boot_sector_offset + PART_TAB_START_ADDR 
                                    + (sizeof(struct partition_entry) * partition_num);
    struct partition_entry my_partition_entry; 
    fseek(disk, partition_entry_offset, SEEK_SET);
    fread(&my_partition_entry, sizeof(struct partition_entry), 1, disk);

    if (my_partition_entry.type != MINIX_PART_TYPE) {
        fprintf(stderr, "Chosen partition has invalid type (%02x).\n", my_partition_entry.type);
        exit(EXIT_FAILURE);
    } 
    return my_partition_entry.lFirst;
}

/**
 * find_base() takes the optional partition and subpartition
 * arguments and returns the base offset, which is how far away
 * the base of the filesystem is in bytes from the start of the disk 
*/
uint32_t find_base(int8_t partition, int8_t subpartition) {
    if (partition == NO_PARTITION) {
        return 0;
    }

    /* sector for the initial partition is 0 */
    uint32_t first_sector = get_partition_lfirst(0, partition);

    if (subpartition == NO_PARTITION) {
        return first_sector * SECTOR_SIZE;
    }

    /* sector for the subpartition is the first sector of the partition */
    first_sector = get_partition_lfirst(first_sector, subpartition);
    return first_sector * SECTOR_SIZE;
}

void get_options(int argc, char *argv[], struct options *my_options) {
    int option;

    /* defaults */
    my_options->verbose = 0;
    my_options->partition = NO_PARTITION;
    my_options->subpartition = NO_PARTITION;

    /* Each letter in "vp:s:" is an option. 
     A colon afterwards means to expect an argument which
     will be pointed to by optarg */
    while ((option = getopt(argc, argv, "vp:s:")) != -1) {
        switch (option) {
            case 'v':
                my_options->verbose = 1;
                break;
            case 'p':
                my_options->partition = atoi(optarg);
                if (my_options->partition > MAX_PARTITION_NUM) {
                    fprintf(stderr, "Partition %d out of range.  Must be 0..%d.", my_options->partition, MAX_PARTITION_NUM);
                    exit(EXIT_FAILURE);
                }
                break;
            case 's':
                my_options->subpartition = atoi(optarg);
                if (my_options->subpartition > MAX_PARTITION_NUM) {
                    fprintf(stderr, "Subpartition %d out of range.  Must be 0..%d.", my_options->subpartition, MAX_PARTITION_NUM);
                    exit(EXIT_FAILURE);
                }
                break;
            default:
                /* getopt will print "invalid option" message */
                break;
        }
    }

    /* optind is index of next element to be processed in argv */
    if (optind >= argc) {
        fprintf(stderr, "usage: minls [ -v ] [ -p num [ -s num ] ] imagefile [ path ]\n\
            Options:\n\
            -p part --- select partition for filesystem (default: none)\n\
            -s sub --- select subpartition for filesystem (default: none)\n\
            -h help --- print usage information and exit\n\
            -v verbose --- increase verbosity level\n");
        exit(1);
    }

    my_options->imagefile = argv[optind];
    /* if there's one more arg, that is our path. otherwise path is "/" */
    if (optind + 1 < argc) {
        my_options->path = malloc(strlen(argv[optind + 1]) + 1);
        strcpy(my_options->path, argv[optind + 1]);
    } else {
        my_options->path = malloc(2);
        my_options->path[0] = '/';
        my_options->path[1] = '\0';
    }
    canonicalize_path(my_options->path);

    /* FOR DEBUGGING */
    printf("%d, %d, %d, %s, %s\n", 
                            my_options->verbose, 
                            my_options->partition,
                            my_options->subpartition, 
                            my_options->imagefile, 
                            my_options->path);
}

void get_inode_n(uint32_t inode_num, struct inode * my_inode) {
    /* inode are not 0 indexed and start at 1 */
    uint32_t inode_addr = inode_table_offset + (inode_num - 1) * INODE_SIZE;
    fseek(disk, inode_addr, SEEK_SET);
    fread(my_inode, sizeof(struct inode), 1, disk);
}

void get_superblock(struct superblock *sb) {
    uint32_t sb_start = base_offset + SUPERBLOCK_OFFSET;
    fseek(disk, sb_start, SEEK_SET);
    fread(sb, sizeof(struct superblock), 1, disk);

    if (sb->magic != MINIX_MAGIC_NUM) {
        fprintf(stderr, "This doesn't look like a MINIX filesystem.\n");
        exit(EXIT_FAILURE);
    }

    zone_size = sb->blocksize << sb->log_zone_size;
    /* spec says that indirect zones only use first block of zone for ptrs */
    ptrs_per_zone = sb->blocksize / sizeof(uint32_t);
    uint32_t inode_table_offset_blocks = I_BLOCK_OFFSET + sb->i_blocks + sb->z_blocks;
    inode_table_offset = base_offset + (inode_table_offset_blocks * sb->blocksize);
}

/**
 * reads data from a single zone into buf, handles holes.
 * Returns amount read. This is a helper function for read_file().
*/
int read_zone(uint32_t zone_num, uint8_t *buf, unsigned long bytes_remaining) {
    /* read entire zone or only what is left of inode */
    int to_read = zone_size < bytes_remaining ? zone_size : bytes_remaining;
    if (zone_num == 0) {
        memset(buf, 0, to_read);
    } else {
        /* need to cast to long to avoid overflow */
        fseek(disk, base_offset + (long)zone_num * zone_size, SEEK_SET);
        fread(buf, 1, to_read, disk);
    }
    return to_read;
}

/**
 * read_indirect() reads a zone full of pointers into indirect_zones, and
 * returns ptr count. This is a helper function for read_file().
*/
int read_indirect(uint32_t zone_num, uint32_t *indirect_zones) {
    fseek(disk, base_offset + (long)zone_num * zone_size, SEEK_SET);
    fread(indirect_zones, sizeof(uint32_t), ptrs_per_zone, disk);
    return ptrs_per_zone;
}

void read_file(struct inode *inode, uint8_t *buf) {
    unsigned long remaining = inode->size;
    uint8_t *ptr = buf;

    /* direct zones */
    int i, j;
    for (i = 0; i < DIRECT_ZONES && remaining > 0; i++) {
        int bytes_read = read_zone(inode->zone[i], ptr, remaining);
        ptr += bytes_read; 
        remaining -= bytes_read;
    }

    /* single indirect */
    if (remaining > 0 && inode->indirect != 0) {
        uint32_t indirect_zones[ptrs_per_zone];
        read_indirect(inode->indirect, indirect_zones);
        for (i = 0; i < ptrs_per_zone && remaining > 0; i++) {
            int bytes_read = read_zone(indirect_zones[i], ptr, remaining);
            ptr += bytes_read; 
            remaining -= bytes_read;
        }
    }

    /* double indirect */
    if (remaining > 0 && inode->two_indirect != 0) {
        uint32_t dbl_indirect_zones[ptrs_per_zone];
        read_indirect(inode->two_indirect, dbl_indirect_zones);
        for (i = 0; i < ptrs_per_zone && remaining > 0; i++) {
            if (dbl_indirect_zones[i] == 0) continue;
            uint32_t indirect_zones[ptrs_per_zone];
            read_indirect(dbl_indirect_zones[i], indirect_zones);
            for (j = 0; j < ptrs_per_zone && remaining > 0; j++) {
                int n = read_zone(indirect_zones[j], ptr, remaining);
                ptr += n; 
                remaining -= n;
            }
        }
    }
}

void print_superblock(struct superblock *sb) {
    printf("Superblock Contents:\n");
    printf("Stored Fields:\n");
    printf("  ninodes %12u\n",   sb->ninodes);
    printf("  i_blocks %11d\n",  sb->i_blocks);
    printf("  z_blocks %11d\n",  sb->z_blocks);
    printf("  firstdata %10u\n",  sb->firstdata);
    printf("  log_zone_size %6d (zone size: %u)\n", sb->log_zone_size, zone_size);
    printf("  max_file %11u\n",  sb->max_file);
    printf("  magic         0x%04x\n", (uint16_t)sb->magic);
    printf("  zones %14u\n",     sb->zones);
    printf("  blocksize %10u\n",  sb->blocksize);
    printf("  subversion %9u\n", sb->subversion);
}

/* RETURNS -1 if file not found */
uint32_t filename_to_inode_num(uint32_t dir_inode_num, char *filename) {
    struct inode dir;
    struct dirent *buf, *curr;
    unsigned int i;
    unsigned long num_dir_entries;

    get_inode_n(dir_inode_num, &dir);
    num_dir_entries = dir.size / sizeof(struct dirent);
    
    buf = (struct dirent *)malloc(dir.size);
    curr = buf;
    read_file(&dir, (uint8_t *) curr);

    for (i = 0; i < num_dir_entries; i++) {
        if (strncmp(filename, curr->filename, MAX_FILENAME_LEN) == 0) {
            free(buf);
            return curr->inode_num;
        }
        curr++;
    }

    free(buf);
    return -1;
}

// uint32_t find_file_inode_from_path(char *path) {
//     struct inode root;

//     if (*path == '/') {
//         return ROOT_INODE_NUM;
//     }

//     char *subpath = strtok(path, '/');
//     while (strtok(NULL, '/') != NULL) {

//     }
//     get_inode_n(ROOT_INODE_NUM, &root);

// }

/**
 * this function fixes a path so that there are no duplicate slashes, at 
 * least one slash for root, and no slashes at the end of the path.
 * It assumes path is null-terminated and edits the path in place.
*/
void canonicalize_path(char *path) {
    /* copy path so we have a copy to work with */
    int len = strlen(path);
    char tmp[len + 1];
    strncpy(tmp, path, len + 1);

    path[0] = '\0';
    
    char *substring = strtok(tmp, "/");

    /* in case the path was just '/' */
    if (substring == NULL) {
        strcat(path, "/");
        return;
    }

    while (substring != NULL) {
        strcat(path, "/"); /* add slash */
        strcat(path, substring); /* add substring */
        substring = strtok(NULL, "/"); /* process next substring, if any */
    }
}

int main(int argc, char *argv[]) {
    struct options my_options = {0};
    struct superblock sb;

    get_options(argc, argv, &my_options);

    disk = fopen(my_options.imagefile, "rb");
    if (disk == NULL) {
        perror("fopen");
        return EXIT_FAILURE;
    }

    base_offset = find_base(my_options.partition, my_options.subpartition);

    get_superblock(&sb);

    if (my_options.verbose) {
        print_superblock(&sb);
    }

    struct inode hello_inode;
    uint32_t hello_inode_num = filename_to_inode_num(ROOT_INODE_NUM, "Hello");
    get_inode_n(hello_inode_num, &hello_inode);
    printf("%u", hello_inode.size);

    free(my_options.path); /* free the path name malloced in get_options() */

    return EXIT_SUCCESS;
}