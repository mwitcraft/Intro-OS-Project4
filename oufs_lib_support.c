#include <stdlib.h>
#include <libgen.h>
#include "oufs_lib.h"

#define debug 0

/**
 * Read the ZPWD and ZDISK environment variables & copy their values into cwd and disk_name.
 * If these environment variables are not set, then reasonable defaults are given.
 *
 * @param cwd String buffer in which to place the OUFS current working directory.
 * @param disk_name String buffer containing the file name of the virtual disk.
 */
void oufs_get_environment(char *cwd, char *disk_name)
{
  // Current working directory for the OUFS
  char *str = getenv("ZPWD");
  if(str == NULL) {
    // Provide default
    strcpy(cwd, "/");
  }else{
    // Exists
    strncpy(cwd, str, MAX_PATH_LENGTH-1);
  }

  // Virtual disk location
  str = getenv("ZDISK");
  if(str == NULL) {
    // Default
    strcpy(disk_name, "vdisk1");
  }else{
    // Exists: copy
    strncpy(disk_name, str, MAX_PATH_LENGTH-1);
  }

}

/**
 * Configure a directory entry so that it has no name and no inode
 *
 * @param entry The directory entry to be cleaned
 */
void oufs_clean_directory_entry(DIRECTORY_ENTRY *entry)
{
  entry->name[0] = 0;  // No name
  entry->inode_reference = UNALLOCATED_INODE;
}

/**
 * Initialize a directory block as an empty directory
 *
 * @param self Inode reference index for this directory
 * @param self Inode reference index for the parent directory
 * @param block The block containing the directory contents
 *
 */

void oufs_clean_directory_block(INODE_REFERENCE self, INODE_REFERENCE parent, BLOCK *block)
{
  // Debugging output
  if(debug)
    fprintf(stderr, "New clean directory: self=%d, parent=%d\n", self, parent);

  // Create an empty directory entry
  DIRECTORY_ENTRY entry;
  oufs_clean_directory_entry(&entry);

  // Copy empty directory entries across the entire directory list
  for(int i = 0; i < DIRECTORY_ENTRIES_PER_BLOCK; ++i) {
    block->directory.entry[i] = entry;
  }

  // Now we will set up the two fixed directory entries

  // Self
  strncpy(entry.name, ".", 2);
  entry.inode_reference = self;
  block->directory.entry[0] = entry;

  // Parent (same as self
  strncpy(entry.name, "..", 3);
  entry.inode_reference = parent;
  block->directory.entry[1] = entry;

}

/**
 * Allocate a new data block
 *
 * If one is found, then the corresponding bit in the block allocation table is set
 *
 * @return The index of the allocated data block.  If no blocks are available,
 * then UNALLOCATED_BLOCK is returned
 *
 */
BLOCK_REFERENCE oufs_allocate_new_block()
{
  BLOCK block;
  // Read the master block
  vdisk_read_block(MASTER_BLOCK_REFERENCE, &block);

  // Scan for an available block
  int block_byte;
  int flag;

  // Loop over each byte in the allocation table.
  for(block_byte = 0, flag = 1; flag && block_byte < N_BLOCKS_IN_DISK / 8; ++block_byte) {
    if(block.master.block_allocated_flag[block_byte] != 0xff) {
      // Found a byte that has an opening: stop scanning
      flag = 0;
      break;
    };
  };
  // Did we find a candidate byte in the table?
  if(flag == 1) {
    // No
    if(debug)
      fprintf(stderr, "No blocks\n");
    return(UNALLOCATED_BLOCK);
  }

  // Found an available data block

  // Set the block allocated bit
  // Find the FIRST bit in the byte that is 0 (we scan in bit order: 0 ... 7)
  int block_bit = oufs_find_open_bit(block.master.block_allocated_flag[block_byte]);

  // Now set the bit in the allocation table
  block.master.block_allocated_flag[block_byte] |= (1 << block_bit);

  // Write out the updated master block
  vdisk_write_block(MASTER_BLOCK_REFERENCE, &block);

  if(debug)
    fprintf(stderr, "Allocating block=%d (%d)\n", block_byte, block_bit);

  // Compute the block index
  BLOCK_REFERENCE block_reference = (block_byte << 3) + block_bit;

  if(debug)
    fprintf(stderr, "Allocating block=%d\n", block_reference);

  // Done
  return(block_reference);
}


