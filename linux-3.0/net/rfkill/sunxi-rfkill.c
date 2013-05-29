#include <linux/module.h>
#include <linux/init.h>
#include <linux/rfkill.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <mach/sys_config.h>

#if defined CONFIG_BT_HCIUART_DEBUG
#define RF_MSG(...)     do {printk("[rfkill]: "__VA_ARGS__);} while(0)
#else
#define RF_MSG(...)
#endif

#if (defined CONFIG_MMC_SUNXI_POWER_CONTROL)
extern int mmc_pm_get_mod_type(void);
extern int mmc_pm_gpio_ctrl(char* name, int level);
extern int mmc_pm_get_io_val(char* name);
#else
static __inline int mmc_pm_get_mod_type(void) {return 0;}
static __inline int mmc_pm_gpio_ctrl(char* name, int level) {return -1;}
static __inline int mmc_pm_get_io_val(char* name) {return -1;}
#endif

#define	AW_SUPPORT_1IN1_BT

static DEFINE_SPINLOCK(bt_power_lock);
static const char bt_name[] = "bcm4329";
static struct rfkill *sw_rfkill;
static int gpio_int_hdle = 0;
static int gpio_wakeup_hdle = 0;
static int gpio_reset_hdle = 0;
static int bt_reset_enable = 0;
static int bt_wakeup_enable = 0;

#ifdef	AW_SUPPORT_1IN1_BT
static void rfkill_bt_set_power(bool blocked)
{
	int ret;
	printk("rfkill set BT power %d\n", blocked);
	if(!blocked)
	{
    	if(1 == bt_wakeup_enable)
		{
			if(EGPIO_SUCCESS != gpio_write_one_pin_value(gpio_wakeup_hdle, 1, "bt_wakeup")){
            printk("rfkill_bt_set_power: err when operate gpio. \n");
        	}
			printk("rfkill set BT wakeup 1 \n");
		}
		if(1 == bt_reset_enable)
		{
			if(EGPIO_SUCCESS != gpio_write_one_pin_value(gpio_reset_hdle, 1, "bt_rst")){
            printk("rfkill_bt_set_power: err when operate reset. \n");
        	}
		}
		msleep(50);

	}
	else
	{
		if(1 == bt_wakeup_enable)
		{
			if(EGPIO_SUCCESS != gpio_write_one_pin_value(gpio_wakeup_hdle, 0, "bt_wakeup")){
            printk("rfkill_bt_set_power: err when operate gpio. \n");
        	}
		}
		
		//add close bt
		if(1 == bt_reset_enable)
		{
			if(EGPIO_SUCCESS != gpio_write_one_pin_value(gpio_reset_hdle, 0, "bt_rst")){
            printk("rfkill_bt_set_power: err when operate reset. \n");
        	}
		}
	}
	
}
#endif	//AW_SUPPORT_1IN1_BT

static int rfkill_set_power(void *data, bool blocked)
{
    unsigned int mod_sel = mmc_pm_get_mod_type();
    
    RF_MSG("rfkill set power %d\n", blocked);
#ifdef	AW_SUPPORT_1IN1_BT	
    rfkill_bt_set_power(blocked);
#endif	
    spin_lock(&bt_power_lock);
    switch (mod_sel)
    {
        case 2: /* usi bm01a */
            if (!blocked) {
                mmc_pm_gpio_ctrl("usi_bm01a_bt_regon", 1);
                mmc_pm_gpio_ctrl("usi_bm01a_bt_rst", 1);
            } else {
                mmc_pm_gpio_ctrl("usi_bm01a_bt_regon", 0);
                mmc_pm_gpio_ctrl("usi_bm01a_bt_rst", 0);
            }
            break;
        case 5: /* swb b23 */
            if (!blocked) {
                mmc_pm_gpio_ctrl("swbb23_bt_shdn", 1);
            } else {
                mmc_pm_gpio_ctrl("swbb23_bt_shdn", 0);
            }
            break;
        case 6: /* huawei mw269x */
            if (!blocked) {
                mmc_pm_gpio_ctrl("hw_mw269x_bt_wake", 1);
                mmc_pm_gpio_ctrl("hw_mw269x_bt_enb", 1);
            } else {
                mmc_pm_gpio_ctrl("hw_mw269x_bt_enb", 0);
                mmc_pm_gpio_ctrl("hw_mw269x_bt_wake", 0);
            }
            break;
        case 8: /* bcm40183 */
            if (!blocked) {
                mmc_pm_gpio_ctrl("bcm40183_bt_regon", 1);
                mmc_pm_gpio_ctrl("bcm40183_bt_rst", 1);
            } else {
                mmc_pm_gpio_ctrl("bcm40183_bt_regon", 0);
                mmc_pm_gpio_ctrl("bcm40183_bt_rst", 0);
            }
            break;
         case 9: /* realtek rtl8723as */
            if (!blocked) {
                mmc_pm_gpio_ctrl("rtk_rtl8723as_bt_dis", 1);
            } else {
                mmc_pm_gpio_ctrl("rtk_rtl8723as_bt_dis", 0);
            }
            break;            
        default:
            RF_MSG("no bt module matched !!\n");
    }
    
    spin_unlock(&bt_power_lock);
    msleep(100);
    return 0;
}

