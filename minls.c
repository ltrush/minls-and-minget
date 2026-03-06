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

void print_usage() {
    fprintf(stderr, "usage: minls [ -v ] [ -p num [ -s num ] ] imagefile [ path ]\n\
    Options:\n\
    -p part --- select partition for filesystem (default: none)\n\
    -s sub --- select subpartition for filesystem (default: none)\n\
    -h help --- print usage information and exit\n\
    -v verbose --- increase verbosity level\n");
}

int main(int argc, char *argv[]) {
    int option;
    int verbose = 0;
    int partition = -1;    /* -1 means "not set" */
    int subpartition = -1;

    /* Each letter in "vp:s:" is an option. 
     A colon afterwards means to expect an argument which
     will be pointed to by optarg */
    while ((option = getopt(argc, argv, "vp:s:")) != -1) {
        switch (option) {
            case 'v':
                verbose = 1;
                break;
            case 'p':
                partition = atoi(optarg);
                break;
            case 's':
                subpartition = atoi(optarg);
                break;
            default:
                /* getopt will print "invalid option" message */
                break;
        }
    }

    /* after getopt, optind points to the first non-flag argument */
    if (optind >= argc) {
        print_usage();
        exit(1);
    }
    char *imagefile = argv[optind];
    /* if there's one more arg, that is our path. otherwise path is "/" */
    char *path = (optind + 1 < argc) ? argv[optind + 1] : "/";

    /* FOR DEBUGGING */
    printf("%d, %d, %d, %s, %s\n", verbose, partition, subpartition, imagefile, path);

    return EXIT_SUCCESS;
}