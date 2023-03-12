#ifndef INC_FUNCS
#define INC_FUNCS

#include "ini.h"

static size_t g_phy_total_memory_size = 0; // 物理内存总大小

#define MAX_MAPINDEX 7000

struct MapTable
{

	uint64_t start;
	uint64_t end;
	char flags[4];
	char name[1024];
};

static int GetMaps(struct MapTable *OutMaps)
{
	struct task_struct *task;
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	
	char path_buf[512];
	char *name;
	int index = 0;


	task = pid_task(proc_pid_struct, PIDTYPE_PID);
	if (!task)
	{
		return -2;
	}

	mm = get_task_mm(task);
	if (!mm) 
	{
		return -3;
	}
	
	
	down_read(&mm->MM_STRUCT_MMAP_LOCK);
	for (vma = mm->mmap; vma; vma = vma->vm_next) 
	{
		if(index>=MAX_MAPINDEX)
		{
			goto out;
		}
		(OutMaps+index)->start = vma->vm_start;
		(OutMaps+index)->end = vma->vm_end;
		*(((OutMaps+index)->flags)+0) = vma->vm_flags & VM_READ ? 'r' : '-';
		*(((OutMaps+index)->flags)+1) = vma->vm_flags & VM_WRITE ? 'w' : '-';
		*(((OutMaps+index)->flags)+2) = vma->vm_flags & VM_EXEC ? 'x' : '-';
		*(((OutMaps+index)->flags)+3) = vma->vm_flags & VM_MAYSHARE ? 's' : 'p';
		*(((OutMaps+index)->flags)+4) ='\0';

		if (vma->vm_file) 
		{
			memset(path_buf, 0, sizeof(path_buf));
			name = d_path(&vma->vm_file->f_path, path_buf, sizeof(path_buf));
			
		} 
		else if (vma->vm_mm && vma->vm_start == (long)vma->vm_mm->context.vdso) 
		{
			name="[vdso]";
		}

		else 
		{
			if (vma->vm_start <= mm->brk &&
				vma->vm_end >= mm->start_brk) 
			{
				name="[heap]";
			} 
			else 
			{
				if (vma->vm_start <= vma->vm_mm->start_stack &&
					vma->vm_end >= vma->vm_mm->start_stack) 
				{						
					name="[stack]";
					
				}
			}
		}
		strcpy((OutMaps+index)->name , name);
		index++;
		printk_debug("%s\n",name);
	}
out:
	up_read(&mm->MM_STRUCT_MMAP_LOCK);
	mmput(mm);
	return index;
}

static int GetModuleBase(uint64_t * outbase , const char *name)
{
	int retval ,i ;
	struct MapTable* vMaps=(struct MapTable*)vmalloc(sizeof(struct MapTable) * MAX_MAPINDEX);
	int index=GetMaps(vMaps);
	printk_debug("vma数量%d" , index);
	
	for (i=0 ; i<index ;i++)
	{
		if(strstr((vMaps+i)->name , name))
		{
			*outbase = (vMaps+i)->start;
			retval=1;
			break;
		}
	}
	vfree(vMaps);
	return retval;
}

