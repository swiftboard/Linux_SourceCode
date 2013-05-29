/*
*************************************************************************************
*                         			      Linux
*					                 USB Host Driver
*
*				        (c) Copyright 2006-2012, SoftWinners Co,Ld.
*							       All Rights Reserved
*
* File Name 	: sw_usb_mu509.c
*
* Author 		: javen
*
* Description 	: USB 3G operations
*
* History 		:
*      <author>    		<time>       	<version >    		<desc>
*       javen     	  2012-4-10            1.0          create this file
*
*************************************************************************************
*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/signal.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/pm.h>
#include <linux/earlysuspend.h>
#endif

#include <linux/time.h>
#include <linux/timer.h>

#include <mach/sys_config.h>
#include <linux/clk.h>

#include  <mach/clock.h>
#include "sw_hci_sun4i.h"

//-----------------------------------------------------------------------------
//   debug
//-----------------------------------------------------------------------------
#define  USB_MU509_DEBUG

#ifdef  USB_MU509_DEBUG
#define  usb_3g_dbg(stuff...)		printk(stuff)
#define  usb_3g_err(...) (usb_3g_dbg("err:L%d(%s):", __LINE__, __FILE__), usb_3g_dbg(__VA_ARGS__))
#else
#define  usb_3g_dbg(...)
#define  usb_3g_err(...)
#endif


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

/* HUAWEI MU509 HSDPA LGA Module Hardware Guide */
struct sw_usb_mu509{
    struct work_struct irq_work;
    spinlock_t lock;

#ifdef CONFIG_HAS_EARLYSUSPEND
    struct early_suspend early_suspend;
#endif

    u32 used;
    u32 usbc_no;                        /* 挂载的USB控制器编号 */
    u32 usbc_type;                      /* 挂载的USB控制器控制器类型 */
    u32 uart_no;                        /* 挂载的uart控制器 */

    u32 vbat_valid;
    user_gpio_set_t vbat_set;           /* vbat pin, mu509总电源 */
    u32 vbat_hd;

    u32 power_on_off_valid;
    user_gpio_set_t power_on_off_set;   /* power_on_off pin */
    u32 power_on_off_hd;

    u32 reset_valid;
    user_gpio_set_t reset_set;          /* reset pin */
    u32 reset_hd;

    u32 wakeup_in_valid;
    user_gpio_set_t wakeup_in_set;     /* wakeup_out pin, A10 wakeup/sleep mu509 */
    u32 wakeup_in_hd;

};

static struct sw_usb_mu509 g_usb_mu509;


