#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
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
	{ 0xb11e775, "module_layout" },
	{ 0x6d26bb71, "kmalloc_caches" },
	{ 0x12da5bb2, "__kmalloc" },
	{ 0x2b688622, "complete_and_exit" },
	{ 0xfbc74f64, "__copy_from_user" },
	{ 0x3ec8886f, "param_ops_int" },
	{ 0x67c2fa54, "__copy_to_user" },
	{ 0x2e5810c6, "__aeabi_unwind_cpp_pr1" },
	{ 0x58b9ccb6, "dev_set_drvdata" },
	{ 0xc8b57c27, "autoremove_wake_function" },
	{ 0x5cca14d3, "send_sig" },
	{ 0xc2df94b5, "find_vpid" },
	{ 0x454bf58d, "sdio_writesb" },
	{ 0x5ab68df7, "sdio_enable_func" },
	{ 0xa9b45071, "mmc_pm_gpio_ctrl" },
	{ 0x2d9cb533, "sdio_claim_irq" },
	{ 0xd4a178de, "mmc_wait_for_cmd" },
	{ 0xb0bb9c02, "down_interruptible" },
	{ 0x9d56a6f9, "ns_net_rx" },
	{ 0xeae3dfd6, "__const_udelay" },
	{ 0xbaaaae3, "nanonet_detach" },
	{ 0xf6288e02, "__init_waitqueue_head" },
	{ 0xe707d823, "__aeabi_uidiv" },
	{ 0x5baaba0, "wait_for_completion" },
	{ 0xfa2a45e, "__memzero" },
	{ 0xac84e060, "sunximmc_rescan_card" },
	{ 0x5f754e5a, "memset" },
	{ 0x480b89c, "nanonet_attach" },
	{ 0x3583f1dd, "dev_alloc_skb" },
	{ 0x27e1a049, "printk" },
	{ 0xca63259a, "pid_task" },
	{ 0x6ab98070, "dev_kfree_skb_any" },
	{ 0xd79b5a02, "allow_signal" },
	{ 0x57525adb, "sdio_readsb" },
	{ 0x23ce93ff, "sdio_unregister_driver" },
	{ 0xe5062922, "sdio_f0_writeb" },
	{ 0x70d90d87, "skb_queue_tail" },
	{ 0x80f82a09, "kmem_cache_alloc" },
	{ 0x18195c80, "sdio_release_irq" },
	{ 0x1000e51, "schedule" },
	{ 0x36f019a3, "nano_util_printbuf" },
	{ 0xc27487dd, "__bug" },
	{ 0x5941766a, "sched_setscheduler" },
	{ 0xb9e52429, "__wake_up" },
	{ 0x37a0cba, "kfree" },
	{ 0x9d669763, "memcpy" },
	{ 0x801678, "flush_scheduled_work" },
	{ 0x75a17bed, "prepare_to_wait" },
	{ 0x8cf51d15, "up" },
	{ 0x763ce25d, "nanonet_create" },
	{ 0x8893fa5d, "finish_wait" },
	{ 0x7e9ebb05, "kernel_thread" },
	{ 0xeb0d2f24, "skb_dequeue" },
	{ 0xefd6cf06, "__aeabi_unwind_cpp_pr0" },
	{ 0xf39c5744, "sdio_register_driver" },
	{ 0x45700a96, "consume_skb" },
	{ 0xdcc6a8ab, "sdio_claim_host" },
	{ 0xac8dec7, "skb_put" },
	{ 0x6084aa40, "dev_get_drvdata" },
	{ 0x3b81e9f3, "sdio_set_block_size" },
	{ 0xac7b2cb8, "nanonet_destroy" },
	{ 0xdc43a9c8, "daemonize" },
	{ 0x51d45f, "sdio_disable_func" },
	{ 0x2ba7bcb2, "sdio_release_host" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=nano_if";

MODULE_ALIAS("sdio:c*v03BBd*");
