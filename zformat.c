#include <stdio.h>
#include <string.h>

#include "oufs_lib.h"
#include "vdisk.h"

//Don't want to make a new header file because all of these functions are only used here
//Functions used later on
int initialize_disk();
int initalize_master_block();
int initialize_first_inode();
int initialize_first_directory();

int main(int argc, char** argv){
  //Write 0s to all bytes in virtual disk
  if(initialize_disk() == -1){
    fprintf(stderr, "ERROR WRITING 0s TO DISK");
  }

  //Marks master block, all inode blocks, and the first data block as allocated
  if(initalize_master_block() == -1){
    fprintf(stderr, "ERROR INITIALIZING MASTER BLOCK");
  }

  //Makes the first inode correspond to the root directory
  if(initialize_first_inode() == -1){
    fprintf(stderr, "ERROR INITIALIZING FIRST INODE");
  }

  //Makes first data block an empty directory, with '.' and '..' both referring to inode 0
  if(initialize_first_directory() == -1){
    fprintf(stderr, "ERROR CREATING FIRST DATA BLOCK");
  }
}

int initialize_disk(){

    // Creates a virtual disk with name 'vdisk1'
    if(vdisk_disk_open("vdisk1") != 0)
      return -1;

    // Steps through all bytes in disk and sets to 0
    for(int num_block = 0; num_block < N_BLOCKS_IN_DISK; ++num_block){ //Steps through each block
      BLOCK block;
      for(int byte = 0; byte < BLOCK_SIZE; ++byte){ //Steps through each byte in the block
        block.data.data[byte] = 0; //Sets the byte to 0
      }
      if(vdisk_write_block(num_block, &block) != 0){ //Writes the block to the disk
        return -1;
      }
    }
  return 0;
}

int initalize_master_block(){
      BLOCK masterBlock;
      for(int i = 0; i <= N_INODE_BLOCKS + 1; ++i){ // Steps through master block, inode blocks, and first data block
        //https://stackoverflow.com/questions/6848617/memory-efficient-flag-array-in-c
        masterBlock.master.block_allocated_flag[i/8] |= (1 << (i % 8)); //Marks corresponding bits as allocated
      }
      masterBlock.master.inode_allocated_flag[0] |= (1 << (0)); //Marks first inode as allocated
      if(vdisk_write_block(0, &masterBlock) != 0){ //Writes the block to the disk
        return -1;
      }

  return 0;
}

int initialize_first_inode(){
    //Creates an inode
    INODE firstInode;
    firstInode.type = IT_DIRECTORY; //with type directory
    firstInode.n_references = 1; //with one reference
    firstInode.data[0] = N_INODE_BLOCKS + 1; //points to the first data block, which is after all inode blocks
    for(int i = 1; i < BLOCKS_PER_INODE; ++i){
        firstInode.data[i] = UNALLOCATED_BLOCK; //All other block are unallocated in this inode
    }
    firstInode.size = 2; //Size of this inode is 2, for '.' and '..'

    BLOCK firstInodeBlock;
    firstInodeBlock.inodes.inode[0] = firstInode; //Assigns this inode to an inode block

    if(vdisk_write_block(1, &firstInodeBlock) != 0){ //Writes the inode block to block 1, the first block after master
      return -1;
    }

    return 0;
}

// This function is basically the same as 'oufs_clean_directory_block', but it's working
// and I do not want to change it.
int initialize_first_directory(){

  //Creates the current directory
  DIRECTORY_ENTRY currentDir;
  char* curDirName = "."; //name of '.'
  strcpy(currentDir.name, curDirName); //Assigns the name to the directory entry
  currentDir.inode_reference = 0; //This directory references the first inode(index 0)

  //Creates the parent directory
  DIRECTORY_ENTRY parentDir;
  char* parentDirName = ".."; //name of '..'
  strcpy(parentDir.name, parentDirName); //Assigns the name to the directory entry
  parentDir.inode_reference = 0; //This directory still references the first inode(index 0)

  //Adds both of these directories to a block
  BLOCK directoryBlock;
  directoryBlock.directory.entry[0] = currentDir;
  directoryBlock.directory.entry[1] = parentDir;

  //Marks the rest of the directories in this block as UNALLOCATED_INODE
  for(int i = 2; i < DIRECTORY_ENTRIES_PER_BLOCK; ++i){
    directoryBlock.directory.entry[i].inode_reference = UNALLOCATED_INODE;
  }

  //Writes this block to the disk
  if(vdisk_write_block(N_INODE_BLOCKS + 1, &directoryBlock) != 0){
    return -1;
  }

  return 0;
}