/**
 *  Given an inode reference, read the inode from the virtual disk.
 *
 *  @param i Inode reference (index into the inode list)
 *  @param inode Pointer to an inode memory structure.  This structure will be
 *                filled in before return)
 *  @return 0 = successfully loaded the inode
 *         -1 = an error has occurred
 *
 */
int oufs_read_inode_by_reference(INODE_REFERENCE i, INODE *inode)
{
  if(debug)
    fprintf(stderr, "Fetching inode %d\n", i);

  // Find the address of the inode block and the inode within the block
  BLOCK_REFERENCE block = i / INODES_PER_BLOCK + 1;
  int element = (i % INODES_PER_BLOCK);

  BLOCK b;
  if(vdisk_read_block(block, &b) == 0) {
    // Successfully loaded the block: copy just this inode
    *inode = b.inodes.inode[element];
    return(0);
  }
  // Error case
  return(-1);
}

// WIll need to come back and complete
int oufs_find_open_bit(unsigned char value){
  int bit = -1;
  for(int i = 0; i < 8; ++i){
      if(!((1 << i) & value)){
        bit = i;
        break;
      }
  }
  return bit;
}

//Creats a new directory in the virtual file system
int oufs_mkdir(char* cwd, char* path){

  //Opens the target to create the new directory's absolute path
  //If the path supplied is an absolute path, just that is used
  char fullPath[strlen(cwd) + strlen(path)];
  fullPath[0] = '\0';
  if(path[0] == '/'){
    strcat(fullPath, path);
  }
  else{
    strcat(fullPath, cwd);
    strcat(fullPath, path);
  }

  //Gets the parent of the new directory's path by calling dirname on fullPath
  char dirnamePath[strlen(fullPath)];
  strncpy(dirnamePath, fullPath, strlen(fullPath));
  dirnamePath[strlen(fullPath)] = '\0';
  strncpy(dirnamePath, dirname(dirnamePath), strlen(dirnamePath));
  dirnamePath[strlen(dirnamePath)] = '\0';

  //Gets the name of new directory by calling basename on fullPath
  char basenamePath[strlen(fullPath)];
  strncpy(basenamePath, fullPath, strlen(fullPath));
  basenamePath[strlen(fullPath)] = '\0';
  strncpy(basenamePath, basename(basenamePath), strlen(basenamePath));
  basenamePath[strlen(basename(basenamePath))] = '\0';

  //If the directory already exists, throw an error
  if(get_inode_reference_from_path(fullPath) != -1){
    fprintf(stderr, "ERROR: Directory already exists\n");
    return -1;
  }

  //If the parent directory does not exist, throw an error
  int parentInodeReference = get_inode_reference_from_path(dirnamePath);
  if(parentInodeReference == -1){
    fprintf(stderr, "ERROR: parent does not exist\n");
    return -1;
  }

  //Open the parent inode and increment the size
  int parentInodeBlockReference = parentInodeReference / INODES_PER_BLOCK + 1; //Gets what block the inode is in
  int parentInodeBlockIndex = parentInodeReference % INODES_PER_BLOCK; //Gets where in the block the inode is
  BLOCK parentBlock;
  vdisk_read_block(parentInodeBlockReference, &parentBlock); //Open block
  if(parentBlock.inodes.inode[parentInodeBlockIndex].size > 15){
    fprintf(stderr, "ERROR: Block full\n");
    return -1;
  }
  ++parentBlock.inodes.inode[parentInodeBlockIndex].size; //Increments the block's size
  vdisk_write_block(parentInodeBlockReference, &parentBlock); //Writes the block back to the disk

  BLOCK_REFERENCE parentDataBlockReference;
  INODE parentInode;
  oufs_read_inode_by_reference(parentInodeReference, &parentInode);
  for(int i = 0; i < BLOCKS_PER_INODE; ++i){
      if(parentInode.data[i] != UNALLOCATED_BLOCK){
        BLOCK_REFERENCE parentDataBlockRef = parentInode.data[i];
        BLOCK parentDataBlock;
        vdisk_read_block(parentDataBlockRef, &parentDataBlock);
        for(int j = 0; j < DIRECTORY_ENTRIES_PER_BLOCK; ++j){
          if(parentDataBlock.directory.entry[j].inode_reference == UNALLOCATED_INODE){
              parentDataBlockReference = parentDataBlockRef;
              break;
          }
        }
      }
  }

  //Gets the location of the next available inode
  INODE_REFERENCE newInodeInodeReference = -1;
  for(int i = 0; i < N_INODES; ++i){
    INODE inode;
    oufs_read_inode_by_reference(i, &inode);
    if(inode.size == 0){
      newInodeInodeReference = i;
      break;
    }
  }

  //Gets the byte and bit of the new location so can assign values later
  int byte = newInodeInodeReference / INODES_PER_BLOCK + 1;
  int bit = newInodeInodeReference % INODES_PER_BLOCK;

  //Byte corresponds to the BLOCK_REFERENCE
  BLOCK_REFERENCE newInodeInodeBlockReference = byte;
  //Allocates a new block for this information and returns the location of that block
  BLOCK_REFERENCE newInodeDataBlockReference = oufs_allocate_new_block();

  //Opens a new block at the location referenced above
  BLOCK newInodeBlock;
  vdisk_read_block(newInodeInodeBlockReference, &newInodeBlock);

  //Fills in the inode information in the new block
  newInodeBlock.inodes.inode[bit].type = IT_DIRECTORY;
  newInodeBlock.inodes.inode[bit].n_references = 1;
  newInodeBlock.inodes.inode[bit].data[0] = newInodeDataBlockReference;
  for(int i = 1; i < BLOCKS_PER_INODE; ++i){
      newInodeBlock.inodes.inode[bit].data[i] = UNALLOCATED_BLOCK;
  }
  newInodeBlock.inodes.inode[bit].size = 2;

  BLOCK parentDataBlock;
  vdisk_read_block(parentDataBlockReference, &parentDataBlock);

  //Adds the new directory to the parent inode's data block
  for(int i = 0; i < DIRECTORY_ENTRIES_PER_BLOCK; ++i){
    if(parentDataBlock.directory.entry[i].inode_reference == UNALLOCATED_INODE){ //Finds first available directory
      strncpy(parentDataBlock.directory.entry[i].name, basenamePath, strlen(basenamePath)); // Writes name
      parentDataBlock.directory.entry[i].inode_reference = newInodeInodeReference; //and inode reference
      break;
    }
  }

  //Creates a brand new empty directory data block
  BLOCK newInodeDataBlock;
  oufs_clean_directory_block(newInodeInodeReference, parentInodeReference, &newInodeDataBlock);

  //Writes all changed blocks back to disk
  vdisk_write_block(newInodeInodeBlockReference, &newInodeBlock);
  vdisk_write_block(parentDataBlockReference, &parentDataBlock);
  vdisk_write_block(newInodeDataBlockReference, &newInodeDataBlock);

  //Opens master block to change allocation table to mark new inode as allocated
  BLOCK masterBlock;
  vdisk_read_block(0, &masterBlock);
  masterBlock.master.inode_allocated_flag[byte - 1] |= (1 << (bit));
  vdisk_write_block(0, &masterBlock);

  return 0;
}

