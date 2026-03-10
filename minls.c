#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>

/**
 * QUESTIONS:
 * does the order of args matter? for some reason -v using his minls doesnt work
*/

/**
 * TESTING:
 * ~pn-cs453/demos/minls ~pn-cs453/Given/Asgn5/Images/____
 * */

#define SECTOR_SIZE             512
#define PART_TAB_START_ADDR     0x1BE

#define MINIX_PART_TYPE         0x81
#define PART_TAB_SIG_1          0x55
#define PART_TAB_SIG_2          0xAA
#define SIG_1_OFFSET            510
#define SIG_2_OFFSET            511

#define MAX_PARTITION_NUM       3
#define NO_PARTITION            -1

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

struct __attribute((packed)) superblock {
    uint32_t ninodes; /* number of inodes in this filesystem */
    uint16_t pad1; /* make things line up properly */
    int16_t i_blocks; /* # of blocks used by inode bit map */
    int16 t z blocks; /* # of blocks used by zone bit map */
    uint16 t firstdata; /* number of first data zone */
int16 t log zone size; /* log2 of blocks per zone */
int16 t pad2; /* make things line up again */
uint32 t max file; /* maximum file size */
uint32 t zones; /* number of zones on disk */
int16 t magic; /* magic number */
int16 t pad3; /* make things line up again */
uint16 t blocksize; /* block size in bytes */
uint8 t subversion; /* filesystem sub–version */
}


}

struct options {
    uint8_t verbose;
    int8_t partition;
    int8_t subpartition;
    char *imagefile;
    char *path;
};

void get_options(int argc, char *argv[], struct options *my_options);
uint32_t get_partition_lfirst(FILE *disk, uint32_t sector, int8_t partition_num);
unsigned long find_base(FILE *disk, int8_t partition, int8_t subpartition);

uint32_t get_partition_lfirst(FILE *disk, uint32_t sector, int8_t partition_num) {
    uint8_t sigs[2];
    uint32_t base_in_bytes = (sector * SECTOR_SIZE);
    fseek(disk, base_in_bytes + SIG_1_OFFSET, SEEK_SET);
    fread(sigs, sizeof(uint8_t), 2, disk);
    if (sigs[0] != PART_TAB_SIG_1 || sigs[1] != PART_TAB_SIG_2) {
        printf("Sector is %d, Partition num is %d\n",sector, partition_num);
        fprintf(stderr, "Invalid partition signature (%02x,%02x).\n", sigs[0], sigs[1]);
        exit(EXIT_FAILURE);
    }

    uint32_t partition_entry_start = base_in_bytes + PART_TAB_START_ADDR 
                                    + (sizeof(struct partition_entry) * partition_num);
    struct partition_entry my_partition_entry; 
    fseek(disk, partition_entry_start, SEEK_SET);
    fread(&my_partition_entry, sizeof(struct partition_entry), 1, disk);

    if (my_partition_entry.type != MINIX_PART_TYPE) {
        fprintf(stderr, "Chosen partition has invalid type (%02x).\n", my_partition_entry.type);
        exit(EXIT_FAILURE);
    } 
    return my_partition_entry.lFirst;
}

unsigned long find_base(FILE *disk, int8_t partition, int8_t subpartition) {
    if (partition == NO_PARTITION) {
        return 0;
    }

    uint32_t first_sector = get_partition_lfirst(disk, 0, partition);

    if (subpartition == NO_PARTITION) {
        return first_sector * SECTOR_SIZE;
    }

    first_sector = get_partition_lfirst(disk, first_sector, subpartition);
    return first_sector * SECTOR_SIZE;

// /* then access the partition you want by index */
// struct partition_entry *p = &table[part_num];
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
    my_options->path = (optind + 1 < argc) ? argv[optind + 1] : "/";

    /* FOR DEBUGGING */
    printf("%d, %d, %d, %s, %s\n", 
                            my_options->verbose, 
                            my_options->partition,
                            my_options->subpartition, 
                            my_options->imagefile, 
                            my_options->path);
}

int main(int argc, char *argv[]) {
    struct options my_options = {0};
    get_options(argc, argv, &my_options);

    FILE *fp = fopen(my_options.imagefile, "rb");
    if (fp == NULL) {
        perror("fopen");
        return EXIT_FAILURE;
    }
    unsigned long base = find_base(fp, 
                                my_options.partition, 
                                my_options.subpartition);

    get_superblock()
    unsigned long superblock_offset = base + 1024;


    if (base == 0) {
        return EXIT_FAILURE;
    }

    // get_filesystem_info(FILE *start);

    return EXIT_SUCCESS;
}