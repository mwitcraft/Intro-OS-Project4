Project 3
Mason Witcraft - mason.witcraft@ou.edu
11/08/2018

My implementation of a virtual file system.
In this README, I will only outline the changes I made to the code that was provided on the Project 3 webpage.

Programs Created:
-zformat:
    -Creates a virtual disk with size provided
    -Sets all bit in disk to 0
    -Creates a master block, which contains 2 tables:
        -block_allocated_flag
        -inode_allocated_flag
		- Both keep track of the blocks that are currently allocated - continuously updated through the life of the disk
    -Initializes the first inode and points it to the root directory
    -Initializes the root directory
-zfilez:
    -Lists directories contained inside specific directory
    -Steps through a given inode and lists all of the entries belonging to that inode, in alphabetical order
-zmkdir:
    -Creates a new directory inside the virtual file system
    -Does this by creating a new inode and pointing it at a new data block
    -New directory is created in first available space
        -Tricky with adding and removing directories
    -Location of new directory inode and data block on disk is marked as allocated in the master block's allocation tables
    -Must also manipulate the parent directory - add a reference to the new directory inside of the parent directory so a heirarchy can be created
-zrmdir:
    -Removes a directory from the virtual file system
    -Must locate inode and related data block and 0 out both
    -Location of inode and related data block on the disk is marked as unallocated in the master block's allocation tables
    -Must also manipulate the parent directory so the parent no longer references a directory that is removed

Current Bugs
    -None that I know of
    
Improvements
    -I could have done a better job of fully understanding code that was provided, as I ended up writing a few functions on my own that were provided (mine still work though, just took time)
    -I also could have done better at making functions for tasks that were performed often
    
Sources:
-https://oudalab.github.io/cs3113fa18/projects/project3
    -Project website, used to get alot of information pertaining to this project, beyond just the specifications
-https://stackoverflow.com/questions/43099269/qsort-function-in-c-used-to-compare-an-array-of-strings
    -Used to learn how to use qsort to sort an array of strings
-https://stackoverflow.com/questions/6848617/memory-efficient-flag-array-in-c
    -Used to learn how to set individual bits in an unsigned char to set flags

Other Remarks:
The vast majority of learning how to do this project and understanding the structure of everything came from studying the given code. So, while I don't have many sources, that is because I used what was given to help me almost every step of the way.