//Removes a specified *empty directory from the virtual disk
int oufs_rmdir(char *cwd, char *path){

  //Creates the absolute path to the directory to be removed
  //If the path supplied is an absolute path, just that is used
  char fullPath[strlen(cwd) + strlen(path)];
  fullPath[0] = '\0';
  if(path[0] == '/'){
    strcat(fullPath, path);
  }
  else{
    strcat(fullPath, cwd);
    strcat(fullPath, path);
  }

  //If trying to remove root directory, throw error
  if(!strcmp(fullPath, "//") || !strcmp(fullPath, "/")){
    fprintf(stderr, "ERROR: cannot delete root directory\n");
    return -1;
  }

  //If the inode does not exist, throw an error
  int inodeToRemoveReference = get_inode_reference_from_path(fullPath);
  if(inodeToRemoveReference == -1){
    fprintf(stderr, "Path does not exist\n");
  }
  else{
    //Open the inode
    INODE inodeToRemove;
    oufs_read_inode_by_reference(inodeToRemoveReference, &inodeToRemove);

    //If the directory is not empty, throw error
    if(inodeToRemove.size > 2){
      fprintf(stderr, "ERROR: Directory not empty\n");
      return -1;
    }

    //Get parent inode reference
    int parentInodeReference;
    for(int i = 0; i < BLOCKS_PER_INODE; ++i){
      if(inodeToRemove.data[i] != UNALLOCATED_BLOCK){ //Step through available blocks referenced in data
        int ref = inodeToRemove.data[i];
        BLOCK block;
        vdisk_read_block(ref, &block); //Open allocated block
        for(int j = 0; j < DIRECTORY_ENTRIES_PER_BLOCK; ++j){ //Step through entries in the block
          if(!strcmp(block.directory.entry[j].name, "..")){ //If the entry's name is '..' (meaning parent)...
            parentInodeReference = block.directory.entry[j].inode_reference; // Store the parent inode reference

            //Mark the inode and the inode's data block as unallocated in the master block allocation tables
            BLOCK masterBlock;
            vdisk_read_block(MASTER_BLOCK_REFERENCE, &masterBlock);
            masterBlock.master.block_allocated_flag[ref / 8] &= ~(1  << (ref % 8));
            masterBlock.master.inode_allocated_flag[inodeToRemoveReference / 8] &= ~(1 << (inodeToRemoveReference % 8));
            vdisk_write_block(MASTER_BLOCK_REFERENCE, &masterBlock);
            break;
          }
        }
      }
    }

    //Get the parent inode location information
    int parentInodeBlockReference = parentInodeReference / INODES_PER_BLOCK + 1;
    int parentInodeBlockIndex = parentInodeReference % INODES_PER_BLOCK;

    //Open the parent block
    BLOCK parentBlock;
    vdisk_read_block(parentInodeBlockReference, &parentBlock);

    for(int i = 0; i < BLOCKS_PER_INODE; ++i){ //Step through all blocks in the inode
      int ref = parentBlock.inodes.inode[parentInodeBlockIndex].data[i];
      if(ref != UNALLOCATED_BLOCK){ //If the block has data in it
        BLOCK block;
        vdisk_read_block(ref, &block);
        for(int j = 0; j < DIRECTORY_ENTRIES_PER_BLOCK; ++j){ //Step through the directory entries in the block
          int inodeRef = block.directory.entry[j].inode_reference;
          if(inodeRef == inodeToRemoveReference){ //if the inode referenced by a directory entry is the inode being removed...
            memset(block.directory.entry[j].name, 0, strlen(block.directory.entry[j].name)); //Empty the name in the data block
            block.directory.entry[j].inode_reference = UNALLOCATED_INODE; //Mark the inode as unallocated
            vdisk_write_block(ref, &block); //Write the block back to the disk
            parentBlock.inodes.inode[parentInodeBlockReference].data[ref] = UNALLOCATED_BLOCK;
          }
        }
      }
    }
    //Decrement the parent inode's size
    --parentBlock.inodes.inode[parentInodeBlockIndex].size;
    vdisk_write_block(parentInodeBlockReference, &parentBlock); //Write that block back

    //Get the location information of the inode being removed
    int inodeBlockReference = inodeToRemoveReference / INODES_PER_BLOCK + 1;
    int index = inodeToRemoveReference % INODES_PER_BLOCK;

    //Open the block containing the inode
    BLOCK inodeBlock;
    vdisk_read_block(inodeBlockReference, &inodeBlock);

    for(int i = 0; i < BLOCKS_PER_INODE; ++i){
      int dirBlockRef = 0;
      BLOCK dirBlock;
      dirBlockRef = inodeBlock.inodes.inode[index].data[i];
      if(dirBlockRef != UNALLOCATED_BLOCK){
        vdisk_read_block(dirBlockRef, &dirBlock);
        for(int j = 0; j < DIRECTORY_ENTRIES_PER_BLOCK; ++j){
            memset(dirBlock.directory.entry[j].name, 0, strlen(dirBlock.directory.entry[j].name)); //Empty the name in the data block
            dirBlock.directory.entry[j].inode_reference = 0;
        }
        vdisk_write_block(dirBlockRef, &dirBlock);
      }
    }

    //Go to that specific inode and 0 everything out
    inodeBlock.inodes.inode[index].type = 0;
    inodeBlock.inodes.inode[index].n_references = 0;
    for(int i = 0; i < BLOCKS_PER_INODE; ++i){
      inodeBlock.inodes.inode[index].data[i] = 0;
    }
    inodeBlock.inodes.inode[index].size = 0;

    //Write the block that contains the removed inode back to the disk
    vdisk_write_block(inodeBlockReference, &inodeBlock);
  }

  return 0;
}

