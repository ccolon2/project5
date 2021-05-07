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

int is_mounted = 0;
int free_block_bitmap[DISK_BLOCK_SIZE] = {0};


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

void inode_load( int inumber, struct fs_inode *inode ) {}
void inode_save( int inumber, struct fs_inode *inode ) {}


int verify_magic_num(int magic) {
	return (magic == FS_MAGIC);
}

int get_block_num(int inumber) {
	return floor(inumber/INODES_PER_BLOCK) + 1;
}

void print_blocks(int a[], int sz){
	for (int i = 0; i < sz; i++) {
		if(a[i] == 0){ 
			continue;
		}
		printf("%d ",a[i]);
	}
	printf("\n");
}

int first_free_block() {
    int i;
    for (i = 2; i < DISK_BLOCK_SIZE; i++) {
        if (free_block_bitmap[i] == 0) {
            return i;
        }
    }
    return -1;
}

int getNextBlock() {

    union fs_block block;

    disk_read(0, block.data); //read superblock



    int i;

    for (i = block.super.ninodeblocks + 1; i < DISK_BLOCK_SIZE; i++) {

        if (free_block_bitmap[i] == 0) {

            memset(&free_block_bitmap[i], 0, sizeof(free_block_bitmap[0]));

            return i;

        }

    }



    printf("Error: no more room for blocks\n");

    return -1;

}

int fs_format() {
	union fs_block iblock;
	int  ninodeblocks;
	union fs_block block;
	disk_read(0, block.data); //read in superblock
	
	if (is_mounted) { //check if the filesystem is already is_mounted
		printf("Format failed: the filesystem is already is_mounted\n");
	 	return 1;
	}

    //create superblock
    ninodeblocks = ceil(.1 * (double)disk_size()); // set aside 10% of blocks for inodes
	block.super.ninodeblocks = ninodeblocks;
	block.super.ninodes = INODES_PER_BLOCK * ninodeblocks;
	block.super.magic = FS_MAGIC;
	block.super.nblocks = disk_size();
	

	// write changes to disk
	disk_write(0, block.data);

	// clear inode table
	for (int i = 1; i <= block.super.ninodeblocks; i++) {
		disk_read(i, iblock.data);

		for (int k = 0; k < INODES_PER_BLOCK; k++) {
			iblock.inode[k].isvalid = 0;
		}
		disk_write(i, iblock.data);
	}

	return 1;
}

void fs_debug() {
	int inum;
	union fs_block indirect_block;
	union fs_block block;
	disk_read(0,block.data); //read in super block
	printf("superblock:\n");
	if (verify_magic_num(block.super.magic))
		printf("    magic number is valid\n");
	else {
		printf("    magic number is not valid\n");
		return;
	}
	printf("    %d blocks\n",block.super.nblocks);
	printf("    %d inode blocks\n",block.super.ninodeblocks);
	printf("    %d inodes\n",block.super.ninodes);
	
	for (int i = 1; i <= block.super.ninodeblocks; i++) {  //traverse inode blocks
	
		disk_read(i, block.data); //read in inode block


		for (int z = 1; z < INODES_PER_BLOCK; z++) {//scan through inodes
			
			if (block.inode[z].isvalid) { //verify inode is valid
			    inum = (i- 1)*INODES_PER_BLOCK + z;
				printf("inode %d:\n", inum);
				printf("    size: %d bytes\n", block.inode[z].size);

				
				if (block.inode[z].size > 0) { //go through direct pointers
					printf("    direct blocks: ");
					print_blocks(block.inode[z].direct, POINTERS_PER_INODE);
				}

			
				if (block.inode[z].indirect != 0) { //go through indirect pointers
					printf("    indirect block: %d\n", block.inode[z].indirect);
					printf("    indirect data blocks: ");
					disk_read(block.inode[z].indirect, indirect_block.data);
					print_blocks(indirect_block.pointers, POINTERS_PER_BLOCK);
				}
			}
		}
	}

}

int fs_mount() {
	union fs_block block;

	// check magic number
	disk_read(0,block.data);
	int valid_super_block = verify_magic_num(block.super.magic);
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
						//printf("block num: %d\n", direct_block_num);
						free_block_bitmap[direct_block_num] = 1;
					}
				}
				// check indirect block
				if (block.inode[j].indirect != 0) {
					union fs_block indirect_block;
					disk_read(block.inode[j].indirect, indirect_block.data);
					for (k = 0; k < POINTERS_PER_BLOCK; k++) {
						int indirect_block_num = indirect_block.pointers[k];
						//printf("indirect block: %d\n", indirect_block_num);
						free_block_bitmap[indirect_block_num] = 1;
					}
				}
			}
		}
	}
	
	// prepare fs for use
	is_mounted = 1;
	return 1;

}

