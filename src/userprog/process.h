#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_execute (const char *task_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

int create_file_descriptor (struct file *file);
struct file * get_file (int fd);
void remove_file (int fd);

#endif /* userprog/process.h */

struct thread * get_child_process(int pid);
void remove_child_process(struct thread *cp);
//static bool install_page (void *upage, void *kpage, bool writable);