/*
*******************************************************************************
*                     mu509_get_config
*
* Description:
*    void
*
* Parameters:
*    void
*
* Return value:
*    void
*
* note:
*    void
*
*******************************************************************************
*/
static s32 mu509_get_config(struct sw_usb_mu509 *mu509)
{
    __s32 ret = 0;

    /* 3g_used */
	ret = script_parser_fetch("3g_para",
	                          "3g_used",
	                          (int *)&mu509->used,
	                          64);
	if(ret != 0){
		usb_3g_err("ERR: get 3g_used failed\n");
		//return -1;
	}

    /* 3g_usbc_num */
	ret = script_parser_fetch("3g_para",
	                          "3g_usbc_num",
	                          (int *)&mu509->usbc_no,
	                          64);
	if(ret != 0){
		usb_3g_err("ERR: get 3g_usbc_num failed\n");
		//return -1;
	}

    /* 3g_usbc_type */
	ret = script_parser_fetch("3g_para",
	                          "3g_usbc_type",
	                          (int *)&mu509->usbc_type,
	                          64);
	if(ret != 0){
		usb_3g_err("ERR: get 3g_usbc_type failed\n");
		//return -1;
	}

    /* 3g_uart_num */
	ret = script_parser_fetch("3g_para",
	                          "3g_uart_num",
	                          (int *)&mu509->uart_no,
	                          64);
	if(ret != 0){
		usb_3g_err("ERR: get 3g_uart_num failed\n");
		//return -1;
	}

    /* 3g_vbat_gpio */
	ret = script_parser_fetch("3g_para",
	                          "3g_vbat_gpio",
	                          (int *)&mu509->vbat_set,
	                          64);
	if(ret != 0){
		usb_3g_err("ERR: get 3g_vbat_gpio failed\n");
		//return -1;
	}

	if(mu509->vbat_set.port){
	    mu509->vbat_valid = 1;
	}else{
	    usb_3g_err("ERR: 3g_vbat_gpio is invalid\n");
	    mu509->vbat_valid = 0;
	}

    /* 3g_power_on_off_gpio */
	ret = script_parser_fetch("3g_para",
	                          "3g_power_on_off_gpio",
	                          (int *)&mu509->power_on_off_set,
	                          64);
	if(ret != 0){
		usb_3g_err("ERR: get 3g_power_on_off_gpio failed\n");
		//return -1;
	}

	if(mu509->power_on_off_set.port){
	    mu509->power_on_off_valid = 1;
	}else{
	    usb_3g_err("ERR: 3g_power_on_off_gpio is invalid\n");
	    mu509->power_on_off_valid  = 0;
	}

    /* 3g_reset_gpio */
	ret = script_parser_fetch("3g_para",
	                          "3g_reset_gpio",
	                          (int *)&mu509->reset_set,
	                          64);
	if(ret != 0){
		usb_3g_err("ERR: get 3g_reset_gpio failed\n");
		//return -1;
	}

	if(mu509->reset_set.port){
	    mu509->reset_valid = 1;
	}else{
	    usb_3g_err("ERR: 3g_reset_gpio is invalid\n");
	    mu509->reset_valid  = 0;
	}

    /* 3g_wakeup_in_gpio */
	ret = script_parser_fetch("3g_para",
	                          "3g_wakeup_in_gpio",
	                          (int *)&mu509->wakeup_in_set,
	                          64);
	if(ret != 0){
		usb_3g_err("ERR: get 3g_wakeup_in_gpio failed\n");
		//return -1;
	}

	if(mu509->wakeup_in_set.port){
	    mu509->wakeup_in_valid = 1;
	}else{
	    usb_3g_err("ERR: 3g_wakeup_in_gpio is invalid\n");
	    mu509->wakeup_in_valid  = 0;
	}

    return 0;
}

/*
*******************************************************************************
*                     mu509_pin_init
*
* Description:
*    void
*
* Parameters:
*    void
*
* Return value:
*    void
*
* note:
*    void
*
*******************************************************************************
*/
static s32 mu509_pin_init(struct sw_usb_mu509 *mu509)
{

    //---------------------------------
    //  3g vbat
    //---------------------------------
    if(mu509->vbat_valid){
		mu509->vbat_hd = gpio_request(&(mu509->vbat_set), 1);
		if(mu509->vbat_hd == 0){
			usb_3g_err("ERR: gpio_request vbat_set failed\n");
			return 0;
		}

		/* set config, ouput */
		gpio_set_one_pin_io_status(mu509->vbat_hd, 1, NULL);

		/* reserved is pull down */
		gpio_set_one_pin_pull(mu509->vbat_hd, 2, NULL);
    }else{
        mu509->vbat_hd = 0;
    }

    //---------------------------------
    //  3g power_on_off
    //---------------------------------
    if(mu509->power_on_off_valid){
		mu509->power_on_off_hd = gpio_request(&(mu509->power_on_off_set), 1);
		if(mu509->power_on_off_hd == 0){
			usb_3g_err("ERR: gpio_request power_on_off_set failed\n");
			return 0;
		}

		/* set config, ouput */
		gpio_set_one_pin_io_status(mu509->power_on_off_hd, 1, NULL);

		/* reserved is pull up */
		gpio_set_one_pin_pull(mu509->power_on_off_hd, 1, NULL);
    }else{
        mu509->power_on_off_hd = 0;
    }

    //---------------------------------
    //  3g reset
    //---------------------------------
    if(mu509->reset_valid){
		mu509->reset_hd = gpio_request(&(mu509->reset_set), 1);
		if(mu509->reset_hd == 0){
			usb_3g_err("ERR: gpio_request reset_set failed\n");
			return 0;
		}

		/* set config, ouput */
		gpio_set_one_pin_io_status(mu509->reset_hd, 1, NULL);

		/* reserved is pull down */
		gpio_set_one_pin_pull(mu509->reset_hd, 2, NULL);
    }else{
        mu509->reset_hd = 0;
    }

    //---------------------------------
    //  3g wakeup_out
    //---------------------------------
    if(mu509->wakeup_in_valid){
		mu509->wakeup_in_hd = gpio_request(&(mu509->wakeup_in_set), 1);
		if(mu509->wakeup_in_hd == 0){
			usb_3g_err("ERR: gpio_request wakeup_in_set failed\n");
			return 0;
		}

		/* set config, ouput */
		gpio_set_one_pin_io_status(mu509->wakeup_in_hd, 1, NULL);

		/* reserved is pull down */
		gpio_set_one_pin_pull(mu509->wakeup_in_hd, 2, NULL);
    }else{
        mu509->wakeup_in_hd = 0;
    }

    return 0;
}

