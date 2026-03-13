#include <unistd.h>
#include <stdint.h>

#include <getopt.h>


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

/*MINIX MODE MASKS */
#define FILE_TYPE_MASK          0170000
#define REGULAR_FILE			0100000
#define MINIX_DIRECTORY         0040000
#define OWNER_READ              0000400
#define OWNER_WRITE             0000200
#define OWNER_EXEC              0000100
#define GROUP_READ              0000040
#define GROUP_WRITE             0000020
#define GROUP_EXEC              0000010
#define OTHER_READ              0000004
#define OTHER_WRITE             0000002
#define OTHER_EXEC              0000001


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



struct __attribute__((packed)) dirent {
    uint32_t inode_num;
    char filename[MAX_FILENAME_LEN];
};

FILE *disk;
struct partition_entry my_partition_entry;
uint32_t base_offset;           /* relative to start of disk */
uint32_t inode_table_offset;    /* relative to start of disk */
/* NOTE THAT I CHANGED THIS FROM OFFSET FROM BASE,
so now we just use inode_table_offset on its own if we want inode table */
uint32_t zone_size;             /* bytes */
uint16_t block_size;
int ptrs_per_zone;              /* used for indirect/double direct zones */

uint32_t get_partition_lfirst(uint32_t boot_sector_num, int8_t partition_num);
uint32_t find_base(int8_t partition, int8_t subpartition);
void get_inode_n(uint32_t inode_num, struct inode * my_inode);
void get_superblock(struct superblock *sb);
int read_zone(uint32_t zone_num, uint8_t *buf, unsigned long bytes_remaining);
void read_indirect(uint32_t zone_num, uint32_t *indirect_zones);
void read_file(struct inode *inode, uint8_t *buf);
void print_superblock(struct superblock *sb);
void print_inode(struct inode * in);
uint32_t filename_to_inode_num(uint32_t dir_inode_num, char *filename);
uint32_t find_file_inode_from_path(char *path);
void canonicalize_path(char *path);
void print_perm(uint16_t mode);
void print_partition_table();
