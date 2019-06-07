#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"  //->file_sema
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "filesys/off_t.h"  /* new */
#include "filesys/inode.h"
#include "filesys/directory.h"

static void syscall_handler (struct intr_frame *);
void userp_exit (int status);

struct file
  {
    struct inode *inode;        /* File's inode. */
    off_t pos;                  /* Current position. */
    bool deny_write;            /* Has file_deny_write() been called? */
  };
  // ctrl c+v from filesys/file.c


/* Reads a byte at user virtual address UADDR.
   UADDR must be below PHYS_BASE.
   Returns the byte value if successful, -1 if a segfault
   occurred. */
/* Check null, unmapped */
static int
get_user (const uint8_t *uaddr)
{
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
       : "=&a" (result) : "m" (*uaddr));
  return result;
}


/* Writes BYTE to user address UDST.
   UDST must be below PHYS_BASE.
   Returns true if successful, false if a segfault occurred. */
static bool
put_user (uint8_t *udst, uint8_t byte)
{
  int error_code;
  asm ("movl $1f, %0; movb %b2, %1; 1:"
       : "=&a" (error_code), "=m" (*udst) : "r" (byte));
  return error_code != -1;
}

void check_valid_pointer(const void *vaddr)
{
  if(!is_user_vaddr(vaddr))
  {
    // printf("%s: exit(%d)\n", thread_name(), -1);
    // thread_exit();
    userp_exit(-1);
  }
}

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f)
{
  // ASSERT(f!= NULL);
  // ASSERT(f->esp != NULL);
  // ASSERT(pagedir_get_page(thread_current()->pagedir, f->esp) != NULL);

  //sc-bad-sp
  check_valid_pointer(f->esp);
  if(get_user((uint8_t *)f->esp) == -1)
  {
    userp_exit(-1);
  }
  if(get_user((uint8_t *)f->esp + 4) == -1)
  {
    userp_exit(-1);
  }
  if(get_user((uint8_t *)f->esp + 8) == -1)
  {
    userp_exit(-1);
  }


  int sys_num  = *(uint32_t *)(f->esp);

  int first = *((int *)((f->esp) + 4));  //fd or file or pid
  void *second = *((void **)((f->esp) + 8));
  unsigned third = *((unsigned*)((f->esp) + 12));

  int i;

  switch(sys_num){
    //syscall0 (SYS_HALT);
    case SYS_HALT: //0
    {
      //halt();
      power_off();
      break;
    }

    //syscall1 (SYS_EXIT, status);
    case SYS_EXIT: //1
    {
      check_valid_pointer((f->esp) + 4); //status
      int status = (int)*(uint32_t *)((f->esp) + 4);

      userp_exit(status);
      break;
    }

    //syscall1 (SYS_EXEC, file);
    case SYS_EXEC: //2
    {
      check_valid_pointer((f->esp) + 4); //file = first
      f->eax = process_execute(*(const char **)(f->esp+4));
      //process_execute(*(char **)((f->esp) + 4));
      break;
    }

    //syscall1 (SYS_WAIT, pid);
    case SYS_WAIT: //3   //FIXME:
    {
      check_valid_pointer((f->esp) + 4); //pid = tid = first
      f->eax = process_wait((tid_t)first);
      // process_wait(thread_tid());
      break;
    }

    //syscall2 (SYS_CREATE, file, initial_size);
    case SYS_CREATE: //4
    {
      if(first == NULL)
      {
        userp_exit(-1);
      }
      check_valid_pointer((f->esp) + 4); //file = first
      check_valid_pointer((f->esp) + 8); //initial_size = second
      check_valid_pointer(second); //also a pointer

      sema_down(&file_sema);
      f->eax = filesys_create((const char *)first, (int32_t)(second), false);
      sema_up(&file_sema);
      break;
    }

    //syscall1 (SYS_REMOVE, file);
    case SYS_REMOVE: //5
    {
      if(first == NULL)
      {
        userp_exit(-1);
      }
      check_valid_pointer((f->esp) + 4); //file = first
      sema_down(&file_sema);
      f->eax = filesys_remove((const char *)first);
      sema_up(&file_sema);
      break;
    }

    //syscall1 (SYS_OPEN, file);
    case SYS_OPEN: //6
    {
      if(first == NULL)
      {
        userp_exit(-1);
      }
      check_valid_pointer((f->esp) + 4); //file = first
      check_valid_pointer(*(char **)(f->esp + 4)); //also a pointer
      // if(get_user((uint8_t *)(f->esp + 4)) == -1) //check if null or unmapped
      // {
      //   exit(-1);
      // }

      struct file* file = *(char **)(f->esp + 4);
      sema_down(&file_sema);
      struct file* fp = filesys_open(*(char **)(f->esp + 4));
      sema_up(&file_sema);

      if(fp == NULL)  //file could not opened
      {
        f->eax = -1;
      }
      else
      {
        f->eax = -1;
        sema_down(&file_sema);
        if(strcmp(thread_current()->name, file) == 0) //FIXME: rox check
        {
          file_deny_write(fp);
        }
        sema_up(&file_sema);

        for(i = 3; i < 128; ++i)
        {
          if(thread_current()->f_d[i] == NULL)
          {

            thread_current()->f_d[i] = fp;
            f->eax = i;
            break;  //end for loop
          }
        }
      }

      break;  //end open
    }

    //syscall1 (SYS_FILESIZE, fd);
    case SYS_FILESIZE: //7
    {
      if(thread_current()->f_d[first] == NULL)
      {
        userp_exit(-1);
      }
      // if(fd == NULL)
      // {
      //   exit(-1);
      // }
      check_valid_pointer((f->esp) + 4); //fd = first
      sema_down(&file_sema);
      f->eax = file_length(thread_current()->f_d[first]);
      sema_up(&file_sema);
      break;
    }

    //syscall3 (SYS_READ, fd, buffer, size);
    case SYS_READ: //8
    {
      check_valid_pointer((f->esp) + 4); //fd = first
      check_valid_pointer((f->esp) + 8); //buffer = second
      check_valid_pointer((f->esp) + 12); //size = third
      check_valid_pointer(second); //also a pointer

      if(get_user((uint8_t *)(f->esp + 4)) == -1) //check if null or unmapped
      {
        userp_exit(-1);
      }

      int i;
      if (first == 0)  //stdin: keyboard input from input_getc()
      {
        for(i = 0; i < third; ++i)
        {
          if(put_user(second++, input_getc())==-1)
          {
            break;
          }
          // if(((char *)second)[i] == NULL)
          // {
          //   break; //remember i
          // }
        }
      }
      else if(first > 2)  //not stdin
      {
        if(thread_current()->f_d[first] == NULL)
        {
          userp_exit(-1);
        }
        if(get_user(second) == -1)
        {
          userp_exit(-1);
        }

        sema_down(&file_sema);
        f->eax = file_read(thread_current()->f_d[first], second, third);
        sema_up(&file_sema);
        break; //end read
      }
      f->eax = i;
      break;
    }

    //syscall3 (SYS_WRITE, fd, buffer, size);
    case SYS_WRITE: //9
    {
      check_valid_pointer((f->esp) + 4); //fd = first
      check_valid_pointer((f->esp) + 8); //buffer = second
      check_valid_pointer((f->esp) + 12); //size = third
      check_valid_pointer(second);  //also a pointer

      //check buffer validity
      for(i = 0; i < third; ++i)
      {
        if(get_user(second + i) == -1)  //!is_user_vaddr(second + i)
        {
          userp_exit(-1);
        }
      }

      int fd = first;
      if(fd == 1)  //stdout: console io
      {
        putbuf(second, third);
        f->eax = third;
        break; //end write
      }
      else if(fd > 2)  //not stdout
      {
        if(thread_current()->f_d[fd] == NULL)
        {
          userp_exit(-1);
        }
        if(thread_current()->f_d[fd]->deny_write)  //FIXME: rox check
        {
          sema_down(&file_sema);
          file_deny_write(thread_current()->f_d[fd]);
          sema_up(&file_sema);
        }

        sema_down(&file_sema);
        f->eax = file_write(thread_current()->f_d[fd], second, third);
        sema_up(&file_sema);
        break;  //end write
      }
      f->eax = -1;
      break;
    }

    //syscall2 (SYS_SEEK, fd, position);
    case SYS_SEEK: //10
    {
      int fd = first;
      if(thread_current()->f_d[fd] == NULL)
      {
        userp_exit(-1);
      }
      check_valid_pointer((f->esp) + 4); //fd = first
      check_valid_pointer((f->esp) + 8); //buffer = second
      check_valid_pointer(second); //also a pointer

      sema_down(&file_sema);
      file_seek(thread_current()->f_d[fd], (unsigned)second);
      sema_up(&file_sema);
      break;
    }

    //return syscall1 (SYS_TELL, fd);
    case SYS_TELL: //11
    {
      int fd = first;
      if(thread_current()->f_d[fd] == NULL)
      {
        userp_exit(-1);
      }
      check_valid_pointer((f->esp) + 4); //fd = first

      sema_down(&file_sema);
      file_tell(thread_current()->f_d[fd]);
      sema_up(&file_sema);
      break;
    }

    //syscall1 (SYS_CLOSE, fd);
    case SYS_CLOSE: //12
    {
      int fd = first;
      if(thread_current()->f_d[fd] == NULL)
      {
        userp_exit(-1);
      }
      check_valid_pointer((f->esp) + 4); //fd = first

      sema_down(&file_sema);
      file_allow_write(thread_current()->f_d[fd]);
      file_close(thread_current()->f_d[fd]);
      // file_allow_write(thread_current()->f_d[fd]);  //FIXME: it occurs error ... wrong position?
      sema_up(&file_sema);

      thread_current()->f_d[fd] = NULL;  //file closed -> make it NULL
      break;
    }

    case SYS_CHDIR:
    {
        check_valid_pointer((f->esp) + 4);
        bool success = false;
        char *name_ = palloc_get_page(0);
        char *file_name = palloc_get_page(0);

        strlcpy(name_, (char **)(f->esp + 4), PGSIZE);
        if (name_ == NULL || file_name == NULL)
        {
          success = false;
        }
          struct dir *dir = path_to_dir(name_, file_name);
          struct inode *inode;
          struct thread *cur_t = thread_current();
          if(dir == NULL || strlen(file_name) == 0)
          {
            dir_close(cur_t->dir);
            cur_t->dir = dir_open(inode);
            success = true;
          }
          else
          {
            if(dir_lookup(dir, file_name, &inode))
            {
              dir_close(cur_t->dir);
              cur_t->dir = dir_open(inode);
              success = true;
            }
          }


        dir_close(dir);
        palloc_free_page(name_);
        palloc_free_page(file_name);

        f->eax = success;

        break;
    }
    case SYS_MKDIR:
    {
      check_valid_pointer((f->esp) + 4);
      /*
      disk_sector_t inode_sector = 0;
      struct inode *inode;

      char *name_ = palloc_get_page(0);
      char *file_name = palloc_get_page(0);
      strlcpy(name_, (char **)(f->esp + 4) , PGSIZE);

      struct dir *dir = path_to_dir(name_, file_name);
      bool success = (dir != NULL
                  &&!dir_lookup(dir, file_name, &inode)
                  && free_map_allocate(1, &inode_sector)
                  && dir_create (inode_sector, 16)
                  && dir_add (dir, file_name, inode_sector));

      if (success == false)
      {
        if(inode_sector != 0)
          free_map_release(inode_sector, 1);
      }
      else
      {
        if(dir_lookup(dir, file_name, &inode))
        {
          struct dir *ch_dir = dir_open(inode);
          dir_add(ch_dir, ".", inode_get_inumber(inode));
          dir_add(ch_dir, "..", inode_get_inumber(dir_get_inode(dir)));
          dir_close(ch_dir);
        }
      }

      dir_close(dir);
      palloc_free_page(name_);
      palloc_free_page(file_name);
      */

      f->eax = filesys_create(first,0,true);

      break;
    }

    case SYS_READDIR:
    {
      check_valid_pointer((f->esp) + 4); //fd = first
      check_valid_pointer((f->esp) + 8); //buffer = second
      bool success = true;
      struct file *fp = thread_current()->f_d[first];
      bool inode_dir;
      struct inode_disk *disk_inode = NULL;
      disk_inode = calloc(1, sizeof *disk_inode);
      if(disk_inode == NULL || file_get_inode(fp) == NULL)
        inode_dir = false;
      else
        {
          cache_read(file_get_inode(fp), disk_inode); //?
          if (disk_inode->is_dir == true)
            inode_dir = true;
          else
            inode_dir = false;
          free(disk_inode);
        }
        if(inode_dir)
        {
          if(!dir_readdir((struct dir*)fp, second))
            success = false;
        }
        else
          success = false;

        f->eax = success;
        break;
    }

    case SYS_ISDIR:
    {
      struct file *fp = thread_current()->f_d[first];
      bool inode_dir;
      struct inode_disk *disk_inode = NULL;
      disk_inode = calloc(1, sizeof *disk_inode);
      if(disk_inode == NULL || file_get_inode(fp) == NULL)
        inode_dir = false;
      else
        {
          cache_read(file_get_inode(fp), disk_inode); //?
          if (disk_inode->is_dir == true)
            inode_dir = true;
          else
            inode_dir = false;
          free(disk_inode);
        }

      f->eax = inode_dir;
      break;
    }

    case SYS_INUMBER:
    {
      struct file *fp = thread_current()->f_d[first];
      if(fp == NULL)
        f->eax = -1;
      else
        f->eax = inode_get_inumber(file_get_inode(fp));

      break;
    }

  }

  //thread_exit ();  //initial
}


void userp_exit (int status)  //userprog_exit
{
  int i;
  thread_current()->exit_status = status;
  for(i = 3; i < 128; ++i)
  {
    if(thread_current()->f_d[i] != NULL)  //close all files before die
    {
      sema_down(&file_sema);
      file_close(thread_current()->f_d[i]);
      sema_up(&file_sema);
    }
  }
  printf("%s: exit(%d)\n", thread_name(), status);
  thread_exit();
}

//test for git