/*
*******************************************************************************
*                     mu509_pin_exit
*
* Description:
*    void
*
* Parameters:
*    void
*
* Return value:
*    void
*
* note:
*    void
*
*******************************************************************************
*/
static s32 mu509_pin_exit(struct sw_usb_mu509 *mu509)
{

    if(mu509->vbat_hd){
        gpio_release(mu509->vbat_hd, 0);
        mu509->vbat_hd = 0;
    }

    if(mu509->power_on_off_hd){
        gpio_release(mu509->power_on_off_hd, 0);
        mu509->power_on_off_hd = 0;
    }

    if(mu509->reset_hd){
        gpio_release(mu509->reset_hd, 0);
        mu509->reset_hd = 0;
    }

    if(mu509->wakeup_in_hd){
        gpio_release(mu509->wakeup_in_hd, 0);
        mu509->wakeup_in_hd = 0;
    }

    return 0;
}

/*
*******************************************************************************
*                     mu509_wakeup_irq_enable
*
* Description:
*    void
*
* Parameters:
*    void
*
* Return value:
*    void
*
* note:
*    void
*
*******************************************************************************
*/
static void mu509_wakeup_irq_enable(void)
{
    __u32 gpio_base = SW_VA_PORTC_IO_BASE;
    __u32 reg_val = 0;
	unsigned long flags = 0;
    struct sw_usb_mu509 *mu509 = &g_usb_mu509;

    /* interrupt enable */
	spin_lock_irqsave(&mu509->lock, flags);
    reg_val = USBC_Readl(gpio_base + 0x210);
    reg_val |= (1 << 2);
    USBC_Writel(reg_val, (gpio_base + 0x210));
	spin_unlock_irqrestore(&mu509->lock, flags);

    return;
}

/*
*******************************************************************************
*                     mu509_wakeup_irq_disable
*
* Description:
*    void
*
* Parameters:
*    void
*
* Return value:
*    void
*
* note:
*    void
*
*******************************************************************************
*/
static void mu509_wakeup_irq_disable(void)
{
    __u32 gpio_base = SW_VA_PORTC_IO_BASE;
    __u32 reg_val = 0;
	unsigned long flags = 0;
    struct sw_usb_mu509 *mu509 = &g_usb_mu509;

    /* interrupt disable */
	spin_lock_irqsave(&mu509->lock, flags);
    reg_val = USBC_Readl(gpio_base + 0x210);
    reg_val &= ~(1 << 2);
    USBC_Writel(reg_val, (gpio_base + 0x210));
	spin_unlock_irqrestore(&mu509->lock, flags);

    return;
}

