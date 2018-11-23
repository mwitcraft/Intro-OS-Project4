#include <stdio.h>
#include <string.h>

#include "oufs_lib.h"

int main(int argc, char** argv) {
  // Fetch the key environment vars
  char cwd[MAX_PATH_LENGTH];
  char disk_name[MAX_PATH_LENGTH];
  oufs_get_environment(cwd, disk_name);

  // Check arguments
  if(argc == 2) {
    // Open the virtual disk
    vdisk_disk_open(disk_name);

    //Gets the file for writing
    OUFILE* oufile = malloc(sizeof(*oufile));
    oufile = oufs_fopen(cwd, argv[1], 'a');
    //Steps through stdin and stores in buffer
    char c = fgetc(stdin);
    int length = 0;
    unsigned char* buf;
    while(c != EOF){
      // printf("%c", c);
      buf[length] = c;
      c = fgetc(stdin);
      ++length;
    }
    // printf("Length: %i\n", length);
    // printf("Length of Buf: %i\n", strlen(buf));
    // for(int i = 0; i < strlen(buf); ++i){
      // printf("%c", buf[i]);
    // }

    //Writes Buffer to file
    oufs_fwrite(oufile, buf, length);
    free(oufile);

    // Clean up
    vdisk_disk_close();

  }else{
    // Wrong number of parameters
    fprintf(stderr, "Usage: ztouch <dirname>\n");
  }

}
