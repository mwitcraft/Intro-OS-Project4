#include <stdio.h>
#include <string.h>

#include "oufs_lib.h"

int main(int argc, char** argv) {
  // Fetch the key environment vars
  char cwd[MAX_PATH_LENGTH];
  char disk_name[MAX_PATH_LENGTH];
  oufs_get_environment(cwd, disk_name);

  // Check arguments
  if(argc == 3) {
    // Open the virtual disk
    vdisk_disk_open(disk_name);

    //Gets the file for writing
    oufs_link(cwd, argv[1], argv[2]);


    // Clean up
    vdisk_disk_close();

  }else{
    // Wrong number of parameters
    fprintf(stderr, "Usage: ztouch <dirname>\n");
  }

}