/*
*******************************************************************************
*                     mu509_wakeup_irq_clear
*
* Description:
*    Only used to provide driver mode change events
*
* Parameters:
*    void
*
* Return value:
*    void
*
* note:
*    void
*
*******************************************************************************
*/
static void mu509_wakeup_irq_clear(void)
{
    __u32 gpio_base = SW_VA_PORTC_IO_BASE;
    __u32 reg_val = 0;
	unsigned long flags = 0;
    struct sw_usb_mu509 *mu509 = &g_usb_mu509;

    /* clear interrupt pending */
	spin_lock_irqsave(&mu509->lock, flags);
    reg_val = USBC_Readl(gpio_base + 0x214);
    reg_val |= (1 << 2);
    USBC_Writel(reg_val, (gpio_base + 0x214));
	spin_unlock_irqrestore(&mu509->lock, flags);

    return;
}

/*
*******************************************************************************
*                     mu509_wakeup_irq_config
*
* Description:
*    Only used to provide driver mode change events
*
* Parameters:
*    void
*
* Return value:
*    void
*
* note:
*    void
*
*******************************************************************************
*/
static int mu509_wakeup_irq_config(void)
{
    __u32 gpio_base = SW_VA_PORTC_IO_BASE;
    __u32 reg_val = 0;
	unsigned long flags = 0;
    struct sw_usb_mu509 *mu509 = &g_usb_mu509;

	spin_lock_irqsave(&mu509->lock, flags);

    /* pull up, PH2 */
    reg_val = USBC_Readl(gpio_base + 0x118);
    reg_val &= ~(0x03 << 4);
    reg_val |= (0x01 << 4);      //EINT2
    USBC_Writel(reg_val, (gpio_base + 0x118));

    /* set port configure register, PH2 */
    reg_val = USBC_Readl(gpio_base + 0xFC);
    reg_val &= ~(0x07 << 8);
    reg_val |= (0x6 << 8);      //EINT2
    USBC_Writel(reg_val, (gpio_base + 0xFC));

    /* PIO interrupt configure register */
    reg_val = USBC_Readl(gpio_base + 0x200);
    reg_val &= ~(0x07 << 8);
    reg_val |= (0x1 << 8);
    USBC_Writel(reg_val, (gpio_base + 0x200));

	spin_unlock_irqrestore(&mu509->lock, flags);

    return 0;
}

/*
*******************************************************************************
*                     mu509_wakeup_irq_config_clear
*
* Description:
*    Only used to provide driver mode change events
*
* Parameters:
*    void
*
* Return value:
*    void
*
* note:
*    void
*
*******************************************************************************
*/
static int mu509_wakeup_irq_config_clear(void)
{
    __u32 gpio_base = SW_VA_PORTC_IO_BASE;
    __u32 reg_val = 0;
	unsigned long flags = 0;
    struct sw_usb_mu509 *mu509 = &g_usb_mu509;

	spin_lock_irqsave(&mu509->lock, flags);

    /* set port configure register, PH2 */
    reg_val = USBC_Readl(gpio_base + 0xFC);
    reg_val &= ~(0x07 << 8);
    USBC_Writel(reg_val, (gpio_base + 0xFC));

    /* PIO interrupt configure register */
    reg_val = USBC_Readl(gpio_base + 0x200);
    reg_val &= ~(0x07 << 8);
    USBC_Writel(reg_val, (gpio_base + 0x200));

	spin_unlock_irqrestore(&mu509->lock, flags);

    return 0;
}

extern void axp_pressshort_ex(void);

/*
*******************************************************************************
*                     mu509_wakeup_irq_work
*
* Description:
*    Only used to provide driver mode change events
*
* Parameters:
*    void
*
* Return value:
*    void
*
* note:
*    void
*
*******************************************************************************
*/
static void mu509_wakeup_irq_work(struct work_struct *data)
{
    usb_3g_dbg("---------mu509_wakeup_irq_work----------\n");

	axp_pressshort_ex();

	return;
}

