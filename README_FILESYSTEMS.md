# Filesystems
***
## Second IDE Controller

Enabling the use of a second IDE controller to support 2 more disks.

IDE driver was extended to support the other 2 disks. 
Writes to the controller ports were determined by which device was currently being
used. 

### Second IDE Controller Tests:

 - ```./usertests``` 
	 - [x] passed

***
## Disk Device Nodes 

The four disk devices were made to be accessible by user programs. 

This was done by turning them into "files".
Read and Write functions were implemented that enabled the 
disks to be read or written using the bread/bwrite functions.

### Disk Device Nodes Tests:

 - ```./test_disk``` 
 	 - Writes and reads from disk 2 and 3.

 - ```./df``` 
	 - Displays the number of free blocks and the 
       number of free inodes on disk 1.

 - ```./usertests```
	 - [x] passed

***
## "Native" mkfs

The mkfs program was copied and re-made into a user usable program
named umfks. The program accepts a disk device node name,
opens the device and creates an empty filesystem in it with 
entries '.' and '..'

### "Native" mkfs Tests:

 - ```./usertests```
	 - [x] passed 

***
## Mount and Unmount

Mounting and Unmounting functionallity was added.
A **mount table** was created that holds the inodes of 
mount points and the major and minor device numbers.
During pathname resolution, the table is searched to test if 
mounting point is found. If it is, then the root inode for the 
mounted device is followed. Unmounting removes the mount point 
from the mount table.

Issues:
	The first issue resolved was the ability to return the the parent
	from inside the mount point after mounting a device onto it. 
	In other words, handling the use of ".." within the mount point directory.
	This was done by testing for the ".." being search while the current inode is a 
	device root inode. If so, the inode is replaced by the inode of the mountpoint.

### Mount and Unmount Tests:

 1. Initialze disk2 with a filesystem
	```umkfs disk2``` (**might take a few seconds**)
 2. Create a directory to serve as the **mount point**
	```mkdir /mnt```
 3. Create another directory in /mnt and a file 
    ```mkdir /mnt/testdir```
	```cd /mnt```
	```../cat > file.txt```
	```Hello!```
	(ctrl-d)
	```cd ..```
 4. Mount:
	```mount /disk2 /mnt```
 5. Check that the /mnt dir no longer has testdir and file.txt
	```ls /mnt```
 6. Create a directory and file in /mnt
	```mkdir /mnt/testdir2```
	```cd /mnt```
	```../cat > file2.txt```
	```Hello2!```
	(ctrl-d)
	```cd ..```
 7. Unmount /mnt, test and see if testdir and file.txt are present and testdir2 and file2.txt are not
	```unmount /mnt```
	```ls /mnt```
 8. Remount /disk2 on /mnt and check if testdir2 and file2.txt are present
	```mount /disk2 /mnt```
	```ls /mnt```

	- disk3 was tested the same way

	- Create two mount points and mount disk2 and disk3 to the first and second respectively, and 
	perform the same test ^

	- User cannot mount multiple devices to one mount point
	Test: 
		```mount disk2 mnt```
		```mount disk3 mnt```
	- User cannot mount one device on multiple targets
	Test:
		``` mount disk2 mnt```
		``` mount disk2 mnt2```
	- User cannot unmount a non existing mount point

	- ```./usertests``` - Allowed for umkfs and mkfs to use more blocks (increased FSSIZE) so that bigfile test can succeed
						- made size 10000 (bigfile tests takes a minute or two)
	    - [x] passed 		- Reverted back to size 1000 for regular use (umkfs takes too long because of the block zeroing - removing block zeroing causes panic in ilock).

***
