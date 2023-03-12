/*
PhyRW 1.5
一次最高读八字节
*/

#include "ini.h"
#include "func.h"
#include "fixFork.h"

#ifndef VM_RESERVED
#define VM_RESERVED (VM_DONTEXPAND | VM_DONTDUMP)
#endif


static ssize_t 读操作(struct file *file, char __user *buf,
			size_t 读取大小, loff_t *ppos)
{
	int 结果;

	
	switch (用户指令)
	{
		case 指定进程:
			proc_pid_struct = find_get_pid(目标pid);
			判断(proc_pid_struct);
			printk_debug("proc_pid_struct 0x%lx\n",proc_pid_struct);
			tag_task = get_pid_task(proc_pid_struct, PIDTYPE_PID);
			判断(tag_task);
			put_task_struct(tag_task);
			printk_debug("tag_task 0x%lx\n",tag_task);
			tag_mm = get_task_mm(tag_task);
			判断(tag_mm);
			
			mmput(tag_mm);
			
			printk_debug("tag_mm 0x%lx\n",tag_mm);
			// 判断(结果);
		break;
		
		
		case 转化内存:
			结果=虚拟地址转物理地址(写入内容 , 目标地址 , tag_mm);
			判断(结果);
		break;

		case 读取内存:
			结果=读取物理内存(写入内容 , 目标地址 , 读取大小);
			判断(结果);
		break;
		
		case 获取基址:
			结果=GetModuleBase(&(映射区->地址) , 映射区->内容);
			判断(结果);
		break;
		
		case 获取cmdline地址:
			结果=GetModuleBase(&(映射区->地址) , 映射区->内容);
			判断(结果);
		break;
	}
	return 0;
}


static ssize_t 写操作(struct file* filp, const char __user* buf, size_t 写入大小, loff_t *ppos)
{
	int 结果;
	结果=写入物理内存(映射区->内容 , 映射区->地址 , 写入大小);
	判断(结果);
	return 0;
}




static int 打开操作(struct inode *inode, struct file *file)
{
	printk_debug("打开驱动了");
	return 0;
}


static int 关闭操作(struct inode *inode, struct file *filp)
{
	printk_debug("关闭驱动了");
	if(proc_pid_struct)
		put_pid(proc_pid_struct);
	return 0;
}



static int 映射内存(struct file *file, struct vm_area_struct *vma)
{
	unsigned long page;
	unsigned long start = (unsigned long)vma->vm_start;
	// unsigned long end =  (unsigned long)vma->vm_end;
	unsigned long size = (unsigned long)(vma->vm_end - vma->vm_start);

	// 得到物理地址
	page = virt_to_phys(映射区);
	// 将用户空间的一个vma虚拟内存区映射到以page开始的一段连续物理页面上
	if (remap_pfn_range(vma, start, page >> PAGE_SHIFT, size,
						PAGE_SHARED)) // 第三个参数是页帧号，由物理地址右移PAGE_SHIFT得到
		return -1;


	映射区->指令='s';
	映射区->返回信息='b';
	映射区->地址=0x7A4AA29000;
	*(long*)映射区->内容=3585345619;
	return 0;
}



static struct file_operations dev_fops = {
	.owner = THIS_MODULE,
	.open = 打开操作,
	.release = 关闭操作,
	.mmap = 映射内存,
	.read = 读操作,
	.write = 写操作,
};


int __init dev_init(void)
{
	int result;
	int err;
	//动态申请设备号
	result = alloc_chrdev_region(&g_rwProcMem_devno, 0, 1, DEV_FILENAME);
	动态设备号 = MAJOR(g_rwProcMem_devno);

	if (result < 0)
	{
		printk(KERN_EMERG "动态申请设备号失败 %d\n", result);
		return result;
	}

	// 2.动态申请设备结构体的内存
	映射区 = (struct 桥梁 *)kmalloc(PAGE_SIZE, GFP_KERNEL);
	
	if (!映射区)
	{
		//申请失败
		result = -ENOMEM;
		goto _fail;
	}
	
	//3.初始化并且添加cdev结构体
	cdev_init(&设备结构体, &dev_fops); //初始化cdev设备
	设备结构体.owner = THIS_MODULE; //使驱动程序属于该模块
	设备结构体.ops = &dev_fops; //cdev连接file_operations指针
	//将cdev注册到系统中
	err = cdev_add(&设备结构体, g_rwProcMem_devno, 1);
	if (err)
	{
		printk(KERN_NOTICE "Error in cdev_add()\n");
		result = -EFAULT;
		goto _fail;
	}

	//4.创建设备文件
	设备类 = class_create(THIS_MODULE, DEV_FILENAME); //创建设备类（位置在/sys/class/xxxxx）
	device_create(设备类, NULL, g_rwProcMem_devno, NULL, "%s", DEV_FILENAME); //创建设备文件（位置在/dev/xxxxx）



    // 将该段内存设置为保留
	SetPageReserved(virt_to_page(映射区));

	printk(KERN_EMERG "Hello, %s\n", DEV_FILENAME);
	
	return 0;
_fail:
	unregister_chrdev_region(g_rwProcMem_devno, 1);
	return result;
}



void __exit dev_exit(void)
{
	device_destroy(设备类, g_rwProcMem_devno); //删除设备文件（位置在/dev/xxxxx）
	class_destroy(设备类); //删除设备类

	cdev_del(&设备结构体); //注销cdev
	ClearPageReserved(virt_to_page(映射区));
	kfree(映射区); // 释放内存
	unregister_chrdev_region(g_rwProcMem_devno, 1); //释放设备号

	printk(KERN_EMERG "Goodbye, %s\n", DEV_FILENAME);
}

module_init(dev_init);
module_exit(dev_exit);
MODULE_LICENSE("GPL");
MODULE_INFO(intree, "Y");
MODULE_AUTHOR("QQ3585345619");