/*
*******************************************************************************
*                     is_mu509_wakeup_irq_pending
*
* Description:
*    void
*
* Parameters:
*    void
*
* Return value:
*    void
*
* note:
*    void
*
*******************************************************************************
*/
static u32 is_mu509_wakeup_irq_pending(void)
{
    __u32 gpio_base = SW_VA_PORTC_IO_BASE;
    __u32 reg_val = 0;
    __u32 result = 0;

    /* interrupt pending */
    reg_val = USBC_Readl(gpio_base + 0x214);
    result = (reg_val & (1 << 2)) ? 1 : 0;

	return result;
}

/*
*******************************************************************************
*                     mu509_wakeup_irq_clear_pending
*
* Description:
*    void
*
* Parameters:
*    void
*
* Return value:
*    void
*
* note:
*    void
*
*******************************************************************************
*/
static void mu509_wakeup_irq_clear_pending(void)
{
    __u32 gpio_base = SW_VA_PORTC_IO_BASE;
    __u32 reg_val = 0;
	unsigned long flags = 0;
    struct sw_usb_mu509 *mu509 = &g_usb_mu509;

    /* clear interrupt pending */
    spin_lock_irqsave(&mu509->lock, flags);
    reg_val = USBC_Readl(gpio_base + 0x214);
    reg_val |= (1 << 2);
    USBC_Writel(reg_val, (gpio_base + 0x214));
    spin_unlock_irqrestore(&mu509->lock, flags);

    return ;
}

/*
*******************************************************************************
*                     is_mu509_wakeup_irq_enable
*
* Description:
*    void
*
* Parameters:
*    void
*
* Return value:
*    void
*
* note:
*    void
*
*******************************************************************************
*/
static u32 is_mu509_wakeup_irq_enable(void)
{
    __u32 gpio_base = SW_VA_PORTC_IO_BASE;
    __u32 reg_val = 0;
    __u32 result = 0;

    /* interrupt pending */
    reg_val = USBC_Readl(gpio_base + 0x210);
    result = (reg_val & (1 << 2)) ? 1 : 0;

    return result;
}

/*
*******************************************************************************
*                     mu509_wakeup_irq_interrupt
*
* Description:
*    void
*
* Parameters:
*    void
*
* Return value:
*    void
*
* note:
*    void
*
*******************************************************************************
*/
static irqreturn_t mu509_wakeup_irq_interrupt(int irq, void *__hci)
{
    __u32 result = 0;

    if(!is_mu509_wakeup_irq_pending()){
        return IRQ_NONE;
    }

	if(is_mu509_wakeup_irq_enable()){
	    result = 1;
	}

    mu509_wakeup_irq_disable();
    mu509_wakeup_irq_config_clear();
    mu509_wakeup_irq_clear_pending();

    if(result){
        schedule_work(&g_usb_mu509.irq_work);
    }

	return IRQ_HANDLED;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void mu509_early_suspend(struct early_suspend *h)
{
    mu509_wakeup_irq_config();
    mu509_wakeup_irq_clear();
    mu509_wakeup_irq_enable();
}

static void mu509_early_resume(struct early_suspend *h)
{
    mu509_wakeup_irq_disable();
    mu509_wakeup_irq_config_clear();
    mu509_wakeup_irq_clear();
}
#endif

/*
*******************************************************************************
*                     mu509_wakeup_irq_init
*
* Description:
*    void
*
* Parameters:
*    void
*
* Return value:
*    void
*
* note:
*    void
*
*******************************************************************************
*/
int mu509_wakeup_irq_init(void)
{
	struct sw_usb_mu509 *mu509 = &g_usb_mu509;
    int ret = 0;
    int nIrq = SW_INT_IRQNO_PIO;

    ret = request_irq(nIrq, mu509_wakeup_irq_interrupt, IRQF_TRIGGER_FALLING | IRQF_SHARED, "MU509", mu509);
    if(ret != 0){
        usb_3g_err("request_irq failed, ret=%d\n", ret);
        goto failed;
    }

	/* Init IRQ workqueue before request_irq */
	INIT_WORK(&mu509->irq_work, mu509_wakeup_irq_work);

    mu509_wakeup_irq_config();

#ifdef CONFIG_HAS_EARLYSUSPEND
    mu509->early_suspend.suspend = mu509_early_suspend;
    mu509->early_suspend.resume = mu509_early_resume;
	register_early_suspend(&mu509->early_suspend);
#endif

    //enable_irq(nIrq);

    return 0;

failed:
    return -1;
}
EXPORT_SYMBOL(mu509_wakeup_irq_init);

/*
*******************************************************************************
*                     mu509_wakeup_irq_exit
*
* Description:
*    void
*
* Parameters:
*    void
*
* Return value:
*    void
*
* note:
*    void
*
*******************************************************************************
*/
int mu509_wakeup_irq_exit(void)
{
	struct sw_usb_mu509 *mu509 = &g_usb_mu509;

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&mu509->early_suspend);
#endif

	mu509_wakeup_irq_disable();

    mu509_wakeup_irq_config_clear();

    mu509_wakeup_irq_clear();

    //free_irq(SW_INT_IRQNO_PIO, mu509_wakeup_irq_interrupt);

    return 0;
}
EXPORT_SYMBOL(mu509_wakeup_irq_exit);