// Lists the files and directories inside a specific directory
int oufs_list(char *cwd, char *path){
  //Creates the full path to the target (directory whose children are listed)
  char fullPath[strlen(cwd) + strlen(path)];
  fullPath[0] = '\0';
  if(path[0] == '/'){
    strcat(fullPath, path);
  }
  else{
    strcat(fullPath, cwd);
    strcat(fullPath, path);
  }

  //Gets the inode reference from the path, opens the inode, and stores in 'inode'
  INODE inode;
  int inodeReference = get_inode_reference_from_path(fullPath);//Gets inode reference from path
  if(inodeReference == -1){
    fprintf(stderr, "ERROR: Directory does not exist\n");
    return -1;
  }
  else{
    oufs_read_inode_by_reference(inodeReference, &inode);//Opens inode
  }

  //Stores directory names from inode in array
  char* dirNames[DIRECTORY_ENTRIES_PER_BLOCK];
  //Initialize the array
  for(int i = 0; i < DIRECTORY_ENTRIES_PER_BLOCK; ++i){
    dirNames[i] = "";
  }
  for(int i = 0; i < BLOCKS_PER_INODE; ++i){ //Step through each block in the inode
    BLOCK block;
    if(inode.data[i] != UNALLOCATED_BLOCK){ //If the block in the inode points to a valid data block
      vdisk_read_block(inode.data[i], &block); //Open the block
      for(int j = 0; j < DIRECTORY_ENTRIES_PER_BLOCK; ++j){//Step through the block
        if(block.directory.entry[j].inode_reference != UNALLOCATED_INODE){//If the block contains valid directories
          dirNames[j] = block.directory.entry[j].name; //Store the directory name in the array
        }
      }
    }
  }

  //Sorts the directory names in alphabetical order and prints out
  qsort(dirNames, DIRECTORY_ENTRIES_PER_BLOCK, sizeof(char*), comparator);
  for(int i = 0; i < DIRECTORY_ENTRIES_PER_BLOCK; ++i){
    if(strcmp(dirNames[i], "")){//If the name is not empty
      printf("%s/\n", dirNames[i]);//Print it out
      fflush(stdout);
    }
  }
  return 0;
}

