Project 4
Mason Witcraft - mason.witcraft@ou.edu
11/29/2018

My implementation of a virtual file system, complete with working directories and files.
In this README, I will only outline the changes I made to the code that was provided on the Project 3 webpage, the Project 4 webpage and the code provided through links on those pages.

Programs Created:
Project 3
---------------------------------------------------------
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
Project 4
----------------------------------------------------------
-Contains all of the above programs from Project 3, as well as:
-ztouch:
    -Creates a new empty file
    -If the file to be created does not already exist and the parent specified does:
        -A entry for the file is placed in the parent directory
        -A new file is created with size 0
            -A new inode of type file is created and marked as allocated in the master block's inode allocation table
            -No new data blocks are created, therefore the master block's block allocation table is untouched
    -If the file does exist, nothing is changed.
-zcreate:
    -Creates a new file that can be initialized with data
    -If the file does not exist:
        -ztouch is called following the exact same steps outlined.
        -The file is opened
        -Input from 'stdin' is written to the file, allocating new data blocks if necessary
    -If the file does exist:
        -The file is opened, all data blocks inside the file's inode are erased, erasing all of the file's data
        -Those data blocks are marked as unallocated in the master block's block allocation table
        -The file size and offset is set to 0
        -Input from 'stdin' is written to the file, allocating new data blocks if necessary
-zappend:
    -Appends new data to an already existing file, or creates a new file that can be initialized with data
    -If the file does not exist:
        -ztouch is called following the exact same steps outlined.
        -The file is opened
        -Input from 'stdin' is written to the file, allocating new data blocks if necessary
    -If the file does exist:
        -The file is opened
        -Input from 'stdin' is appended to the end of the file, starting from the file's offset, creating new data blocks if necessary
-zmore:
    -Prints the contents of a file
    -If the file specified exists:
        -The contents of that file are printed to 'stdout'
            -Accomplished by going to the data blocks contained in the file's inode and printing each character contained in the block
-zremove:
    -Removes a file
    -If the specified file exists:
        -The data blocks contained in the file's inode are emptied and set to 0
        -Those data blocks are marked as unallocated in the master block's block allocation table
        -The inode is then erased, setting all fields to 0 and marking as unallocated in the master block's inode allocation table
-zlink:
    -Links the data of one file to another, thus 2 files access the same data
    -If the specified file to be linked exists:
        -A new file is created using ztouch
        -That new file's data blocks are pointed to the same data blocks that are contained in the linking file
        -Thus 2 inodes point to the exact same set of data blocks

Current Bugs
    -None that I know of
    
Improvements
    -Should have rewritten my Project 3 code to use the functions given on the Project 4 webpage. Instead my program contains a few functions that perform basically the same thing.
    -I also could have done better at making functions for tasks that were performed often
    
Sources:
-https://oudalab.github.io/cs3113fa18/projects/project4
    -Used to gather requirements for the project
-https://oudalab.github.io/cs3113fa18/projects/project3
    -Project website, used to get alot of information pertaining to this project, beyond just the specifications
-https://stackoverflow.com/questions/43099269/qsort-function-in-c-used-to-compare-an-array-of-strings
    -Used to learn how to use qsort to sort an array of strings
-https://stackoverflow.com/questions/6848617/memory-efficient-flag-array-in-c
    -Used to learn how to set individual bits in an unsigned char to set flags

Other Remarks:
Project 3:
    The vast majority of learning how to do this project and understanding the structure of everything came from studying the given code. So, while I don't have many sources, that is because I used what was given to help me almost every step of the way.
Project 4:
    After Project 3, I was confident in my understanding of the structure of the program, therefore implementing the new functionality for Project 4 was mainly accomplished on my own, with few sources used.