/*
*******************************************************************************
*                     mu509_vbat
*
* Description:
*    void
*
* Parameters:
*    void
*
* Return value:
*    void
*
* note:
*
*******************************************************************************
*/
void mu509_vbat(struct sw_usb_mu509 *mu509, u32 on)
{
    if(!mu509->vbat_hd){
        //usb_3g_err("err: drv_vbus_ext_Handle == NULL\n");
        return;
    }

    if(on){
        usb_3g_dbg("Set USB MU509 vBat on\n");
    }else{
        usb_3g_dbg("Set USB MU509 vBat off\n");
    }

    gpio_write_one_pin_value(mu509->vbat_hd, on, NULL);

    return;
}
EXPORT_SYMBOL(mu509_vbat);

/*
*******************************************************************************
*                     mu509_reset
*
* Description:
*    void
*
* Parameters:
*    void
*
* Return value:
*    void
*
* note:
*    void
*
*******************************************************************************
*/
void mu509_reset(u32 usbc_no, u32 time)
{
    struct sw_usb_mu509 *mu509 = &g_usb_mu509;

    if(!mu509->reset_hd){
        //usb_3g_err("err: drv_vbus_ext_Handle == NULL\n");
        return;
    }

    gpio_write_one_pin_value(mu509->reset_hd, 1, NULL);
    mdelay(time);
    gpio_write_one_pin_value(mu509->reset_hd, 0, NULL);

    return;
}
EXPORT_SYMBOL(mu509_reset);

/*
*******************************************************************************
*                     mu509_wakeup_sleep
*
* Description:
*    void
*
* Parameters:
*    void
*
* Return value:
*    void
*
* note:
*    void
*
*******************************************************************************
*/
void mu509_wakeup_sleep(u32 usbc_no, u32 sleep)
{
    struct sw_usb_mu509 *mu509 = &g_usb_mu509;

    if(!mu509->wakeup_in_hd){
        //usb_3g_err("err: drv_vbus_ext_Handle == NULL\n");
        return;
    }

    if(sleep){
        usb_3g_dbg("sleep MU509\n");
    }else{
        usb_3g_dbg("wakeup MU509\n");
    }

    gpio_write_one_pin_value(mu509->wakeup_in_hd, sleep, NULL);

    return;
}
EXPORT_SYMBOL(mu509_wakeup_sleep);

