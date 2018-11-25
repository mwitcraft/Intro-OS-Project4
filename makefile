all: format filez inspect mkdir rmdir touch append more create link remove
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
create:
	gcc zcreate.c oufs_lib_support.c vdisk.c -o zcreate 
remove:
	gcc zremove.c oufs_lib_support.c vdisk.c -o zremove
more:
	gcc zmore.c oufs_lib_support.c vdisk.c -o zmore
link:
	gcc zlink.c oufs_lib_support.c vdisk.c -o zlink
clean:
	rm zformat zfilez zinspect zmkdir zrmdir ztouch zappend zcreate zmore zlink
