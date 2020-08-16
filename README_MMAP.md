# Virtual Memory
***
## MMAP() SYSTEM CALL

This system call allows for memory to be "mapped" in a processes 
user address space. This can be a private block of memory or a per-process shared
block that is backed by a file.

When the user mmap's a file, the file is retrived from disk only when it is
accessed (read/write). It is initially unallocated. 

This allows for less number of I/O needed to load files into memory. 
The system call also allows for faster file reads/writes.

### MMAP() SYSTEM CALL Tests:

 - ```./usertests``` 
	 - [x] passed

 - ```./mapping_anon_test```
	- Tests an anonymous block of memory by writing to it and reading what was written.
	The test also munmap's the block and attempts to access the previously mapped region of 
	memory. 

 - ```./mapping_file_test```
	- Tests the mapping of a file. 
		- Opens the README file with read only permission.
		- Reads it.
		- Write to it.
		- Closes File.
		- Reads from it. (Allowed, even though file was closed)
		- Opens README file with read and write permissions.
		- Writes to it.
		- Read it. 


***
