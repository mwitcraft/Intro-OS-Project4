#include <stdio.h>
#include <string.h>

#include "oufs.h"
#include "oufs_lib.h"

int main(int argc, char** argv){
  //Get working directory for the OUFS
  char cwd[MAX_PATH_LENGTH];
  char diskName[MAX_PATH_LENGTH];
  oufs_get_environment(cwd, diskName);

  //Opens the disk for reading
  vdisk_disk_open(diskName);

  //If an argument is provided, list the directories in there
  if(argc == 2)
    oufs_list(cwd, argv[1]);
  //If no argument is provided, list the directories in the cwd
  else if(argc == 1)
    oufs_list(cwd, "");
  //If more than 1 argument is provided, throw an error
  else
    fprintf(stderr, "ERROR: zfilez only accepts one argument\n");

  //Closes the disk after all work is done
  vdisk_disk_close();


  return 0;
}