/*
*******************************************************************************
*                     mu509_power_on_off
*
* Description:
*    void
*
* Parameters:
*    void
*
* Return value:
*    void
*
* note:
*    void
*
*******************************************************************************
*/
static void mu509_power_on_off(struct sw_usb_mu509 *mu509, u32 is_on)
{
    u32 on_off = 0;

    usb_3g_dbg("Set MU509 Power %s\n", (is_on ? "ON" : "OFF"));

    /* set power */
    if(mu509->power_on_off_set.data == 0){
        on_off = is_on ? 1 : 0;
    }else{
        on_off = is_on ? 0 : 1;
    }

    gpio_write_one_pin_value(mu509->power_on_off_hd, on_off, NULL);

    return;
}

/*
*******************************************************************************
*                     mu509_power
*
* Description:
*    void
*
* Parameters:
*    void
*
* Return value:
*    void
*
* note:
*
* 1、vbat通过GPIO操作时，vbat关掉后，整个3G模组都掉电了，因此 power_on_off 可以省去
*    power on:
*		1、vbat pin pull up
*		2、delay 4000ms
*		3、power_on_off pin pull down
*		4、delay 700ms
*		5、power_on_off pin pull up
*
*	  power off:
*		1、vbat pin pull down
*
*
*******************************************************************************
*/
void mu509_power(u32 usbc_no, u32 on)
{
    struct sw_usb_mu509 *mu509 = &g_usb_mu509;

    usb_3g_dbg("mu509 power %s with gpio\n", (on ? "ON" : "OFF"));

    if(on){
		mu509_vbat(mu509, 1);
		mdelay(4000);
        mu509_power_on_off(mu509, 0);
        mdelay(700);
        mu509_power_on_off(mu509, 1);
    }else{
		mu509_vbat(mu509, 0);
    }

    return;
}
EXPORT_SYMBOL(mu509_power);

/*
*******************************************************************************
*                     is_suspport_mu509
*
* Description:
*    void
*
* Parameters:
*    usbc_no   : input. 控制器编号, usb0,usb1,usb2
*    usbc_type : input. 控制器类型, 0: unkown, 1: ehci, 2: ohci
*
* Return value:
*    void
*
* note:
*    void
*
*******************************************************************************
*/
u32 is_suspport_mu509(u32 usbc_no, u32 usbc_type)
{
    if(!g_usb_mu509.used){
        //usb_3g_err("err: not support, (%d, %d)\n", g_usb_mu509.usbc_no, usbc_no);
        return 0;
    }

    if(g_usb_mu509.usbc_no != usbc_no){
        //usb_3g_err("err: not support, (%d, %d)\n", g_usb_mu509.usbc_no, usbc_no);
        return 0;
    }

    if(g_usb_mu509.usbc_type != usbc_type){
        //usb_3g_err("err: not support, (%d, %d)\n", g_usb_mu509.usbc_type, usbc_type);
        return 0;
    }

    return 1;
}
EXPORT_SYMBOL(is_suspport_mu509);

/*
*******************************************************************************
*                     mu509_init
*
* Description:
*    void
*
* Parameters:
*    void
*
* Return value:
*    void
*
* note:
*    void
*
*******************************************************************************
*/
int mu509_init(void)
{
    int ret = 0;

    memset(&g_usb_mu509, 0, sizeof(struct sw_usb_mu509));
	spin_lock_init(&g_usb_mu509.lock);

    ret = mu509_get_config(&g_usb_mu509);
    if(ret != 0){
        usb_3g_err("err: usb_3g_get_config failed\n");
        goto failed0;
    }

    ret =  mu509_pin_init(&g_usb_mu509);
    if(ret != 0){
       usb_3g_err("err: mu509_pin_init failed\n");
       goto failed1;
    }

    return 0;

failed1:
failed0:

    return -1;
}
EXPORT_SYMBOL(mu509_init);

/*
*******************************************************************************
*                     mu509_exit
*
* Description:
*    void
*
* Parameters:
*    void
*
* Return value:
*    void
*
* note:
*    void
*
*******************************************************************************
*/
int mu509_exit(void)
{
    mu509_pin_exit(&g_usb_mu509);

    return 0;
}
EXPORT_SYMBOL(mu509_exit);




