#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "devices/shutdown.h"
#include "filesys/filesys.h"

static void syscall_handler (struct intr_frame *);

static void check_user_address_valid (void *);
static void get_arguments (void *sp, void **args, uint8_t num);

static void halt (void);
static int exec (const char *cmd_line);
static int wait (int pid);
static bool create (const char *file, unsigned initial_size);
static bool remove (const char *file);
static int open (const char *file);
static int filesize (int fd);
static int read (int fd, void *buffer, unsigned size);
static int write (int fd, const void *buffer, unsigned size);
static void seek (int fd, unsigned position);
static unsigned tell (int fd);
static void close (int fd);

static struct lock filesys_lock;

void
syscall_init (void) 
{
  lock_init (&filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int i;
  for (i = 0; i < 4; i++)
    {
      check_user_address_valid (f->esp + i);
    }
  int syscall_number = *(int *) (f->esp);


  switch (syscall_number)
    {
      case SYS_HALT:
        {
          halt ();
          break;
        }
      case SYS_EXIT:
        {
          void *args[1];
          get_arguments (f->esp + 4, args, 1);
          exit (*(int *) (args[0]));
          break;
        }
      case SYS_EXEC:
        {
          void *args[1];
          get_arguments (f->esp + 4, args, 1);
          check_user_address_valid((void *)args[0]);
          f->eax = exec (*(const char **) (args[0]));
          break;
        }
      case SYS_WAIT:
        {
          void *args[1];
          get_arguments (f->esp + 4, args, 1);
          f->eax = wait (*(int *) (args[0]));
          break;
        }
      case SYS_CREATE:
        {
          void *args[2];
          get_arguments (f->esp + 4, args, 2);
          f->eax = create (*(const char **) (args[0]), *(unsigned *) (args[1]));
          break;
        }
      case SYS_REMOVE:
        {
          void *args[1];
          get_arguments (f->esp + 4, args, 1);
          f->eax = remove (*(const char **) (args[0]));
          break;
        }
      case SYS_OPEN:
        {
          void *args[1];
          get_arguments (f->esp + 4, args, 1);
          f->eax = open (*(const char **) (args[0]));
          break;
        }
      case SYS_FILESIZE:
        {
          void *args[1];
          get_arguments (f->esp + 4, args, 1);
          f->eax = filesize (*(int *) (args[0]));
          break;
        }
      case SYS_READ:
        {
          void *args[3];
          get_arguments (f->esp + 4, args, 3);
          f->eax = read (*(int *) (args[0]), *(void **) (args[1]), *(unsigned *) (args[2]));
          break;
        }
      case SYS_WRITE:
        {
          void *args[3];
          get_arguments (f->esp + 4, args, 3);
          f->eax = write (*(int *) (args[0]), *(const void **) (args[1]), *(unsigned *) (args[2]));
          break;
        }
      case SYS_SEEK:
        {
          void *args[2];
          get_arguments (f->esp + 4, args, 2);
          seek (*(int *) (args[0]), *(unsigned *) (args[1]));
          break;
        }
      case SYS_TELL:
        {
          void *args[1];
          get_arguments (f->esp + 4, args, 1);
          f->eax = tell (*(int *) (args[0]));
          break;
        }
      case SYS_CLOSE:
        {
          void *args[1];
          get_arguments (f->esp + 4, args, 1);
          close (*(int *) (args[0]));
          break;
        }
      default:
        {
          exit (-1);
          break;
        }
    }
}

static void
check_user_address_valid (void *uaddr)
{
  if (!is_user_vaddr (uaddr))
    exit (-1);

  if (pagedir_get_page (thread_current ()->pagedir, uaddr) == NULL)
    exit (-1);
}

static void
get_arguments (void *sp, void **args, uint8_t num)
{
  int i, j;
  for (i = 0; i < num; i++)
    {
      for (j = 0; j < 4; j++)
        {
          check_user_address_valid (sp + 4 * i + j);
        }
      args[i] = sp + 4 * i;
    }
}

static void
halt (void)
{
  shutdown_power_off ();
}

void
exit (int status)
{
  struct thread *child = thread_current();
  child->exit_status = status;
  child->parent->exit_status = status;

  printf ("%s: exit(%d)\n", thread_name (), status);

  thread_exit();
}

static int
exec (const char *cmd_line)
{
  return process_execute(cmd_line);
}

static int
wait (int pid)
{
  return process_wait(pid);
}

static bool
create (const char *file, unsigned initial_size)
{
  check_user_address_valid ((void *) file);

  lock_acquire (&filesys_lock);
  bool success = filesys_create (file, initial_size);
  lock_release (&filesys_lock);

  return success;
}

static bool
remove (const char *file)
{
  check_user_address_valid ((void *) file);

  lock_acquire (&filesys_lock);
  bool success = filesys_remove (file);
  lock_release (&filesys_lock);

  return success;
}

static int
open (const char *file)
{
  check_user_address_valid ((void *) file);
  int fd = -1;

  lock_acquire (&filesys_lock);
  struct file* opened_file = filesys_open (file);
  if (opened_file != NULL)
    {
      fd = create_file_descriptor (opened_file);
    }
  lock_release (&filesys_lock);
  return fd;
}

static int
filesize (int fd)
{
  printf ("filesize system call!\n");
  thread_exit ();
}

static int
read (int fd, void *buffer, unsigned size)
{
  printf ("read system call!\n");
  thread_exit ();
}

static int
write (int fd, const void *buffer, unsigned size)
{
  unsigned i;
  for (i = 0; i < size; i++)
    check_user_address_valid ((void *) buffer + i);

  if (fd == 1)
    {
      putbuf (buffer, size);
      return size;
    }
  else
    {
      /* TODO: should implement file write */
      thread_exit ();
    }
}

static void
seek (int fd, unsigned position)
{
  printf ("seek system call!\n");
  thread_exit ();
}

static unsigned
tell (int fd)
{
  printf ("tell system call!\n");
  thread_exit ();
}

static void
close (int fd)
{
  printf ("close system call!\n");
  thread_exit ();
}
