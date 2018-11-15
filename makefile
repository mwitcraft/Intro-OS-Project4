all: format filez inspect mkdir rmdir touch
format:
	gcc zformat.c oufs_lib_support.c vdisk.c -o zformat
filez:
	gcc zfilez.c oufs_lib_support.c vdisk.c -o zfilez
inspect:
	gcc zinspect.c oufs_lib_support.c vdisk.c -o zinspect
mkdir:
	gcc zmkdir.c oufs_lib_support.c vdisk.c -o zmkdir 
rmdir:
	gcc zrmdir.c oufs_lib_support.c vdisk.c -o zrmdir 
touch:
	gcc ztouch.c oufs_lib_support.c vdisk.c -o ztouch 
clean:
	rm zformat zfilez zinspect zmkdir zrmdir
