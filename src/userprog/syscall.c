#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "devices/shutdown.h"
#include "filesys/filesys.h"
#include "devices/input.h"
#include "vm/page.h"

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

static int mmap (int fd, void *addr);

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

  thread_current()->esp = f->esp;

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
      case SYS_MMAP:
        {
          void *args[2];
          get_arguments (f->esp + 4, args, 2);
          f->eax = mmap (*(int *) (args[0]), *(void **) (args[1]));
          break;
        }
      case SYS_MUNMAP:
        {
          void *args[1];
          get_arguments (f->esp + 4, args, 1);
          munmap (*(int *) (args[0]));
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
  if (uaddr == NULL || !is_user_vaddr (uaddr))
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

  printf ("%s: exit(%d)\n", thread_name (), status);

  thread_exit();
}

static int
exec (const char *cmd_line)
{
  lock_acquire (&filesys_lock);
  int pid = process_execute(cmd_line);
  lock_release (&filesys_lock);
  return pid;
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
  struct file *file = get_file (fd);
  if (file == NULL)
    return -1;
  lock_acquire (&filesys_lock);
  int size = file_length (file);
  lock_release (&filesys_lock);
  return size;
}

static int
read (int fd, void *buffer, unsigned size)
{
  unsigned i;
  for (i = 0; i < size; i++)
    check_user_address_valid (buffer + i);

  if (fd == 0)
    {
      for (i = 0; i < size; i++)
        *(uint8_t *) (buffer + i) = input_getc ();
      return size;
    }
  else
    {
      lock_acquire (&filesys_lock);
      struct file *file = get_file (fd);
      if (file == NULL)
        {
          lock_release (&filesys_lock);
          return -1;
        }
      int bytes_read = file_read (file, buffer, size);
      lock_release (&filesys_lock);
      return bytes_read;
    }
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
      lock_acquire (&filesys_lock);
      struct file *file = get_file (fd);
      if (file == NULL)
        {
          lock_release (&filesys_lock);
          return -1;
        }
      int bytes_written = file_write (file, buffer, size);
      lock_release (&filesys_lock);
      return bytes_written;
    }
}

static void
seek (int fd, unsigned position)
{
  lock_acquire (&filesys_lock);
  struct file *file = get_file (fd);
  if (file == NULL)
    {
      lock_release (&filesys_lock);
      exit (-1);
    }
  file_seek (file, position);
  lock_release (&filesys_lock);
}

static unsigned
tell (int fd)
{
  lock_acquire (&filesys_lock);
  struct file *file = get_file (fd);
  if (file == NULL)
    {
      lock_release (&filesys_lock);
      exit (-1);
    }
  int position = file_tell (file);
  lock_release (&filesys_lock);
  return position;
}

static void
close (int fd)
{
  lock_acquire (&filesys_lock);
  remove_file (fd);
  lock_release (&filesys_lock);
}

static int
mmap (int fd, void *addr)
{
  if (fd == 0 || fd == 1)
    return -1;
  if (addr == NULL || pg_ofs (addr) != 0)
    return -1;

  lock_acquire (&filesys_lock);
  struct file *found_file = get_file (fd);
  if (found_file == NULL)
    {
      lock_release (&filesys_lock);
      return -1;
    }

  struct file *new_file = file_reopen (found_file);
  if (new_file == NULL)
    {
      lock_release (&filesys_lock);
      return -1;
    }

  int file_size = file_length (new_file);
  if (file_size == 0)
    {
      lock_release (&filesys_lock);
      return -1;
    }

  struct thread *t = thread_current ();
  int i;
  for (i = 0; i < file_size; i += PGSIZE)
    {
      if (has_entry_in_spt (t->spt, addr + i))
        {
          lock_release (&filesys_lock);
          return -1;
        }
    }

  for (i = 0; i < file_size; i += PGSIZE)
    {
      size_t page_read_bytes = file_size - i < PGSIZE ? file_size - i : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      install_filesys_entry_in_spt (t->spt, addr + i, new_file, i, page_read_bytes, page_zero_bytes, true);
    }

  struct mmap_descriptor *mmap_desc = malloc (sizeof (struct mmap_descriptor));
  if (list_empty (&t->mmap_list))
    mmap_desc->id = 1;
  else
    {
      struct mmap_descriptor *last_desc =
              list_entry (list_back (&t->mmap_list), struct mmap_descriptor, elem);
      mmap_desc->id = last_desc->id + 1;
    }
  mmap_desc->file = new_file;
  mmap_desc->size = file_size;
  mmap_desc->addr = addr;
  list_push_back (&t->mmap_list, &mmap_desc->elem);

  lock_release (&filesys_lock);
  return mmap_desc->id;
}

static struct mmap_descriptor *
get_mmap_descriptor(int mapid)
{
  struct thread *t = thread_current ();

  struct list_elem *e;
  for (e = list_begin (&t->mmap_list); e != list_end (&t->mmap_list);
       e = list_next (e))
    {
      struct mmap_descriptor *desc = list_entry (e, struct mmap_descriptor, elem);
      if (desc->id == mapid)
        return desc;
    }
  return NULL;
}


void
munmap (int mapid)
{
  struct mmap_descriptor *mmap_desc = get_mmap_descriptor (mapid);
  if (mmap_desc == NULL)
    return;

  struct thread *t = thread_current ();
  lock_acquire (&filesys_lock);
  int i;
  for (i = 0; i < mmap_desc->size; i += PGSIZE)
    {
      int size = mmap_desc->size - i < PGSIZE ? mmap_desc->size - i : PGSIZE;
      spt_unmap (t->spt, mmap_desc->addr + i, t->pagedir, i, size);
    }
  list_remove (&mmap_desc->elem);
  file_close (mmap_desc->file);
  free (mmap_desc);
  lock_release (&filesys_lock);
}
