
#include "fs.h"
#include "disk.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#define FS_MAGIC           0xf0f03410
#define INODES_PER_BLOCK   128
#define POINTERS_PER_INODE 5
#define POINTERS_PER_BLOCK 1024

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

int fs_format()
{
	{
	//Read in super block
	union fs_block block;
	disk_read(0, block.data);

	//Check if FS already mounted
	if (mounted){
		printf("FS is already mounted, format failed\n");
	 	return 0;
	}

	//Create superblock, prepare for mount
	int ninodeblocks = ceil(.1 * (double)disk_size());
	block.super.magic = FS_MAGIC;
	block.super.nblocks = disk_size();
	block.super.ninodeblocks = ninodeblocks;
	block.super.ninodes = INODES_PER_BLOCK * ninodeblocks;

	// Write changes to disk
	disk_write(0, block.data);

	//Clear the inode table
	union fs_block iblock;
	for(int i=1; i<=block.super.ninodeblocks; i++){
		// Read in inode block
		disk_read(i, iblock.data);
		for(int j=0; j<INODES_PER_BLOCK; j++){
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
	if(valid_super_block)
		printf("    magic number is valid\n");
	else{
		printf("    magic number is not valid\n");
		return;
	}
	printf("    %d blocks\n",block.super.nblocks);
	printf("    %d inode blocks\n",block.super.ninodeblocks);
	printf("    %d inodes\n",block.super.ninodes);

	// Go through inode blocks
	for(int i=1; i<=block.super.ninodeblocks; i++){ //added equal
		// Read in inode block
		disk_read(i, block.data);

		// Traverse inodes
		for(int j = 0; j<INODES_PER_BLOCK; j++) {
			// Check if inode is valid
			if(block.inode[j].isvalid) {
				int inumber = get_inum(i, j);
				printf("inode %d:\n", inumber);
				printf("    size: %d bytes\n", block.inode[j].size);

				// Traverse direct pointers
				if(block.inode[j].size > 0){
					printf("    direct blocks: ");
					print_valid_blocks(block.inode[j].direct, POINTERS_PER_INODE);
				}

				// Traverse indirect pointers
				if(block.inode[j].indirect != 0){
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
	return 0;
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

int fs_read( int inumber, char *data, int length, int offset )
{
	return 0;
}

int fs_write( int inumber, const char *data, int length, int offset )
{
	return 0;
}
