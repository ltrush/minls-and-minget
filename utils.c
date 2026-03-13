#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <time.h>
#include "utils.h"

//~pn-cs453/demos/minget -v  ~pn-cs453/Given/Asgn5/Images/Files 
//Holes/whole-indirect ./whole-indirect

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
        printf("Sector is %d, Partition num is %d\n",
                                boot_sector_num, partition_num);
        fprintf(stderr, "Invalid partition signature (%02x,%02x).\n", 
                                               sigs[0], sigs[1]);
        exit(EXIT_FAILURE);
    }

    /* find offset to desired partition entry in partition table */
    uint32_t partition_entry_offset = boot_sector_offset + PART_TAB_START_ADDR 
                        + (sizeof(struct partition_entry) * partition_num); 
    fseek(disk, partition_entry_offset, SEEK_SET);
    fread(&my_partition_entry, sizeof(struct partition_entry), 1, disk);

    if (my_partition_entry.type != MINIX_PART_TYPE) {
        fprintf(stderr, "Chosen partition has invalid type (%02x).\n", 
                                        my_partition_entry.type);
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
    block_size = sb->blocksize;
    /* spec says that indirect zones only use first block of zone for ptrs */
    ptrs_per_zone = sb->blocksize / sizeof(uint32_t);
    uint32_t inode_table_offset_blocks = I_BLOCK_OFFSET + sb->i_blocks +
                                             sb->z_blocks;
    inode_table_offset = base_offset + 
                    (inode_table_offset_blocks * sb->blocksize);
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
void read_indirect(uint32_t zone_num, uint32_t *indirect_zones) {
    if (zone_num == 0) {
        memset(indirect_zones, 0, block_size);
        return;
    }
    fseek(disk, base_offset + (long)zone_num * zone_size, SEEK_SET);
    fread(indirect_zones, sizeof(uint32_t), ptrs_per_zone, disk);
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
    if (remaining > 0) {
        uint32_t indirect_zones[ptrs_per_zone];
        read_indirect(inode->indirect, indirect_zones);
        for (i = 0; i < ptrs_per_zone && remaining > 0; i++) {
            int bytes_read = read_zone(indirect_zones[i], ptr, remaining);
            ptr += bytes_read; 
            remaining -= bytes_read;
        }
    }

    /* double indirect */
    if (remaining > 0) {
        uint32_t dbl_indirect_zones[ptrs_per_zone];
        read_indirect(inode->two_indirect, dbl_indirect_zones);
        for (i = 0; i < ptrs_per_zone && remaining > 0; i++) {
            // if (dbl_indirect_zones[i] == 0) continue;
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
    
    printf("\nSuperblock Contents:\n");
    printf("Stored Fields:\n");
    printf("  ninodes %12u\n",   sb->ninodes);
    printf("  i_blocks %11d\n",  sb->i_blocks);
    printf("  z_blocks %11d\n",  sb->z_blocks);
    printf("  firstdata %10u\n",  sb->firstdata);
    printf("  log_zone_size %6d (zone size: %u)\n", sb->log_zone_size, 
                                                        zone_size);
    printf("  max_file %11u\n",  sb->max_file);
    printf("  magic         0x%04x\n", (uint16_t)sb->magic);
    printf("  zones %14u\n",     sb->zones);
    printf("  blocksize %10u\n",  sb->blocksize);
    printf("  subversion %9u\n", sb->subversion);
}
void print_inode(struct inode * in) {
    int i;
    time_t t;

    printf("\nFile inode:\n");
    printf("  uint16_t mode 0x%04x (",in->mode);
    print_perm(in->mode);
    printf(")\n");
    printf("  uint16_t links %u\n",in->links);
    printf("  uint16_t uid %u\n", in->uid);
    printf("  uint16_t gid %u\n", in->gid);
    printf("  uint32_t size %u\n", in->size);
    t= in->atime;
    printf("  uint32_t atime %u --- %s", in->atime, ctime(&t));
    t= in->mtime;
    printf("  uint32_t mtime %u --- %s", in->mtime, ctime(&t));
    t= in->ctime;
    printf("  uint32_t ctime %u --- %s", in->ctime, ctime(&t));
    printf("\nDirect zones:\n");
    for (i = 0; i < DIRECT_ZONES; i++) {
        printf("            zone[%d] = %u\n",i,in->zone[i]);
    }
    printf("  uint32_t indirect %u\n", in->indirect);
    printf("  uint32_t double %u\n", in->indirect);

}

void print_partition_table() {
    printf("\nPartition_Table:\n");
    printf("  uint8_t  bootind    %u\n", my_partition_entry.bootind);
    printf("  uint8_t  start_head %u\n", my_partition_entry.start_head);
    printf("  uint8_t  start_sec  %u\n", my_partition_entry.start_sec);
    printf("  uint8_t  start_cyl  %u\n", my_partition_entry.start_cyl);
    printf("  uint8_t  type       %u\n", my_partition_entry.type);
    printf("  uint8_t  end_head   %u\n", my_partition_entry.end_head);
    printf("  uint8_t  end_sec    %u\n", my_partition_entry.end_sec);
	printf("  uint8_t  end_cyl    %u\n", my_partition_entry.end_cyl);
	printf("  uint32_t lFirst     %u\n", my_partition_entry.lFirst);
	printf("  uint32_t size       %u\n", my_partition_entry.size);

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
	if (buf == NULL) {
		fprintf(stderr,"malloc dirent");
		exit(EXIT_FAILURE);
	}
    curr = buf;
    read_file(&dir, (uint8_t *) curr);

    for (i = 0; i < num_dir_entries; i++) {
        if (curr->inode_num == 0) {
                curr++;
                continue;
        }
        if (strlen(filename) == strlen(curr->filename) && 
           strncmp(filename, curr->filename, MAX_FILENAME_LEN) == 0) {
            free(buf);
            return curr->inode_num;
        }
        curr++;
    }
    
    free(buf);
    return (uint32_t)-1;
}
/*given the path, it will tokenize by the / 
 path is already canonicalized and finds the
 inode of the last file/directory in path */
uint32_t find_file_inode_from_path(char *path) {
    uint32_t current_inode = ROOT_INODE_NUM;
    struct inode in;

    if (strcmp(path, "/") == 0) {
        return ROOT_INODE_NUM;
    }

    char copy[strlen(path) + 1];
    strcpy(copy,path);
    char *token = strtok(copy,"/");
    while (token != NULL) {
        get_inode_n(current_inode,&in);
		/* is this still a valid directory to pass through/open */
        if ((in.mode & FILE_TYPE_MASK) != MINIX_DIRECTORY) {
            fprintf(stderr,"Not a directory: %s\n", token);
            exit(EXIT_FAILURE);
        }
		/*find the inode that is currently in the path*/
        current_inode = filename_to_inode_num(current_inode, token);

        if (current_inode == (uint32_t)-1) {
            fprintf(stderr, "File not found: %s\n",token);
            exit(EXIT_FAILURE);
        }
		/*continue the token from where left off*/
        token = strtok(NULL,"/");

    }
    return current_inode;

}

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

/*given the mode of an inode it will use the MINIX MASK
 to print out 10 characters representing the permissions*/
void print_perm(uint16_t mode) {
    char perm[11];
    perm[10] ='\0';
    uint16_t is_dir = (mode & FILE_TYPE_MASK); 

    perm[0] = (is_dir == MINIX_DIRECTORY) ? 'd' : '-';
    perm[1] = (mode & OWNER_READ) ? 'r' : '-';
    perm[2] = (mode & OWNER_WRITE) ? 'w' : '-';
    perm[3] = (mode & OWNER_EXEC) ? 'x' : '-';
    perm[4] = (mode & GROUP_READ) ? 'r' : '-';
    perm[5] = (mode & GROUP_WRITE) ? 'w' : '-';
    perm[6] = (mode & GROUP_EXEC) ? 'x' : '-';
    perm[7] = (mode & OTHER_READ) ? 'r' : '-';
    perm[8] = (mode & OTHER_WRITE) ? 'w' : '-';
    perm[9] = (mode & OTHER_EXEC) ? 'x' : '-';

    printf("%s",perm);
}