int get_inode_reference_from_path(char* path){

    int currentInodeReference = 0;
    char* token = strtok(path, "/");
    while(token != NULL){
        currentInodeReference = get_inode_reference_from_path_helper(currentInodeReference, token);
        if(currentInodeReference == -1){
          return -1;
        }
        token = strtok(NULL, "/");
    }
    return currentInodeReference;
}

int get_inode_reference_from_path_helper(INODE_REFERENCE parentInodeReference, char* name){

  if(!strcmp(name, "/")){
    return 0;
  }

  int returner = -1;
  INODE inode;
  oufs_read_inode_by_reference(parentInodeReference, &inode);
  for(int i = 0; i < BLOCKS_PER_INODE; ++i){
    if(inode.data[i] != UNALLOCATED_BLOCK){
      BLOCK_REFERENCE currentBlockRef = inode.data[i];
      BLOCK dirBlock;
      vdisk_read_block(currentBlockRef, &dirBlock);
      for(int j = 0; j < DIRECTORY_ENTRIES_PER_BLOCK; ++j){
        if(dirBlock.directory.entry[j].inode_reference != UNALLOCATED_INODE){
          if(!strncmp(dirBlock.directory.entry[j].name, name, strlen(name))){
              returner = dirBlock.directory.entry[j].inode_reference;
              break;
          }
        }
      }
    }
  }
  return returner;
}

// https://stackoverflow.com/questions/43099269/qsort-function-in-c-used-to-compare-an-array-of-strings
//Sorts an array in alphabetical order
int comparator(const void* p, const void* q){
  char* a = *(char**)p; //Opens the void pointers as char*
  char* b = *(char**)q;
  return strcmp(a, b); //Sorts alphabetically
}
