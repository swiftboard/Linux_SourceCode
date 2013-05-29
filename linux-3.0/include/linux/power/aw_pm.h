/*
*********************************************************************************************************
*                                                    LINUX-KERNEL
*                                        ReuuiMlla Linux Platform Develop Kits
*                                                   Kernel Module
*
*                                    (c) Copyright 2006-2011, kevin.z China
*                                             All Rights Reserved
*
* File    : pm.h
* By      : kevin.z
* Version : v1.0
* Date    : 2011-5-27 14:08
* Descript: power manager
* Update  : date                auther      ver     notes
*********************************************************************************************************
*/
#ifndef __AW_PM_H__
#define __AW_PM_H__


/**max device number of pmu*/
#define PMU_MAX_DEVS        2
/**start address for function run in sram*/
#define SRAM_FUNC_START     SW_VA_SRAM_BASE

/**
*@name PMU command
*@{
*/
#define AW_PMU_SET          0x10
#define AW_PMU_VALID        0x20
/**
*@}
*/

/*
* define event source for wakeup system when suspended
*/
#define SUSPEND_WAKEUP_SRC_EXINT    (1<<0)  /* external interrupt, pmu event for ex.    */
#define SUSPEND_WAKEUP_SRC_USB      (1<<1)  /* usb connection event */
#define SUSPEND_WAKEUP_SRC_KEY      (1<<2)  /* key event    */
#define SUSPEND_WAKEUP_SRC_IR       (1<<3)  /* ir event */
#define SUSPEND_WAKEUP_SRC_ALARM    (1<<4)  /* alarm event  */
#define SUSPEND_WAKEUP_SRC_TIMEOFF  (1<<5)  /* set time to power off event  */
#define SUSPEND_WAKEUP_SRC_PIO      (1<<6)  /* gpio event  */


/**
*@brief struct of pmu device arg
*/
struct aw_pmu_arg{
    unsigned int  twi_port;     /**<twi port for pmu chip   */
    unsigned char dev_addr;     /**<address of pmu device   */
};


/**
*@brief struct of standby
*/
struct aw_standby_para{
    unsigned int event;     /**<event type for system wakeup    */
	unsigned int axp_event;     /**<axp event type for system wakeup    */
    signed int   time_off;  /**<time to power off from now, based on second */
};


/**
*@brief struct of power management info
*/
struct aw_pm_info{
    struct aw_standby_para  standby_para;   /* standby parameter            */
    struct aw_pmu_arg       pmu_arg;        /**<args used by main function  */
};


#define AXP_WAKEUP_KEY          (1<<0)
#define AXP_WAKEUP_LOWBATT      (1<<1)
#define AXP_WAKEUP_USB          (1<<2)
#define AXP_WAKEUP_AC           (1<<3)
#define AXP_WAKEUP_ASCEND       (1<<4)
#define AXP_WAKEUP_DESCEND      (1<<5)
#define AXP_WAKEUP_SHORT_KEY    (1<<6)
#define AXP_WAKEUP_LONG_KEY     (1<<7)
 
#define AXP_MEM_WAKEUP              (AXP_WAKEUP_LOWBATT | AXP_WAKEUP_USB | AXP_WAKEUP_AC | AXP_WAKEUP_DESCEND | AXP_WAKEUP_ASCEND)
#define AXP_BOOTFAST_WAKEUP         (AXP_WAKEUP_LOWBATT | AXP_WAKEUP_LONG_KEY)


#endif /* __AW_PM_H__ */

