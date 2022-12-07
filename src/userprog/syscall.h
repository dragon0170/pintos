#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);

void exit (int status);
void munmap (int mapid);

#endif /* userprog/syscall.h */
