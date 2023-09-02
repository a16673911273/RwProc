#include <linux/cpu.h>
#include <linux/memory.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <asm-generic/errno-base.h>
 
#ifdef pr_fmt
#undef pr_fmt
#define pr_fmt(fmt) "InlineHookDemo: " fmt
#endif
 
static const uint32_t mbits = 6u;
static const uint32_t mask  = 0xfc000000u; // 0b11111100000000000000000000000000
static const uint32_t rmask = 0x03ffffffu; // 0b00000011111111111111111111111111
static const uint32_t op_bl = 0x94000000u; // "bl" ADDR_PCREL26
 
typedef long (*do_faccessat_t)(int, const char __user *, int, int) ;
static do_faccessat_t my_do_faccessat;
 
unsigned int orig_insn, hijack_insn;
unsigned long func_addr, insn_addr = 0;
 
uintptr_t kprobe_get_addr(const char *symbol_name) {
    int ret;
    struct kprobe kp;
    uintptr_t tmp = 0;
    kp.addr = 0;
    kp.symbol_name = symbol_name;
    ret = register_kprobe(&kp);
    tmp = (uintptr_t)kp.addr;
    if (ret < 0) {
        goto out; // not function, maybe symbol
    }
    unregister_kprobe(&kp);
out:
    return tmp;
}
 
bool is_bl_insn(unsigned long addr){
    uint32_t insn = *(uint32_t*)addr;
    const uint32_t opc = insn & mask;
    if (opc == op_bl) {
        return true;
    }
    return false;
}
 
uint64_t get_bl_target(unsigned long addr){
    uint32_t insn = *(uint32_t*)addr;
    int64_t absolute_addr = (int64_t)(addr) + ((int32_t)(insn << mbits) >> (mbits - 2u)); // sign-extended
    return (uint64_t)absolute_addr;
}
 
uint32_t build_bl_insn(unsigned long addr, unsigned long target){
    uint32_t insn = *(uint32_t*)addr;
    const uint32_t opc = insn & mask;
    int64_t new_pc_offset = ((int64_t)target - (int64_t)(addr)) >> 2; // shifted
    uint32_t new_insn = opc | (new_pc_offset & ~mask);
    return new_insn;
}
 
uint32_t get_insn(unsigned long addr){
    return *(unsigned int*)addr;
}
 
void set_insn(unsigned long addr, unsigned int insn){
    cpus_read_lock();
    *(unsigned int*)addr = insn;
    cpus_read_unlock();
}
 
long hijack_do_faccessat(int dfd, const char __user *filename, int mode, int flags){
    char prefix[8];
    pr_emerg("hijack success!");
    copy_from_user(prefix, filename, 8);
    prefix[7] = 0;
    pr_emerg("access: %s", prefix);
    if (strcmp(prefix, "/memfd:") == 0) {
        pr_emerg("magic!");
        return 0;
    }
    return my_do_faccessat(dfd, filename, mode, flags);
}
 
int ihd_init(void){
    int i;
 
    // 获取函数地址
    func_addr = kprobe_get_addr("__arm64_sys_faccessat");
    pr_emerg("func_addr:%lX, ", func_addr);
    // 遍历内存找到BL指令地址
    for(i = 0; i < 0x100; i++){
        if (is_bl_insn(func_addr + i * 0x4)) {
            insn_addr = func_addr + i * 0x4;
            break;
        }
    }
    if (insn_addr == 0) { // 未找到BL指令
        return -ENOENT;
    }
    orig_insn = get_insn(insn_addr);
    my_do_faccessat = (do_faccessat_t)insn_addr;
    pr_emerg("insn_addr:%lX, ", insn_addr);
    pr_emerg("orig_insn:%X orig_target_addr:%lX", orig_insn, get_bl_target(insn_addr));
    hijack_insn = build_bl_insn(insn_addr, (unsigned long)&hijack_do_faccessat);
    set_insn(insn_addr, hijack_insn);
    pr_emerg("new_insn:%X new_target_addr:%lX", hijack_insn, get_bl_target(insn_addr));
    return 0;
}
 
void ihd_exit(void){
    // 恢复修改
    set_insn(insn_addr, orig_insn);
}
 
module_init(ihd_init);
module_exit(ihd_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ylarod");
MODULE_DESCRIPTION("A simple inline hook demo");
