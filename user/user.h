#ifndef USER_H
#define USER_H

struct stat;
struct rtcdate;
struct file;

// system calls
int fork(void);
//int exit(void) __attribute__((noreturn));
int exit(int);
int wait(int*);
int pipe(int*);
int write(int, void*, int);
int read(int, void*, int);
int close(int);
int kill(int);
int exec(char*, char**);
int open(char*, int);
int mknod(char*, short, short);
int unlink(char*);
int fstat(int, struct stat*);
int link(char*, char*);
int mkdir(char*);
int chdir(char*);
int dup(int);
int getpid(void);
char* sbrk(int);
int sleep(int);
int uptime(void);
int getdate(struct rtcdate *);
int setdate(struct rtcdate *);
int lseek(int, int, int);
int mount(char *, char *);
int unmount(char *);

// ulib.c
int stat(char*, struct stat*);
char* strcpy(char*, char*);
void *memmove(void*, void*, int);
char* strchr(const char*, char);
int strcmp(const char*, const char*);
void printf(int, char*, ...);
char* gets(char*, int);
uint strlen(char*);
void* memset(void*, int, uint);
void* malloc(uint);
void free(void*);
int atoi(const char*);

#endif