//how do we know where to store new inode?
//how do we access superblock to get number of inodes so we can do block.inode[ninodes] to set inode
//inodes should start as valid correct
//do we need to traverse inodes to know where to create?

/*
struct fs_inode {
    int isvalid;
    int size;
    int direct[POINTERS_PER_INODE];
    int indirect;
};
 */
//iterates through blocks to find first free inode and sets it to valid, returns 0 if fails

//POSSIBLE SUGGESTION: have inode numbers start at 1, but the inodes themselves are placed starting at position 0

int fs_create() {
    union fs_block block;
    union fs_block iblock;
    
    disk_read(0, block.data); //read in superblock
    
    
    
    int found = 0;
    int inm = 0;
    // check for first free inode
    for (int i = 1; i <= block.super.ninodeblocks; i++) {
        disk_read(i, iblock.data);

        for (int k = 0; k < INODES_PER_BLOCK; k++) {
            
            int temp_inm = ((i-1)*INODES_PER_BLOCK)+k;
            
            //if inode is free, set it to be valid and zero all other variables
            if(iblock.inode[k].isvalid == 0 && temp_inm != 0)
            {
                iblock.inode[k].isvalid = 1;
                iblock.inode[k].size = 0;
                iblock.inode[k].indirect = 0;
                for (int j = 0; j < POINTERS_PER_INODE; j++) {
                    iblock.inode[k].direct[j] = 0;
                }

                //getting inumber based on array location (k) and block location (i)
                inm = temp_inm;
                found = 1;
            }
            
            //write changes if new inode is created, increment number of inodes and return inumber
            if(found != 0)
            {
                disk_write(i, iblock.data);
                
                return inm;
                
            }
            
            
        }
        
        
    }
    
    
	return 0;
}

//sets specified inode to invalid
//TODO ADD BAD INPUT CHECKING
int fs_delete( int inumber ) {
    
    // read block from inumber
    union fs_block block;
    int block_num = get_block_num(inumber);
    disk_read(block_num, block.data);
    
    //translate inumber to inode (get array location)
    int inode_number = inumber - ((block_num-1)*INODES_PER_BLOCK);

    //mark as invalid and zero
    if (block.inode[inode_number].isvalid) {
        block.inode[inode_number].isvalid = 0;
        block.inode[inode_number].size = 0;
        block.inode[inode_number].indirect = 0;
        for (int j = 0; j < POINTERS_PER_INODE; j++) {
            if (block.inode[inode_number].direct[j] != 0) {
                free_block_bitmap[block.inode[inode_number].direct[j]] = 0;
            }
            block.inode[inode_number].direct[j] = 0;
        }

        
        //write changes
        disk_write(block_num, block.data);
        
        return 1;
    }
    
	return 0;
}

int fs_getsize( int inumber ) {
	// read block from inumber
	union fs_block block;
	int block_num = get_block_num(inumber);
	disk_read(block_num, block.data);

    //translate inumber to inode (get array location)
    int inode_number = inumber - ((block_num-1)*INODES_PER_BLOCK);

	if (block.inode[inode_number].isvalid) {	// only return size if valid inode
		return block.inode[inode_number].size;
	}
	return -1;
}

// when scanning free_block_bitmap, start at index 1 bc 0 is not used (block number of 0 indicates null pointer)

