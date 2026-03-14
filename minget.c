#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <time.h>
#include "utils.h"

/**
 * TESTING:
 * ~pn-cs453/demos/minls ~pn-cs453/Given/Asgn5/Images/____
 * */

struct options {
    uint8_t verbose;
    int8_t partition;
    int8_t subpartition;
    char *imagefile;
    char *srcpath;
    char *dstpath;
};

void get_options_minget(int argc, char *argv[], struct options *my_options) {
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
                if (my_options->partition > MAX_PARTITION_NUM ||
                                my_options->partition < MIN_PARTITION_NUM) {
                    fprintf(stderr, "Partition %d out of range. Need(0..%d)\n",
                                     my_options->partition, MAX_PARTITION_NUM);
                    exit(EXIT_FAILURE);
                }
                break;
            case 's':
                my_options->subpartition = atoi(optarg);
                if (my_options->subpartition > MAX_PARTITION_NUM ||
                                my_options->partition < MIN_PARTITION_NUM) {
                    fprintf(stderr, "Subpartition %d out of range.  \
                                Must be 0..%d.\n",
                                my_options->subpartition, MAX_PARTITION_NUM);
                    exit(EXIT_FAILURE);
                }
                break;
            default:
                /* getopt will print "invalid option" message */
                break;
        }
    }

    /* optind is index of next element to be processed in argv */
    if (optind + 1 >= argc) {
        fprintf(stderr, "usage: minls [ -v ] [ -p num [ -s num ] ]\
         imagefile srcpath [ dstpath ]\n\
            Options:\n\
            -p part --- select partition for filesystem (default: none)\n\
            -s sub --- select subpartition for filesystem (default: none)\n\
            -h help --- print usage information and exit\n\
            -v verbose --- increase verbosity level\n");
        exit(EXIT_FAILURE);
    }

    my_options->imagefile = argv[optind];

    my_options->srcpath = malloc(strlen(argv[optind + 1]) + 1);
    if (my_options->srcpath == NULL) {
        perror("malloc srcpath");
        exit(EXIT_FAILURE);
    }
    strcpy(my_options->srcpath, argv[optind + 1]);
    canonicalize_path(my_options->srcpath);

    /* if there's one more arg, that is our path. otherwise path is "/" */
    if (optind + 2 < argc) {
        my_options->dstpath = malloc(strlen(argv[optind + 2]) + 1);
        if (my_options->dstpath == NULL) {
            perror("malloc dstpath");
            exit(EXIT_FAILURE);
        }
        strcpy(my_options->dstpath, argv[optind + 2]);
    } else {
        my_options->dstpath = NULL;

    }
}

int main(int argc, char *argv[]) {
    struct options my_options = {0};
    struct superblock sb;
    FILE* out;

    get_options_minget(argc, argv, &my_options);

    disk = fopen(my_options.imagefile, "rb");
    if (disk == NULL) {
        perror("fopen");
        return EXIT_FAILURE;
    }

    base_offset = find_base(my_options.partition, my_options.subpartition);

    get_superblock(&sb);

    struct inode target_inode;
    uint32_t target_inode_num;

    target_inode_num = find_file_inode_from_path(my_options.srcpath);
    get_inode_n(target_inode_num, &target_inode);
     if (my_options.verbose) {
        printf("%s:\n",my_options.dstpath);
        print_partition_table();
        print_superblock(&sb);
        print_inode(&target_inode);
    }

    if ((target_inode.mode & FILE_TYPE_MASK) == MINIX_DIRECTORY) {
        fprintf(stderr, "Cannot copy a directory.\n");
        return EXIT_FAILURE;
    }

    if ((target_inode.mode & FILE_TYPE_MASK) != REGULAR_FILE) {
        fprintf(stderr, "%s: not a regular file\n", my_options.srcpath);
        return EXIT_FAILURE;
    }

    uint8_t *buffer = malloc(target_inode.size);
    if (buffer == NULL) {
        perror("malloc buffer");
        return EXIT_FAILURE;
    }

    read_file(&target_inode, buffer);

    if (my_options.dstpath == NULL) {
        out = stdout;
    } else {
        out = fopen(my_options.dstpath, "wb");
        if (!out) {
            perror("fopen");
            exit(EXIT_FAILURE);
        }
    }
    fwrite(buffer,1,target_inode.size, out);

    if (out != stdout) {
        fclose(out);
    }
    free(buffer);
    free(my_options.srcpath);
    if (my_options.dstpath) {
        free(my_options.dstpath);
    }
    fclose(disk);

    return EXIT_SUCCESS;
}