static struct rfkill_ops sw_rfkill_ops = {
    .set_block = rfkill_set_power,
};

static int sw_rfkill_probe(struct platform_device *pdev)
{
    int ret = 0;

    sw_rfkill = rfkill_alloc(bt_name, &pdev->dev, 
                        RFKILL_TYPE_BLUETOOTH, &sw_rfkill_ops, NULL);
    if (unlikely(!sw_rfkill))
        return -ENOMEM;

    ret = rfkill_register(sw_rfkill);
    if (unlikely(ret)) {
        rfkill_destroy(sw_rfkill);
    }
    return ret;
}

static int sw_rfkill_remove(struct platform_device *pdev)
{
    if (likely(sw_rfkill)) {
        rfkill_unregister(sw_rfkill);
        rfkill_destroy(sw_rfkill);
    }
    return 0;
}

static struct platform_driver sw_rfkill_driver = {
    .probe = sw_rfkill_probe,
    .remove = sw_rfkill_remove,
    .driver = { 
        .name = "sunxi-rfkill",
        .owner = THIS_MODULE,
    },
};

static struct platform_device sw_rfkill_dev = {
    .name = "sunxi-rfkill",
};

#ifdef	AW_SUPPORT_1IN1_BT
static void sw_bt_fetch_para(void)
{
	int bt_used = -1;
	printk("=========bt fetch-para============\n");	

    if(SCRIPT_PARSER_OK != script_parser_fetch("bt_para", "bt_used", &bt_used, 1)){
        return;
    }
    if(1 != bt_used){
        return;
    }
	
	gpio_int_hdle = gpio_request_ex("bt_para", "bt_gpio");
    if(!gpio_int_hdle) {
        pr_warning(" BT gpio_para request gpio fail!\n");
        return;
    }
    
    gpio_wakeup_hdle = gpio_request_ex("bt_para", "bt_wakeup");
    if(!gpio_wakeup_hdle) {
        bt_wakeup_enable = 0; 
    }else{
        bt_wakeup_enable = 1; 
    }
    printk("bt_wakeup_enable = %d. \n", bt_wakeup_enable);
 
    gpio_reset_hdle = gpio_request_ex("bt_para", "bt_rst");
    if(!gpio_reset_hdle) {
        bt_reset_enable = 0;
    }else{
        bt_reset_enable = 1;
    }
    printk("bt_reset_enable = %d. \n", bt_reset_enable);



}
#endif	//AW_SUPPORT_1IN1_BT

static int __init sw_rfkill_init(void)
{
#ifdef	AW_SUPPORT_1IN1_BT
	sw_bt_fetch_para();
#endif	
    platform_device_register(&sw_rfkill_dev);
    return platform_driver_register(&sw_rfkill_driver);
}

static void __exit sw_rfkill_exit(void)
{
#ifdef	AW_SUPPORT_1IN1_BT
	gpio_release(gpio_int_hdle, 2);
    gpio_release(gpio_wakeup_hdle, 2);
    gpio_release(gpio_reset_hdle, 2);
#endif	
    platform_device_unregister(&sw_rfkill_dev);
    platform_driver_unregister(&sw_rfkill_driver);
}

module_init(sw_rfkill_init);
module_exit(sw_rfkill_exit);

MODULE_DESCRIPTION("sunxi-rfkill driver");
MODULE_AUTHOR("Aaron.yemao<leafy.myeh@allwinnertech.com>");
MODULE_LICENSE(GPL);

