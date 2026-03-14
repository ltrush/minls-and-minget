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
    char *path;
};

void get_options_minls(int argc, char *argv[], struct options *my_options) {
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
    if (optind >= argc) {
        fprintf(stderr, "usage: minls [ -v ] [ -p num [ -s num ] ]\
         imagefile [ path ]\n\
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
        if (my_options->path == NULL) {
            perror("malloc path");
            exit(EXIT_FAILURE);
        }
        strcpy(my_options->path, argv[optind + 1]);
    } else {
        my_options->path = malloc(2);
        if (my_options->path == NULL) {
            perror("malloc path");
            exit(EXIT_FAILURE);
        }
        my_options->path[0] = '/';
        my_options->path[1] = '\0';
    }
    canonicalize_path(my_options->path);
}

void print_file(char *path, struct inode *in) {
    print_perm(in->mode);
    printf(" %9u %s\n",in->size, path + 1);
}

void print_dir_entry(struct dirent * dir) {
    struct inode in;
    get_inode_n(dir->inode_num, &in);
    print_perm(in.mode);
    char *name_to_print = (dir->filename[0] == '/') 
                ? &dir->filename[1] : dir->filename;
    printf(" %9u %s\n", in.size, name_to_print);
}

void print_dir(struct inode *in) {
    struct dirent * entries;
    int i;
    int count;

    entries = malloc(in->size);
    if (entries == NULL) {
        perror("malloc dir_entries");
        exit(EXIT_FAILURE);
    }
    read_file(in, (uint8_t *)entries);
    count = in->size / sizeof(struct dirent);
    for (i = 0; i < count; i++) {
        if (entries[i].inode_num == 0) {
            continue;
        }
        print_dir_entry(&entries[i]);
    }
    free(entries);
}

int main(int argc, char *argv[]) {
    struct options my_options = {0};
    struct superblock sb;

    get_options_minls(argc, argv, &my_options);

    disk = fopen(my_options.imagefile, "rb");
    if (disk == NULL) {
        perror("fopen");
        return EXIT_FAILURE;
    }

    base_offset = find_base(my_options.partition, my_options.subpartition);

    get_superblock(&sb);

   
    
    struct inode target_inode;
    uint32_t target_inode_num;

    target_inode_num = find_file_inode_from_path(my_options.path);
    get_inode_n(target_inode_num, &target_inode);
     if (my_options.verbose) {
        printf("%s:\n",my_options.path);
        print_partition_table();
        print_superblock(&sb);
        print_inode(&target_inode);
    }

    
    if ((target_inode.mode & FILE_TYPE_MASK) == MINIX_DIRECTORY) {
        printf("%s:\n",my_options.path);
        print_dir(&target_inode);
    } else {
        print_file(my_options.path, &target_inode);
    }

    free(my_options.path); /* free the path name malloced in get_options() */
    fclose(disk);

    return EXIT_SUCCESS;
}