int fs_read( int inumber, char *data, int length, int offset ) {

    int block_num = get_block_num(inumber);

    union fs_block block;

    disk_read(0, block.data);

    if (block_num > block.super.ninodeblocks || block_num == 0)
    {
        printf("Error: Block number is out of bounds.\n");
        return 0;
    }

    disk_read(block_num, block.data);



    struct fs_inode inode;

    inode = block.inode[inumber % 128];



    //check for error

    if (!inode.isvalid)
    {
        printf("Error: Invalid inode\n");
        return 0;
    }
    else if (inode.size == 0)
    {
        printf("Error: Inode's size is 0\n");
        return 0;
    }
    else if (inode.size < offset)
    {
        printf("Error: Offset out of bounds\n");
        return 0;
    }




    int direct_index_num = offset / DISK_BLOCK_SIZE;
    if (direct_index_num > 5)
    {
        printf("Error: Direct block index out of bounds\n");
        return 0;
    }

    if (inode.size < length + offset)
    {
        length = inode.size - offset;
    }

    union fs_block directblock;
    int totalbytesread = 0;
    memset(data, 0, length);
    int tempbytesread = DISK_BLOCK_SIZE;
    while (direct_index_num < 5 && totalbytesread < length)
    {
        disk_read(inode.direct[direct_index_num], directblock.data);
        if (tempbytesread + totalbytesread > length)
        {
            tempbytesread = length - totalbytesread;
        }
        strncat(data, directblock.data, tempbytesread);
        direct_index_num++;
        totalbytesread += tempbytesread;
    }



    // read from indirect block if we still have bytes left to be read

    if (totalbytesread < length)
    {
        // read in the indirect block
        union fs_block indirectblock;
        union fs_block tempblock;
        disk_read(inode.indirect, indirectblock.data);



        // iterate through the indirect data blocks

        int indirectblocks;

        if (inode.size % DISK_BLOCK_SIZE == 0)
        {
            indirectblocks = inode.size / DISK_BLOCK_SIZE - 5;
        }
        else
        {
            indirectblocks = inode.size / DISK_BLOCK_SIZE - 5 + 1;
        }

        int i;

        tempbytesread = DISK_BLOCK_SIZE;

        for (i = 0; (i < indirectblocks) && (totalbytesread < length); i++)
        {

            disk_read(indirectblock.pointers[i], tempblock.data);



            // adjust tempbytesread variable if we have reached the end of the inode
            if (tempbytesread + totalbytesread > length)
            {
                tempbytesread = length - totalbytesread;
            }



            // append read data to our data variable
            strncat(data, tempblock.data, tempbytesread);
            totalbytesread += tempbytesread;

        }

    }



    // return the total number of bytes read (could be smaller than the number requested)

    return totalbytesread;

}
/*
int fs_write( int inumber, const char *data, int length, int offset )
{
    //INPUT CHECKING
    int data_length = strlen(data);
    if (data_length < offset + length) {        //checking that offset and length data is possible
        return 0;
    }
    if (inumber > block.super.ninodes) {        //checking that inumber is within number of possible inodes
        return 0;
    }
    
    
    // read block from inumber
    union fs_block block;
    int block_num = get_block_num(inumber);
    disk_read(block_num, block.data);

    //translate inumber to inode (get array location)
    int inode_number = inumber - ((block_num-1)*INODES_PER_BLOCK);

    if (block.inode[inode_number].isvalid) {    // only return size if valid inode
        char copied_data[length];
        strncpy(copied_data, data + offset, length);
        
        //start writing copied data to blocks in direct
        
        
    }
    
	return 0;
}
 */



int fs_write( int inumber, const char *data, int length, int offset ) {
    
    // read block from inumber
    int block_num = get_block_num(inumber);
    union fs_block block;
    disk_read(0, block.data);

    if (block_num > block.super.ninodeblocks || block_num == 0)
    {
        printf("Error: Block number is out of bounds.\n");
        return 0;
    }
    //fetch block
    disk_read(block_num, block.data);

    //check for error
    if (!block.inode[inumber % 128].isvalid)
    {
        printf("Error: Invalid inode\n");
        return 0;
    }


    int direct_index_num = offset / DISK_BLOCK_SIZE;
    if (direct_index_num > 5)
    {
        printf("Error: Direct block index out of bounds\n");
        return 0;
    }
    
    union fs_block directblock;
    int totalbyteswritten = 0;
    int tempbyteswritten = DISK_BLOCK_SIZE;
    while (direct_index_num < 5 && totalbyteswritten < length)
    {
        printf("BLOCK NUMBER: %d \n", block.inode[inumber % 128].direct[direct_index_num]);
        if (block.inode[inumber % 128].direct[direct_index_num] == 0) {
            int free_block_num = first_free_block();
            //int free_block_num = 3;
            printf("NEW BLOCK NUMBER: %d \n", free_block_num);
            
            block.inode[inumber % 128].direct[direct_index_num] = free_block_num;
            block.inode[inumber % 128].size += tempbyteswritten;
            
            //get block to write to
            union fs_block wblock;
            strncpy(wblock.data, data + totalbyteswritten, tempbyteswritten);
            disk_write(free_block_num, wblock.data);
            disk_write(block_num, block.data);
            
            //increment bytes_written
            free_block_bitmap[free_block_num] = 1;
            totalbyteswritten += tempbyteswritten;
        }
        /*
        disk_read(inode.direct[direct_index_num], directblock.data);
        if (tempbytesread + totalbytesread > length)
        {
            tempbytesread = length - totalbytesread;
        }
        strncat(data, directblock.data, tempbytesread);
        direct_index_num++;
        totalbytesread += tempbytesread;
         */
        direct_index_num++;
    }

    return totalbyteswritten;
}


//REMINDER: free blocks in direct when deleting inode

//is bitmap to big? we only have 5 blocks apparently
