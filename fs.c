#include "fs.h"
#include "disk.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>
#include <stdbool.h>

#define DISK_BLOCK_SIZE    4096
#define FS_MAGIC           0xf0f03410
#define INODES_PER_BLOCK   128
#define POINTERS_PER_INODE 5
#define POINTERS_PER_BLOCK 1024

int free_block_bitmap[DISK_BLOCK_SIZE] = {0};
int mounted = 0;

struct fs_superblock {
	int magic;
	int nblocks;
	int ninodeblocks;
	int ninodes;
};

struct fs_inode {
	int isvalid;
	int size;
	int direct[POINTERS_PER_INODE];
	int indirect;
};

union fs_block {
	struct fs_superblock super;
	struct fs_inode inode[INODES_PER_BLOCK];
	int pointers[POINTERS_PER_BLOCK];
	char data[DISK_BLOCK_SIZE];
};

void print_valid_blocks(int array[], int size){
	int i;
	for (i = 0; i < size; i++) {
		if(array[i] == 0){ 
			continue;
		}
		printf("%d ",array[i]);
	}
	printf("\n");
}

int check_magic(int magic){
	return (magic == FS_MAGIC);
}
int get_inum(int iblock, int inode_index) {
	return (iblock - 1)*INODES_PER_BLOCK + inode_index;
}
int fs_format()
{
	// Read in super block
	union fs_block block;
	disk_read(0, block.data);

	// Check if FS already mounted
	if (mounted) {
		printf("FS is already mounted, format failed\n");
	 	return 0;
	}

	// Create superblock, prepare for mount
	int ninodeblocks = ceil(.1 * (double)disk_size()); // set aside 10% of blocks for inodes
	block.super.magic = FS_MAGIC;
	block.super.nblocks = disk_size();
	block.super.ninodeblocks = ninodeblocks;
	block.super.ninodes = INODES_PER_BLOCK * ninodeblocks;

	// Write changes to disk
	disk_write(0, block.data);

	// Clear the inode table
	union fs_block iblock;
	int i;
	for (i = 1; i <= block.super.ninodeblocks; i++) {
		// Read in inode block
		disk_read(i, iblock.data);
		int j;
		for (j = 0; j < INODES_PER_BLOCK; j++) {
			iblock.inode[j].isvalid = 0;
		}
		disk_write(i, iblock.data);
	}

	return 1;
}

void fs_debug()
{
	
	union fs_block block;
	union fs_block indirect_block;

	// Read in super block
	disk_read(0,block.data);

	int valid_super_block = check_magic(block.super.magic);

	printf("superblock:\n");
	if (valid_super_block)
		printf("    magic number is valid\n");
	else {
		printf("    magic number is not valid\n");
		return;
	}
	printf("    %d blocks\n",block.super.nblocks);
	printf("    %d inode blocks\n",block.super.ninodeblocks);
	printf("    %d inodes\n",block.super.ninodes);

	// Traversing inode blocks
	int i;
	for (i = 1; i <= block.super.ninodeblocks; i++) { //added equal
		// Read in inode block
		disk_read(i, block.data);

		// Traverse inodes
		int j;
		for (j = 0; j < INODES_PER_BLOCK; j++) {
			// Check if inode is valid
			if (block.inode[j].isvalid) {
				int inumber = get_inum(i, j);
				printf("inode %d:\n", inumber);
				printf("    size: %d bytes\n", block.inode[j].size);

				// Traverse direct pointers
				if (block.inode[j].size > 0) {
					printf("    direct blocks: ");
					print_valid_blocks(block.inode[j].direct, POINTERS_PER_INODE);
				}

				// Traverse indirect pointers
				if (block.inode[j].indirect != 0) {
					printf("    indirect block: %d\n", block.inode[j].indirect);
					printf("    indirect data blocks: ");
					disk_read(block.inode[j].indirect, indirect_block.data);
					print_valid_blocks(indirect_block.pointers, POINTERS_PER_BLOCK);
				}
			}
		}
	}

}

int fs_mount() 
{
	union fs_block block;

	// check magic number
	disk_read(0,block.data);
	int valid_super_block = check_magic(block.super.magic);
	if (!valid_super_block) {
		printf("Invalid superblock\n");
		return 0;
	}

	// scan through all inodes and record which blocks in use
	int i;
	for (i = 1; i <= block.super.ninodeblocks; i++){
		// Read in inode block
		disk_read(i, block.data);

		// Traverse inodes
		int j;
		for (j = 0; j < INODES_PER_BLOCK; j++) {
			// Check if inode is valid
			if (block.inode[j].isvalid) {

				// check direct blocks
				int k;
				for (k = 0; k < POINTERS_PER_INODE; k++) {
					int direct_block_num = block.inode[j].direct[k];
					if (direct_block_num != 0) {
						printf("block num: %d\n", direct_block_num);
						free_block_bitmap[direct_block_num] = 1;
					}
				}
				// check indirect block
				if (block.inode[j].indirect != 0) {
					union fs_block indirect_block;
					disk_read(block.inode[j].indirect, indirect_block.data);
					for (k = 0; k < POINTERS_PER_BLOCK; k++) {
						int indirect_block_num = indirect_block.pointers[k];
						printf("indirect block: %d\n", indirect_block_num);
						free_block_bitmap[indirect_block_num] = 1;
					}
				}
			}
		}
	}
	
	// prepare fs for use
	mounted = 1;
	return 1;

}

int fs_create()
{
	return 0;
}

int fs_delete( int inumber )
{
	return 0;
}

int fs_getsize( int inumber )
{
	return -1;
}

// when scanning free_block_bitmap, start at index 1 bc 0 is not used (block number of 0 indicates null pointer)
int fs_read( int inumber, char *data, int length, int offset )
{
	return 0;
}

int fs_write( int inumber, const char *data, int length, int offset )
{
	return 0;
}
