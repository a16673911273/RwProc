#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/inotify.h>

MODULE_LICENSE("GPL");

asmlinkage long my_inotify_init(void)
{
    struct inotify_handle *handle;

    /* 调用原始的inotify_init函数创建inotify实例 */
    handle = (struct inotify_handle *)kallsyms_lookup_name("inotify_init");
    if (!handle)
        return -ENOSYS;

    /* 修改inotify实例以便返回假的事件 */
    handle->pevents = 0;

    /* 返回inotify实例的文件描述符 */
    return handle->fd;
}

static struct symbol_symbol my_symbols[] = {
    { "inotify_init", (unsigned long) my_inotify_init },
    { NULL, 0 }
};

static int my_init_module(void)
{
    if (symbol_table_install(my_symbols) != 0) {
        printk(KERN_ERR "Failed to install symbol table.\n");
        return -ENOSYS;
    }

    return 0;
}

static void my_cleanup_module(void)
{
    symbol_table_uninstall(my_symbols);
}

module_init(my_init_module);
module_exit(my_cleanup_module);
