#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x8d6218e1, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0x3675a080, __VMLINUX_SYMBOL_STR(cdev_del) },
	{ 0x8c92149a, __VMLINUX_SYMBOL_STR(kmalloc_caches) },
	{ 0xd2b09ce5, __VMLINUX_SYMBOL_STR(__kmalloc) },
	{ 0xe6c8b53f, __VMLINUX_SYMBOL_STR(usb_wait_anchor_empty_timeout) },
	{ 0x178df606, __VMLINUX_SYMBOL_STR(pci_release_region) },
	{ 0xda3e43d1, __VMLINUX_SYMBOL_STR(_raw_spin_unlock) },
	{ 0x784213a6, __VMLINUX_SYMBOL_STR(pv_lock_ops) },
	{ 0xc996d097, __VMLINUX_SYMBOL_STR(del_timer) },
	{ 0x20000329, __VMLINUX_SYMBOL_STR(simple_strtoul) },
	{ 0xf22449ae, __VMLINUX_SYMBOL_STR(down_interruptible) },
	{ 0x3f0546a8, __VMLINUX_SYMBOL_STR(ioread32_rep) },
	{ 0x7ab981a7, __VMLINUX_SYMBOL_STR(usb_deregister_dev) },
	{ 0xb95a4053, __VMLINUX_SYMBOL_STR(device_destroy) },
	{ 0x593a99b, __VMLINUX_SYMBOL_STR(init_timer_key) },
	{ 0xde295364, __VMLINUX_SYMBOL_STR(mutex_unlock) },
	{ 0xdee716ff, __VMLINUX_SYMBOL_STR(usb_autopm_get_interface) },
	{ 0x91715312, __VMLINUX_SYMBOL_STR(sprintf) },
	{ 0x7d11c268, __VMLINUX_SYMBOL_STR(jiffies) },
	{ 0xe2d5255a, __VMLINUX_SYMBOL_STR(strcmp) },
	{ 0xbfc177bc, __VMLINUX_SYMBOL_STR(iowrite32_rep) },
	{ 0xf432dd3d, __VMLINUX_SYMBOL_STR(__init_waitqueue_head) },
	{ 0x4f8b5ddb, __VMLINUX_SYMBOL_STR(_copy_to_user) },
	{ 0xfb578fc5, __VMLINUX_SYMBOL_STR(memset) },
	{ 0x91c1926e, __VMLINUX_SYMBOL_STR(dev_err) },
	{ 0x8f64aa4, __VMLINUX_SYMBOL_STR(_raw_spin_unlock_irqrestore) },
	{ 0x87bf1566, __VMLINUX_SYMBOL_STR(usb_deregister) },
	{ 0xae6cd726, __VMLINUX_SYMBOL_STR(__mutex_init) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0x4c9d28b0, __VMLINUX_SYMBOL_STR(phys_base) },
	{ 0xfaef0ed, __VMLINUX_SYMBOL_STR(__tasklet_schedule) },
	{ 0xa1c76e0a, __VMLINUX_SYMBOL_STR(_cond_resched) },
	{ 0x47e7d52b, __VMLINUX_SYMBOL_STR(usb_register_dev) },
	{ 0x16305289, __VMLINUX_SYMBOL_STR(warn_slowpath_null) },
	{ 0x9f2211b6, __VMLINUX_SYMBOL_STR(mutex_lock) },
	{ 0x9545af6d, __VMLINUX_SYMBOL_STR(tasklet_init) },
	{ 0x8834396c, __VMLINUX_SYMBOL_STR(mod_timer) },
	{ 0x8751684c, __VMLINUX_SYMBOL_STR(_dev_info) },
	{ 0xf53b4714, __VMLINUX_SYMBOL_STR(usb_submit_urb) },
	{ 0x78764f4e, __VMLINUX_SYMBOL_STR(pv_irq_ops) },
	{ 0x6156db20, __VMLINUX_SYMBOL_STR(usb_get_dev) },
	{ 0xff1076fc, __VMLINUX_SYMBOL_STR(usb_kill_anchored_urbs) },
	{ 0xdb7305a1, __VMLINUX_SYMBOL_STR(__stack_chk_fail) },
	{ 0x3bd1b1f6, __VMLINUX_SYMBOL_STR(msecs_to_jiffies) },
	{ 0xbe48ab2, __VMLINUX_SYMBOL_STR(usb_bulk_msg) },
	{ 0xd339498f, __VMLINUX_SYMBOL_STR(usb_put_dev) },
	{ 0x1000e51, __VMLINUX_SYMBOL_STR(schedule) },
	{ 0xa202a8e5, __VMLINUX_SYMBOL_STR(kmalloc_order_trace) },
	{ 0x43261dca, __VMLINUX_SYMBOL_STR(_raw_spin_lock_irq) },
	{ 0x9d749dd5, __VMLINUX_SYMBOL_STR(usb_find_interface) },
	{ 0xbdfb6dbb, __VMLINUX_SYMBOL_STR(__fentry__) },
	{ 0xc14e944e, __VMLINUX_SYMBOL_STR(kmem_cache_alloc_trace) },
	{ 0xd52bf1ce, __VMLINUX_SYMBOL_STR(_raw_spin_lock) },
	{ 0xcba94256, __VMLINUX_SYMBOL_STR(__dynamic_dev_dbg) },
	{ 0x9327f5ce, __VMLINUX_SYMBOL_STR(_raw_spin_lock_irqsave) },
	{ 0xcf21d241, __VMLINUX_SYMBOL_STR(__wake_up) },
	{ 0x34f22f94, __VMLINUX_SYMBOL_STR(prepare_to_wait_event) },
	{ 0x4f68e5c9, __VMLINUX_SYMBOL_STR(do_gettimeofday) },
	{ 0x37a0cba, __VMLINUX_SYMBOL_STR(kfree) },
	{ 0x69acdf38, __VMLINUX_SYMBOL_STR(memcpy) },
	{ 0xd3719d59, __VMLINUX_SYMBOL_STR(paravirt_ticketlocks_enabled) },
	{ 0x71e3cecb, __VMLINUX_SYMBOL_STR(up) },
	{ 0xe6638d26, __VMLINUX_SYMBOL_STR(usb_register_driver) },
	{ 0xfa66f77c, __VMLINUX_SYMBOL_STR(finish_wait) },
	{ 0x7ad130d7, __VMLINUX_SYMBOL_STR(dev_warn) },
	{ 0xb0e602eb, __VMLINUX_SYMBOL_STR(memmove) },
	{ 0x436c2179, __VMLINUX_SYMBOL_STR(iowrite32) },
	{ 0x4f6b400b, __VMLINUX_SYMBOL_STR(_copy_from_user) },
	{ 0xb3cd0096, __VMLINUX_SYMBOL_STR(usb_free_urb) },
	{ 0xb3e6df83, __VMLINUX_SYMBOL_STR(dma_ops) },
	{ 0x8572770f, __VMLINUX_SYMBOL_STR(usb_autopm_put_interface) },
	{ 0xe484e35f, __VMLINUX_SYMBOL_STR(ioread32) },
	{ 0x5e64ca16, __VMLINUX_SYMBOL_STR(usb_alloc_urb) },
	{ 0xf20dabd8, __VMLINUX_SYMBOL_STR(free_irq) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

MODULE_ALIAS("usb:v18F7p0030d*dc*dsc*dp*ic*isc*ip*in*");

MODULE_INFO(srcversion, "148B929D4E21C30A5A07628");
