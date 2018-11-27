#include "oufs_lib.h"
#include <libgen.h>
#include <stdlib.h>

#define debug 0

/**
 * Read the ZPWD and ZDISK environment variables & copy their values into cwd
 * and disk_name. If these environment variables are not set, then reasonable
 * defaults are given.
 *
 * @param cwd String buffer in which to place the OUFS current working
 * directory.
 * @param disk_name String buffer containing the file name of the virtual disk.
 */
void oufs_get_environment(char *cwd, char *disk_name) {
  // Current working directory for the OUFS
  char *str = getenv("ZPWD");
  if (str == NULL) {
    // Provide default
    strcpy(cwd, "/");
  } else {
    // Exists
    strncpy(cwd, str, MAX_PATH_LENGTH - 1);
  }

  // Virtual disk location
  str = getenv("ZDISK");
  if (str == NULL) {
    // Default
    strncpy(disk_name, "vdisk1", strlen("vdisk1"));
  } else {
    // Exists: copy
    strncpy(disk_name, str, strlen(str));
  }
}

/**
 * Configure a directory entry so that it has no name and no inode
 *
 * @param entry The directory entry to be cleaned
 */
void oufs_clean_directory_entry(DIRECTORY_ENTRY *entry) {
  entry->name[0] = 0; // No name
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

void oufs_clean_directory_block(INODE_REFERENCE self, INODE_REFERENCE parent,
                                BLOCK *block) {
  // Debugging output
  if (debug)
    fprintf(stderr, "New clean directory: self=%d, parent=%d\n", self, parent);

  // Create an empty directory entry
  DIRECTORY_ENTRY entry;
  oufs_clean_directory_entry(&entry);

  // Copy empty directory entries across the entire directory list
  for (int i = 0; i < DIRECTORY_ENTRIES_PER_BLOCK; ++i) {
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
 * If one is found, then the corresponding bit in the block allocation table is
 * set
 *
 * @return The index of the allocated data block.  If no blocks are available,
 * then UNALLOCATED_BLOCK is returned
 *
 */
BLOCK_REFERENCE oufs_allocate_new_block() {
  BLOCK block;
  // Read the master block
  vdisk_read_block(MASTER_BLOCK_REFERENCE, &block);

  // Scan for an available block
  int block_byte;
  int flag;

  // Loop over each byte in the allocation table.
  for (block_byte = 0, flag = 1; flag && block_byte < N_BLOCKS_IN_DISK / 8;
       ++block_byte) {
    if (block.master.block_allocated_flag[block_byte] != 0xff) {
      // Found a byte that has an opening: stop scanning
      flag = 0;
      break;
    };
  };
  // Did we find a candidate byte in the table?
  if (flag == 1) {
    // No
    if (debug)
      fprintf(stderr, "No blocks\n");
    return (UNALLOCATED_BLOCK);
  }

  // Found an available data block

  // Set the block allocated bit
  // Find the FIRST bit in the byte that is 0 (we scan in bit order: 0 ... 7)
  int block_bit =
      oufs_find_open_bit(block.master.block_allocated_flag[block_byte]);

  // Now set the bit in the allocation table
  block.master.block_allocated_flag[block_byte] |= (1 << block_bit);

  // Write out the updated master block
  vdisk_write_block(MASTER_BLOCK_REFERENCE, &block);

  if (debug)
    fprintf(stderr, "Allocating block=%d (%d)\n", block_byte, block_bit);

  // Compute the block index
  BLOCK_REFERENCE block_reference = (block_byte << 3) + block_bit;

  if (debug)
    fprintf(stderr, "Allocating block=%d\n", block_reference);

  // Done
  return (block_reference);
}

INODE_REFERENCE oufs_allocate_new_directory(INODE_REFERENCE parent) {
  // Find available inode, get reference, will be self
  // int self = -1;
  // for(int i = 0; i < N_INODES; ++i){
  //   INODE inode;
  //   oufs_read_inode_by_reference(i, &inode);
  //   if(inode.size == 0){
  //     self = i;
  //     break;
  //   }
  // }
  // if(self == -1)
  //   return UNALLOCATED_INODE;
  BLOCK masterBlock;
  vdisk_read_block(MASTER_BLOCK_REFERENCE, &masterBlock);
  int b_byte;
  int flag;
  for (b_byte = 0, flag = 1; flag && b_byte < INODES_PER_BLOCK; ++b_byte) {
    if (masterBlock.master.inode_allocated_flag[b_byte] != 0xff) {
      flag = 0;
      break;
    }
  }

  int b_bit =
      oufs_find_open_bit(masterBlock.master.inode_allocated_flag[b_byte]);
  INODE_REFERENCE self = (b_byte << 3) + b_bit;

  // Allcoate new block
  BLOCK_REFERENCE b;
  b = oufs_allocate_new_block();

  // Call clean directory block
  BLOCK block;
  oufs_clean_directory_block(self, parent, &block);

  // Write information to self
  INODE inode;
  inode.type = IT_DIRECTORY;
  inode.n_references = 1;
  inode.data[0] = b;
  for (int i = 1; i < BLOCKS_PER_INODE; ++i) {
    inode.data[i] = UNALLOCATED_BLOCK;
  }
  inode.size = 2;
  oufs_write_inode_by_reference(self, &inode);

  // Store clean directory block in new block
  vdisk_write_block(b, &block);

  // Mark inode as allocated in master block
  int byte = self / INODES_PER_BLOCK + 1;
  int bit = self % INODES_PER_BLOCK;
  BLOCK master;
  vdisk_read_block(0, &master);
  master.master.inode_allocated_flag[byte - 1] |= (1 << (bit));
  vdisk_write_block(0, &master);

  return self;
}

INODE_REFERENCE oufs_find_directory_element(INODE *inode, char *name) {
  for (int i = 0; i < BLOCKS_PER_INODE; ++i) {
    if (inode->data[i] != UNALLOCATED_BLOCK) {
      BLOCK_REFERENCE ref = inode->data[i];
      BLOCK b;
      vdisk_read_block(ref, &b);
      for (int j = 0; j < DIRECTORY_ENTRIES_PER_BLOCK; ++j) {
        // if(b.directory.entry[j].name == name){
        //   return b.directory.entry[j].inode_reference;
        // }
        if (!strcmp(b.directory.entry[j].name, name)) {
          return b.directory.entry[j].inode_reference;
        }
      }
    }
  }
  return UNALLOCATED_INODE;
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
int oufs_read_inode_by_reference(INODE_REFERENCE i, INODE *inode) {
  if (debug)
    fprintf(stderr, "Fetching inode %d\n", i);

  // Find the address of the inode block and the inode within the block
  BLOCK_REFERENCE block = i / INODES_PER_BLOCK + 1;
  int element = (i % INODES_PER_BLOCK);

  BLOCK b;
  if (vdisk_read_block(block, &b) == 0) {
    // Successfully loaded the block: copy just this inode
    *inode = b.inodes.inode[element];
    return (0);
  }
  // Error case
  return (-1);
}

int oufs_write_inode_by_reference(INODE_REFERENCE i, INODE *inode) {
  BLOCK_REFERENCE block = i / INODES_PER_BLOCK + 1;
  int element = i % INODES_PER_BLOCK;

  BLOCK b;
  if (vdisk_read_block(block, &b) == 0) {
    b.inodes.inode[element].type = inode->type;
    b.inodes.inode[element].n_references = inode->n_references;
    for (int i = 0; i < BLOCKS_PER_INODE; ++i) {
      b.inodes.inode[element].data[i] = inode->data[i];
    }
    b.inodes.inode[element].size = inode->size;
  }

  vdisk_write_block(block, &b);
  return 0;
}

// WIll need to come back and complete
int oufs_find_open_bit(unsigned char value) {
  int bit = -1;
  for (int i = 0; i < 8; ++i) {
    if (!((1 << i) & value)) {
      bit = i;
      break;
    }
  }
  return bit;
}

// Removes a specified *empty directory from the virtual disk
int oufs_rmdir(char *cwd, char *path) {

  // Creates the absolute path to the directory to be removed
  // If the path supplied is an absolute path, just that is used
  char fullPath[strlen(cwd) + strlen(path)];
  fullPath[0] = '\0';
  if (path[0] == '/') {
    strcat(fullPath, path);
  } else {
    strcat(fullPath, cwd);
    strcat(fullPath, path);
  }

  // If trying to remove root directory, throw error
  if (!strcmp(fullPath, "//") || !strcmp(fullPath, "/")) {
    fprintf(stderr, "ERROR: cannot delete root directory\n");
    return -1;
  }

  // If the inode does not exist, throw an error
  int inodeToRemoveReference = get_inode_reference_from_path(fullPath);
  if (inodeToRemoveReference == -1) {
    fprintf(stderr, "Path does not exist\n");
  } else {
    // Open the inode
    INODE inodeToRemove;
    oufs_read_inode_by_reference(inodeToRemoveReference, &inodeToRemove);

    // If the directory is not empty, throw error
    if (inodeToRemove.size > 2) {
      fprintf(stderr, "ERROR: Directory not empty\n");
      return -1;
    }

    // Get parent inode reference
    int parentInodeReference;
    for (int i = 0; i < BLOCKS_PER_INODE; ++i) {
      if (inodeToRemove.data[i] !=
          UNALLOCATED_BLOCK) { // Step through available blocks referenced in
                               // data
        int ref = inodeToRemove.data[i];
        BLOCK block;
        vdisk_read_block(ref, &block); // Open allocated block
        for (int j = 0; j < DIRECTORY_ENTRIES_PER_BLOCK;
             ++j) { // Step through entries in the block
          if (!strcmp(
                  block.directory.entry[j].name,
                  "..")) { // If the entry's name is '..' (meaning parent)...
            parentInodeReference =
                block.directory.entry[j]
                    .inode_reference; // Store the parent inode reference

            // Mark the inode and the inode's data block as unallocated in the
            // master block allocation tables
            BLOCK masterBlock;
            vdisk_read_block(MASTER_BLOCK_REFERENCE, &masterBlock);
            masterBlock.master.block_allocated_flag[ref / 8] &=
                ~(1 << (ref % 8));
            masterBlock.master
                .inode_allocated_flag[inodeToRemoveReference / 8] &=
                ~(1 << (inodeToRemoveReference % 8));
            vdisk_write_block(MASTER_BLOCK_REFERENCE, &masterBlock);
            break;
          }
        }
      }
    }

    // Get the parent inode location information
    int parentInodeBlockReference = parentInodeReference / INODES_PER_BLOCK + 1;
    int parentInodeBlockIndex = parentInodeReference % INODES_PER_BLOCK;

    // Open the parent block
    BLOCK parentBlock;
    vdisk_read_block(parentInodeBlockReference, &parentBlock);

    for (int i = 0; i < BLOCKS_PER_INODE;
         ++i) { // Step through all blocks in the inode
      int ref = parentBlock.inodes.inode[parentInodeBlockIndex].data[i];
      if (ref != UNALLOCATED_BLOCK) { // If the block has data in it
        BLOCK block;
        vdisk_read_block(ref, &block);
        for (int j = 0; j < DIRECTORY_ENTRIES_PER_BLOCK;
             ++j) { // Step through the directory entries in the block
          int inodeRef = block.directory.entry[j].inode_reference;
          if (inodeRef == inodeToRemoveReference) { // if the inode referenced
                                                    // by a directory entry is
                                                    // the inode being removed...
            memset(block.directory.entry[j].name, 0,
                   strlen(block.directory.entry[j]
                              .name)); // Empty the name in the data block
            block.directory.entry[j].inode_reference =
                UNALLOCATED_INODE;          // Mark the inode as unallocated
            vdisk_write_block(ref, &block); // Write the block back to the disk
            parentBlock.inodes.inode[parentInodeBlockReference].data[ref] =
                UNALLOCATED_BLOCK;
          }
        }
      }
    }
    // Decrement the parent inode's size
    --parentBlock.inodes.inode[parentInodeBlockIndex].size;
    vdisk_write_block(parentInodeBlockReference,
                      &parentBlock); // Write that block back

    // Get the location information of the inode being removed
    int inodeBlockReference = inodeToRemoveReference / INODES_PER_BLOCK + 1;
    int index = inodeToRemoveReference % INODES_PER_BLOCK;

    // Open the block containing the inode
    BLOCK inodeBlock;
    vdisk_read_block(inodeBlockReference, &inodeBlock);

    for (int i = 0; i < BLOCKS_PER_INODE; ++i) {
      int dirBlockRef = 0;
      BLOCK dirBlock;
      dirBlockRef = inodeBlock.inodes.inode[index].data[i];
      if (dirBlockRef != UNALLOCATED_BLOCK) {
        vdisk_read_block(dirBlockRef, &dirBlock);
        for (int j = 0; j < DIRECTORY_ENTRIES_PER_BLOCK; ++j) {
          memset(dirBlock.directory.entry[j].name, 0,
                 strlen(dirBlock.directory.entry[j]
                            .name)); // Empty the name in the data block
          dirBlock.directory.entry[j].inode_reference = 0;
        }
        vdisk_write_block(dirBlockRef, &dirBlock);
      }
    }

    // Go to that specific inode and 0 everything out
    inodeBlock.inodes.inode[index].type = 0;
    inodeBlock.inodes.inode[index].n_references = 0;
    for (int i = 0; i < BLOCKS_PER_INODE; ++i) {
      inodeBlock.inodes.inode[index].data[i] = 0;
    }
    inodeBlock.inodes.inode[index].size = 0;

    // Write the block that contains the removed inode back to the disk
    vdisk_write_block(inodeBlockReference, &inodeBlock);
  }

  return 0;
}

// Lists the files and directories inside a specific directory
int oufs_list(char *cwd, char *path) {

  INODE_REFERENCE parent;
  INODE_REFERENCE child;
  char local_name[MAX_PATH_LENGTH];
  int ret;

  if ((ret = oufs_find_file(cwd, path, &parent, &child, local_name)) < -1) {
    fprintf(stderr, "zfilez error\n");
  }

  if(child == UNALLOCATED_INODE){
    fprintf(stderr, "zfilez error: directory does not exist\n");
    return -1;
  }

  char *entryNames[DIRECTORY_ENTRIES_PER_BLOCK];
  char *directoryNames[DIRECTORY_ENTRIES_PER_BLOCK];
  for (int i = 0; i < DIRECTORY_ENTRIES_PER_BLOCK; ++i) {
    entryNames[i] = "";
    directoryNames[i] = "";
  }

  INODE inode;
  oufs_read_inode_by_reference(child, &inode);

  for (int i = 0; i < BLOCKS_PER_INODE; ++i) {
    if (inode.data[i] != UNALLOCATED_BLOCK) {
      BLOCK block;
      vdisk_read_block(inode.data[i], &block);
      for (int j = 0; j < DIRECTORY_ENTRIES_PER_BLOCK; ++j) {
        if (block.directory.entry[j].inode_reference != UNALLOCATED_INODE) {
          INODE curInode;
          oufs_read_inode_by_reference(block.directory.entry[j].inode_reference,
                                       &curInode);
          if (curInode.type == IT_DIRECTORY) {
            entryNames[j] = strdup(block.directory.entry[j].name);
            directoryNames[j] = strdup(block.directory.entry[j].name);
          } else {
            entryNames[j] = strdup(block.directory.entry[j].name);
          }
        }
      }
    }
  }

  // Sorts the directory names in alphabetical order and prints out
  qsort(entryNames, DIRECTORY_ENTRIES_PER_BLOCK, sizeof(char *), comparator);
  for (int i = 0; i < DIRECTORY_ENTRIES_PER_BLOCK; ++i) {
    if (strcmp(entryNames[i], "")) { // If the name is not empty
      int flag = 1;
      for(int j = 0; flag && j < DIRECTORY_ENTRIES_PER_BLOCK; ++j){ //Determines if entry is a directory or not
        if(!strcmp(entryNames[i], directoryNames[j])){
          flag = 0;
        }
      }
      if(flag){ //If entry is not a directory, do not add '/'
        printf("%s\n", entryNames[i]);
        fflush(stdout);
      }
      else{ //If entry is a directory, add '/'
        printf("%s/\n", entryNames[i]);
        fflush(stdout);
      }
    }
  }
  return 0;
}

int get_inode_reference_from_path(char *path) {

  int currentInodeReference = 0;
  char *token = strtok(path, "/");
  while (token != NULL) {
    currentInodeReference =
        get_inode_reference_from_path_helper(currentInodeReference, token);
    if (currentInodeReference == -1) {
      return -1;
    }
    token = strtok(NULL, "/");
  }
  return currentInodeReference;
}

int get_inode_reference_from_path_helper(INODE_REFERENCE parentInodeReference,
                                         char *name) {

  if (!strcmp(name, "/")) {
    return 0;
  }

  int returner = -1;
  INODE inode;
  oufs_read_inode_by_reference(parentInodeReference, &inode);
  for (int i = 0; i < BLOCKS_PER_INODE; ++i) {
    if (inode.data[i] != UNALLOCATED_BLOCK) {
      BLOCK_REFERENCE currentBlockRef = inode.data[i];
      BLOCK dirBlock;
      vdisk_read_block(currentBlockRef, &dirBlock);
      for (int j = 0; j < DIRECTORY_ENTRIES_PER_BLOCK; ++j) {
        if (dirBlock.directory.entry[j].inode_reference != UNALLOCATED_INODE) {
          if (!strncmp(dirBlock.directory.entry[j].name, name, strlen(name))) {
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
// Sorts an array in alphabetical order
int comparator(const void *p, const void *q) {
  char *a = *(char **)p; // Opens the void pointers as char*
  char *b = *(char **)q;
  return strcmp(a, b); // Sorts alphabetically
}

// Given Code from project 3
int oufs_find_file(char *cwd, char *path, INODE_REFERENCE *parent,
                   INODE_REFERENCE *child, char *local_name) {
  INODE_REFERENCE grandparent;
  char full_path[MAX_PATH_LENGTH];

  // Construct an absolute path the file/directory in question
  if (path[0] == '/') {
    strncpy(full_path, path, MAX_PATH_LENGTH - 1);
  } else {
    if (strlen(cwd) > 1) {
      strncpy(full_path, cwd, MAX_PATH_LENGTH - 1);
      strncat(full_path, "/", 2);
      strncat(full_path, path,
              MAX_PATH_LENGTH - 1 - strnlen(full_path, MAX_PATH_LENGTH));
    } else {
      strncpy(full_path, "/", 2);
      strncat(full_path, path, MAX_PATH_LENGTH - 2);
    }
  }

  if (debug) {
    fprintf(stderr, "Full path: %s\n", full_path);
  };

  // Start scanning from the root directory
  // Root directory inode
  grandparent = *parent = *child = 0;
  if (debug)
    fprintf(stderr, "Start search: %d\n", *parent);

  // Parse the full path
  char *directory_name;
  directory_name = strtok(full_path, "/");
  while (directory_name != NULL) {
    if (strlen(directory_name) >= FILE_NAME_SIZE - 1)
      // Truncate the name
      directory_name[FILE_NAME_SIZE - 1] = 0;
    if (debug) {
      fprintf(stderr, "Directory: %s\n", directory_name);
    }
    if (strlen(directory_name) != 0) {
      // We have a non-empty name
      // Remember this name
      if (local_name != NULL) {
        // Copy local name of file
        strncpy(local_name, directory_name, MAX_PATH_LENGTH - 1);
        // Make sure we have a termination
        local_name[MAX_PATH_LENGTH - 1] = 0;
      }

      // Real next element
      INODE inode;
      // Fetch the inode that corresponds to the child
      if (oufs_read_inode_by_reference(*child, &inode) != 0) {
        return (-3);
      }

      // Check the type of the inode
      if (inode.type != 'D') {
        // Parent is not a directory
        *parent = *child = UNALLOCATED_INODE;
        return (-2); // Not a valid directory
      }
      // Get the new inode that corresponds to the name by searching the current
      // directory
      INODE_REFERENCE new_inode =
          oufs_find_directory_element(&inode, directory_name);
      grandparent = *parent;
      *parent = *child;
      *child = new_inode;
      if (new_inode == UNALLOCATED_INODE) {
        // name not found
        //  Is there another (nontrivial) step in the path?
        //  Loop until end or we have found a nontrivial name
        do {
          directory_name = strtok(NULL, "/");
          if (directory_name != NULL &&
              strlen(directory_name) >= FILE_NAME_SIZE - 1)
            // Truncate the name
            directory_name[FILE_NAME_SIZE - 1] = 0;
        } while (directory_name != NULL && (strcmp(directory_name, "") == 0));

        if (directory_name != NULL) {
          // There are more sub-items - so the parent does not exist
          *parent = UNALLOCATED_INODE;
        };
        // Directory/file does not exist
        return (-1);
      };
    }
    // Go on to the next directory
    directory_name = strtok(NULL, "/");
    if (directory_name != NULL && strlen(directory_name) >= FILE_NAME_SIZE - 1)
      // Truncate the name
      directory_name[FILE_NAME_SIZE - 1] = 0;
  };

  // Item found.
  if (*child == UNALLOCATED_INODE) {
    // We went too far - roll back one step ***
    *child = *parent;
    *parent = grandparent;
  }
  if (debug) {
    fprintf(stderr, "Found: %d, %d\n", *parent, *child);
  }
  // Success!
  return (0);
}

int oufs_mkdir(char *cwd, char *path) {
  INODE_REFERENCE parent;
  INODE_REFERENCE child;
  char local_name[MAX_PATH_LENGTH];
  int ret;

  // Attempt to find the specified directory
  if ((ret = oufs_find_file(cwd, path, &parent, &child, local_name)) < -1) {
    if (debug)
      fprintf(stderr, "oufs_mkdir(): ret = %d\n", ret);
    return (-1);
  };

  if (parent != UNALLOCATED_INODE && child == UNALLOCATED_INODE) {
    // Parent exists and child does not
    if (debug)
      fprintf(stderr, "oufs_mkdir(): parent=%d, child=%d\n", parent, child);

    // Get the parent inode
    INODE inode;
    if (oufs_read_inode_by_reference(parent, &inode) != 0) {
      return (-5);
    }
    if (debug) {
      printf("Find result: %d, %d\n", parent, child);
    }

    if (inode.type == 'D') {
      // Parent is a directory
      BLOCK block;
      // Read the directory
      if (vdisk_read_block(inode.data[0], &block) != 0) {
        return (-6);
      }
      // Find a hole in the directory entry list
      for (int i = 0; i < DIRECTORY_ENTRIES_PER_BLOCK; ++i) {
        if (block.directory.entry[i].inode_reference == UNALLOCATED_INODE) {
          // Found the hole: use this one
          if (debug)
            fprintf(stderr, "Making in parent inode: %d\n", parent);

          INODE_REFERENCE inode_reference = oufs_allocate_new_directory(parent);
          if (inode_reference == UNALLOCATED_INODE) {
            fprintf(stderr, "Disk is full\n");
            return (-4);
          }
          // Add the item to the current directory
          block.directory.entry[i].inode_reference = inode_reference;
          if (debug)
            fprintf(stderr, "new file: %s\n", local_name);
          strncpy(block.directory.entry[i].name, local_name,
                  FILE_NAME_SIZE - 1);
          block.directory.entry[i].name[FILE_NAME_SIZE - 1] = 0;

          // Write the block back out
          if (vdisk_write_block(inode.data[0], &block) != 0) {
            return (-7);
          }

          // Update the parent inode size
          inode.size++;

          // Write out the  parent inode
          if (oufs_write_inode_by_reference(parent, &inode) != 0) {
            return (-8);
          }

          // All done
          return (0);
        }
      }
      // No holes
      fprintf(stderr, "Parent is full\n");
      return (-4);
    } else {
      // Parent is not a directory
      fprintf(stderr, "Parent is a file\n");
      return (-3);
    }
  } else if (child != UNALLOCATED_INODE) {
    // Child exists
    fprintf(stderr, "%s already exists\n", path);
    return (-1);
  } else {
    // Parent does not exist
    fprintf(stderr, "Parent does not exist\n");
    return (-2);
  }
  // Should not get to this point
  return (-100);
}
// Project 4 Below
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

OUFILE* oufs_fopen(char *cwd, char *path, char mode) {
  INODE_REFERENCE parent;
  INODE_REFERENCE child;
  char local_name[MAX_PATH_LENGTH];
  int ret;
  if ((ret = oufs_find_file(cwd, path, &parent, &child, local_name)) < -1) {
    return NULL;
  }

  // Parent exists and child does not, create file
  if (parent != UNALLOCATED_INODE && child == UNALLOCATED_INODE) {
    // Get parent inode
    INODE parentInode;
    if (oufs_read_inode_by_reference(parent, &parentInode) != 0) {
      return NULL;
    }
    // If parent is a directory
    if (parentInode.type == IT_DIRECTORY) {
      // Get next open inode for child inode location;
      BLOCK masterBlock;
      vdisk_read_block(MASTER_BLOCK_REFERENCE, &masterBlock);
      int byte;
      int flag;
      for (byte = 0, flag = 1; flag && byte < INODES_PER_BLOCK; ++byte) {
        if (masterBlock.master.inode_allocated_flag[byte] != 0xff) {
          flag = 0;
          break;
        }
      }
      int bit =
          oufs_find_open_bit(masterBlock.master.inode_allocated_flag[byte]);
      INODE_REFERENCE childLocation = (byte << 3) + bit;
      // Create new inode with follow fields
      //  type-F
      //  All blocks are unallocated
      //  size-0
      INODE childInode;
      childInode.type = IT_FILE;
      childInode.n_references = 1;
      for (int i = 0; i < BLOCKS_PER_INODE; ++i) {
        childInode.data[i] = UNALLOCATED_BLOCK;
      }
      childInode.size = 0;
      oufs_write_inode_by_reference(childLocation, &childInode);
      // Increment master inode table
      masterBlock.master.inode_allocated_flag[byte] |= (1 << (bit));
      vdisk_write_block(MASTER_BLOCK_REFERENCE, &masterBlock);
      // Link child location inside of parentInode
      for (int i = 0; i < BLOCKS_PER_INODE; ++i) {
        if (parentInode.data[i] != UNALLOCATED_BLOCK) {
          BLOCK_REFERENCE ref = parentInode.data[i];
          BLOCK block;
          vdisk_read_block(ref, &block);
          for (int j = 0; j < DIRECTORY_ENTRIES_PER_BLOCK; ++j) {
            if (block.directory.entry[j].inode_reference == UNALLOCATED_INODE) {
              strncpy(block.directory.entry[j].name, local_name,
                      strlen(local_name));
              block.directory.entry[j].inode_reference = childLocation;
              vdisk_write_block(ref, &block);
              ++parentInode.size;
              break;
            }
          }
        }
      }
      oufs_write_inode_by_reference(parent, &parentInode);
      OUFILE *file = malloc(sizeof(OUFILE));
      file->inode_reference = childLocation;
      file->mode = mode;
      file->offset = 0;
      return file;
    }
    // Parent is not a directory, throw error
    else {
      fprintf(stderr, "error: parent is not a directory\n");
      return NULL;
    }
  }
  // Parent and child exist
  else if (parent != UNALLOCATED_INODE && child != UNALLOCATED_INODE) {
    // Check if child is a file
    INODE childInode;
    if (oufs_read_inode_by_reference(child, &childInode) != 0) {
      return NULL;
    }
    // If child is file, do nothing
    if (childInode.type == IT_FILE) {
      //TODO: return the oufile
      OUFILE *file = malloc(sizeof(OUFILE));
      file->inode_reference = child;
      file->mode = mode;
      file->offset = childInode.size;
      return file;
    }
    // If child is directory, throw error
    else {
      fprintf(stderr, "ztouch error: child exists and is not a file\n");
      return NULL;
    }

  }
  // Parent does not exist, throw error
  else {
    fprintf(stderr, "ztouch error: parent does not exist\n");
    return NULL;
  }
  return NULL;
}

int oufs_fwrite(OUFILE *fp, unsigned char* buf, int len){
  INODE_REFERENCE file_inode_reference = fp->inode_reference;
  INODE file_inode;
  oufs_read_inode_by_reference(file_inode_reference, &file_inode);
  char mode = fp->mode;

  //If the file is from 'zcreate', 0 out the file and write from beginning, otherwise
  //skip this step
  if(mode == 'w'){
    //Zero out all data blocks
    BLOCK master;
    vdisk_read_block(MASTER_BLOCK_REFERENCE, &master);
    for(int i = 0; i < BLOCKS_PER_INODE; ++i){
      if(file_inode.data[i] != UNALLOCATED_BLOCK){
        BLOCK b;
        vdisk_read_block(file_inode.data[i], &b);
        for(int j = 0; j < BLOCK_SIZE; ++j){
          b.data.data[j] = 0;
        }
        master.master.block_allocated_flag[file_inode.data[i] / 8] &= ~(1 << (file_inode.data[i] % 8)); //Mark the data blocks as unallocated in the master allocation table
        file_inode.data[i] = UNALLOCATED_BLOCK;
      }
    }
    file_inode.size = 0;
    fp->offset = 0;
    oufs_write_inode_by_reference(file_inode_reference, &file_inode); //Write changes back to file
    vdisk_write_block(MASTER_BLOCK_REFERENCE, &master); //^^
  }

  //To know which data block to insert into, do offset / BLOCK_SIZE
  //To know where in the data block to insert into, do offset % BLOCK_SIZE
  int offset = file_inode.size;
  int location = offset;
  int data_block_index = offset / BLOCK_SIZE;
  BLOCK data_block;
  if(offset != 0) //If there is data present, load the data
    vdisk_read_block(file_inode.data[data_block_index], &data_block);

  //Gets what will be the total size of the file after writing
  int total_size = file_inode.size + len;
  //If the size of the file is too big, shrink to max size available
  if(total_size > (BLOCKS_PER_INODE * BLOCK_SIZE))
    total_size = BLOCKS_PER_INODE * BLOCK_SIZE;

  //Gets total number of blocks after adding buf 
  int num_data_blocks;
  if(total_size % BLOCK_SIZE == 0)
    num_data_blocks = total_size / BLOCK_SIZE;
  else
    num_data_blocks = total_size / BLOCK_SIZE + 1;

  //Must always start at 0 for buf 
  int buf_index = 0;
  for(int i = file_inode.size; i < num_data_blocks * BLOCK_SIZE; ++i){
    if(offset % BLOCK_SIZE == 0){ //At the end of a block, need to allocate new block
      if(offset != 0)
        vdisk_write_block(file_inode.data[data_block_index], &data_block); //Writes the full data block back to the disk

      data_block_index = offset / BLOCK_SIZE; //Gets the location in the inode where this block will be placed
      BLOCK_REFERENCE bf = oufs_allocate_new_block();
      file_inode.data[data_block_index] = bf; //Places the new data block in the inode
    }
    //If more chars need to be written, write them to the correct place
    if(i < total_size){
      data_block.data.data[offset % BLOCK_SIZE] = buf[buf_index];
      ++buf_index;
      ++offset;
      ++location;
    }
    //All of chars have been written, now write 0s
    else {
      data_block.data.data[location % BLOCK_SIZE] = 0;
      ++location;
    }
  }
  //Write the changes back to the file
  vdisk_write_block(file_inode.data[data_block_index], &data_block);
  file_inode.size = total_size;
  oufs_write_inode_by_reference(file_inode_reference, &file_inode);
  vdisk_write_block(file_inode.data[data_block_index], &data_block); 
}

int oufs_fread(OUFILE *fp, unsigned char* buf, int len){
  INODE_REFERENCE file_inode_reference = fp->inode_reference;
  INODE file_inode;
  oufs_read_inode_by_reference(file_inode_reference, &file_inode);

  int num_data_blocks;
  if(file_inode.size % BLOCK_SIZE == 0)
    num_data_blocks = file_inode.size / BLOCK_SIZE;
  else
    num_data_blocks = file_inode.size / BLOCK_SIZE + 1;

  for(int i = 0; i < num_data_blocks; ++i){
    BLOCK b;
    vdisk_read_block(file_inode.data[i], &b);
    if(i == num_data_blocks - 1){
      for(int j = 0; j < file_inode.size % BLOCK_SIZE; ++j){
        printf("%c", b.data.data[j]);
        fflush(stdout);
      }
    } 
    else{
      for(int j = 0; j < BLOCK_SIZE; ++j){
        printf("%c", b.data.data[j]);
        fflush(stdout);
      }
    }
  }
  return 0;
}

int oufs_remove(char *cwd, char* path){
  INODE_REFERENCE parent_ref;
  INODE_REFERENCE child_ref;
  char local_name[MAX_PATH_LENGTH];
  char ret;
  if((ret = oufs_find_file(cwd, path, &parent_ref, &child_ref, local_name)) < -1){
    return -1;
  }

  if(child_ref != UNALLOCATED_INODE){
    //Remove entry from parent's data block
    INODE parent_inode;
    oufs_read_inode_by_reference(parent_ref, &parent_inode);

    //Step through the parents directory block to remove the entry
    for(int i = 0; i < BLOCKS_PER_INODE; ++i){
      if(parent_inode.data[i] != UNALLOCATED_BLOCK){
        BLOCK b;
        vdisk_read_block(parent_inode.data[i], &b);
        for(int j = 0; j < DIRECTORY_ENTRIES_PER_BLOCK; ++j){ //Load individual directory blocks
          if(!strcmp(b.directory.entry[j].name, local_name)){ //If entry matches the target
            strncpy(b.directory.entry[j].name, "", 1); //Set name to empty
            b.directory.entry[j].inode_reference = UNALLOCATED_INODE; //Mark inode as unallocated in parent
            vdisk_write_block(parent_inode.data[i], &b); //Write changes back to disk
            --parent_inode.size; //Decrement the parent's size
            oufs_write_inode_by_reference(parent_ref, &parent_inode); //Write changes back to disk
          }
        }
      }
    }
    
    //Decrement n_references in inode
    INODE child_inode;
    oufs_read_inode_by_reference(child_ref, &child_inode);
    --child_inode.n_references;
    oufs_write_inode_by_reference(child_ref, &child_inode);

    //If n_references is now 0, the inode and all associated data blocks are deallocated
    if(child_inode.n_references == 0){
      BLOCK master;
      vdisk_read_block(MASTER_BLOCK_REFERENCE, &master);
      //Step through all the data blocks and emtpy
      for(int i = 0; i < BLOCKS_PER_INODE; ++i){
        if(child_inode.data[i] != UNALLOCATED_BLOCK){
          BLOCK b;
          vdisk_read_block(child_inode.data[i], &b);
          for(int j = 0; j < BLOCK_SIZE; ++j){ //Empty entire block
            b.data.data[j] = 0; 
          }
          vdisk_write_block(child_inode.data[i], &b); //Write emtpy block back to disk
          master.master.block_allocated_flag[child_inode.data[i] / 8] &= ~(1 << (child_inode.data[i] % 8)); //Mark block as unallocated in master
        }
      }
      //Deallocate inode
      child_inode.type = IT_NONE;
      child_inode.size = 0;
      oufs_write_inode_by_reference(child_ref, &child_inode); //Write emtpy inode back to disk
      master.master.inode_allocated_flag[child_ref / 8] &= ~(1 << (child_ref % 8)); //Mark inode as unallocated in master block
      vdisk_write_block(MASTER_BLOCK_REFERENCE, &master); //Write master block back to disk
    }
  }
}

int oufs_link(char* cwd, char *path_src, char* path_dst){
  //Checking if source exists
  INODE_REFERENCE src_parent_ref;
  INODE_REFERENCE src_child_ref;
  char src_local_name[MAX_PATH_LENGTH];
  int src_ret;
  if((src_ret = oufs_find_file(cwd, path_src, &src_parent_ref, &src_child_ref, src_local_name)) < -1){
    return -1;
  }

  INODE src_child_inode;
  if(src_child_ref != UNALLOCATED_INODE){
    oufs_read_inode_by_reference(src_child_ref, &src_child_inode);
  }
  
  //If the source does exist
  if(src_child_inode.type == IT_FILE){
    //Opens source OUFILE 
    OUFILE* src_file;
    src_file = oufs_fopen(cwd, path_src, 'r');
    INODE_REFERENCE src_file_inode_ref = src_file->inode_reference;
    INODE src_file_inode;
    oufs_read_inode_by_reference(src_file_inode_ref, &src_file_inode);

    //Gets destination file information
    INODE_REFERENCE dst_parent_ref;
    INODE_REFERENCE dst_child_ref;
    char local_name[MAX_PATH_LENGTH];
    int ret;
    if((ret = oufs_find_file(cwd, path_dst, &dst_parent_ref, &dst_child_ref, local_name)) < -1){
      return -1;
    }

    //Links the destination to the source's inode
    if(dst_parent_ref != UNALLOCATED_INODE && dst_child_ref == UNALLOCATED_INODE){
      INODE dst_parent_inode;
      if(oufs_read_inode_by_reference(dst_parent_ref, &dst_parent_inode))
        return -1;
      if(dst_parent_inode.type == IT_DIRECTORY){
        for(int i = 0; i < BLOCKS_PER_INODE; ++i){
          if(dst_parent_inode.data[i] != UNALLOCATED_BLOCK){
            BLOCK_REFERENCE ref = dst_parent_inode.data[i];
            BLOCK block;
            vdisk_read_block(ref, &block);
            for(int j = 0; j < DIRECTORY_ENTRIES_PER_BLOCK; ++j){
              if(block.directory.entry[j].inode_reference == UNALLOCATED_INODE){
                strncpy(block.directory.entry[j].name, local_name, strlen(local_name));
                block.directory.entry[j].inode_reference = src_file->inode_reference;
                vdisk_write_block(ref, &block);
                ++dst_parent_inode.size;
                oufs_write_inode_by_reference(dst_parent_ref, &dst_parent_inode);
                ++src_file_inode.n_references;
                oufs_write_inode_by_reference(src_file_inode_ref, &src_file_inode);
                return 0;
              }
            }
          }
        }
      }
    }
  }
  //Reaches only if the arguments are incorrect
  fprintf(stderr, "Invalid zlink arguments\n");
  fprintf(stderr, "Usage: ./zlink <existing> <new_name>\n");
  fprintf(stderr, "\t 'existing' must exist and be a file\n");
  fprintf(stderr, "\t 'new_name' must not exist and parent must be an existing directory\n");
  return -1;
}
