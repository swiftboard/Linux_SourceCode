/*
 * modules/cedarx/cedar_interface.c
 * (C) Copyright 2007-2011
 * Reuuimlla Technology Co., Ltd. <www.reuuimllatech.com>
 * huangxin <huangxin@Reuuimllatech.com>
 *
 * some simple description for this code
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */
#include <linux/init.h>
#include <linux/module.h>
#include "cedar_interface.h"

static int __init init_cedar_libmodule(void)
{
	printk("%s,%d\n", __func__, __LINE__);
	cedardev_init();
	return 0;
}
module_init(init_cedar_libmodule);

static void __exit exit_cedar_libmodule(void)
{
	cedardev_exit();
}
module_exit(exit_cedar_libmodule);

MODULE_AUTHOR("Soft-Reuuimlla");
MODULE_DESCRIPTION("User mode CEDAR device interface");
MODULE_LICENSE("GPL");
