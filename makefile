all: format filez inspect mkdir rmdir touch append more
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
append:
	gcc zappend.c oufs_lib_support.c vdisk.c -o zappend 
more:
	gcc zmore.c oufs_lib_support.c vdisk.c -o zmore
clean:
	rm zformat zfilez zinspect zmkdir zrmdir