static int 虚拟地址转物理地址(char * ppa , uint64_t va , struct mm_struct * tag_mm)
{
	uint64_t page_offset2;
	pgd_t *pgd_tmp = NULL;
	p4d_t *p4d_tmp = NULL;
	pud_t *pud_tmp = NULL;
	pmd_t *pmd_tmp = NULL;
	// pte_t *pte_tmp = NULL;
	// bool res=false;
	// struct vm_area_struct *vma;
	int retval=-1;
	
	
	// vma=find_vma(tag_mm,va);
	
	// if (vma) 
	// {
		// if (vma->vm_flags & VM_READ) 
		// {
			// if (va <= vma->vm_end) 
			// {
				// res = true;
			// }
		// }
	// }
	
	// if(res !=true)
	// {
		// goto out;
	// }
	
	
	pgd_tmp = pgd_offset(tag_mm,va);
	if(pgd_none(*pgd_tmp))
	{
		goto out;
	}
	p4d_tmp = p4d_offset(pgd_tmp,va);
	if(p4d_none(*p4d_tmp))
	{
		goto out;
	}
	pud_tmp = pud_offset(p4d_tmp,va);
	if(pud_none(*pud_tmp))
	{
		goto out;
	}
	pmd_tmp = pmd_offset(pud_tmp,va);
	if(pmd_none(*pmd_tmp))
	{
		goto out;
	}
	
	
	
	pte = pte_offset_kernel(pmd_tmp,va);
	if(pte_none(*pte))
	{
		goto out;
	}
	if(!pte_present(*pte))
	{
		goto out;
	}
	
	
	
	//下为页物理地址
	page_addr = (uint64_t)(pte_pfn(*pte) << PAGE_SHIFT);
	//下为页偏移
	
	page_offset2= va & (PAGE_SIZE-1);


	// page_offset2 = va & ~PAGE_MASK; //虚拟地址在物理地址中的偏移

	//两者相加即用户进程虚拟地址对应的物理地址
	*(uint64_t *)ppa=page_addr+page_offset2;
	printk_debug(KERN_INFO"target phys=0x%lx\n" , *(uint64_t *)ppa);
	
	// up_read(&tag_mm->MM_STRUCT_MMAP_LOCK);
	// mmput(tag_mm);
	retval=1;
out:
	// up_read(&tag_mm->MM_STRUCT_MMAP_LOCK);
	// mmput(tag_mm);
	// up_read(&tag_mm->mmap_sem);
	return retval;
}




static int init_phy_total_memory_size(void) 
{
	struct sysinfo si;
	unsigned long mem_total, sav_total;
	unsigned int bitcount = 0;
	unsigned int mem_unit = 0;
	if (g_phy_total_memory_size) 
	{
		return 0;
	}

	si_meminfo(&si);
	mem_unit = si.mem_unit;

	mem_total = si.totalram;
	while (mem_unit > 1) {
		bitcount++;
		mem_unit >>= 1;
		sav_total = mem_total;
		mem_total <<= 1;
		if (mem_total < sav_total) {
			return 0;
		}
	}
	si.totalram <<= bitcount;
	g_phy_total_memory_size = __pa(si.totalram);
	printk_debug(KERN_INFO "MemTotal si.totalram:%ld\n", si.totalram);
	printk_debug(KERN_INFO "g_phy_total_memory_size:%ld\n", g_phy_total_memory_size);
	return 0;
}

static inline int 检查物理地址(size_t addr, size_t count) 
{
	if (g_phy_total_memory_size == 0) 
	{
		init_phy_total_memory_size();
	}
	return (addr + count) <= g_phy_total_memory_size;
}



static inline unsigned long size_inside_page(unsigned long start,
	unsigned long size)
{
	unsigned long sz;

	sz = PAGE_SIZE - (start & (PAGE_SIZE - 1));

	return min(sz, size);
}

static int 读取物理内存(void* 输出 , uint64_t phy_addr , size_t 大小)
{
	int probe=1;
	int retval=-1;
	char Temp[16];
	
	size_t sz = size_inside_page(phy_addr, 大小);
	char *ptr = xlate_dev_mem_ptr(phy_addr);
	if(!ptr)
	{
		goto out;
	}
	probe = probe_kernel_read(Temp, ptr, sz);
	
	if(probe)
	{
		goto out;
	}
	
	memcpy(输出,Temp,sz);
	
	unxlate_dev_mem_ptr(phy_addr, ptr);
	retval=1;
out:
	return retval;
}



static int 写入物理内存(char* 输入 , uint64_t phy_addr , size_t 大小)
{
	int retval=-1;
	size_t sz = size_inside_page(phy_addr, 大小);
	char *ptr = xlate_dev_mem_ptr(phy_addr);
	if(!ptr)
	{
		goto out;
	}
	memcpy(ptr, 输入, sz);
	unxlate_dev_mem_ptr(phy_addr, ptr);
	retval=1;
out:
	return retval;
}
#endif /* INC_FUNCS */