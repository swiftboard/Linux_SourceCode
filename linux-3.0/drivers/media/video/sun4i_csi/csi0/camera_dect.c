/*
 * Sun4i Camera Interface  driver
 * Author: raymonxiu
 */
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/videodev2.h>
#include <linux/clk.h>
#include <linux/delay.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20)
#include <linux/freezer.h>
#endif

#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <mach/sys_config.h>
#include <mach/clock.h>
#include <mach/irqs.h>
#include <linux/regulator/consumer.h>

typedef struct _camera_list_
{
	char *cam_name;
	int i2c_addr;
	int power_en_pol;
	int reset_pol;	
	int stby_pol;
	int (*sensor_detect)(int idx);
}s_camera_list;

static int gt2005_detect(int i2c_addr);
static int gc2015_detect(int i2c_addr);
static int gc0308_detect(int i2c_addr);
static int gc0329_detect(int i2c_addr);
static int hi253_detect(int i2c_addr);
static int mt9d112_detect(int i2c_addr);
static int mt9m112_detect(int i2c_addr);

s_camera_list cam_list [] = {
	{"gc0308", 0x21, 1, 0 ,1, gc0308_detect},
	//{"gc0329", 0x31, 1, 0 ,1, gc0329_detect},
	//{"gc2015", 0x30, 1, 0 ,1, gc2015_detect},
	{"mt9d112", 0x3c, 1, 0 ,1, mt9d112_detect},
	//{"mt9m112", 0x48, 1, 0 ,1, mt9m112_detect},
	
	
};

s_camera_list cam_b_list [] = {
    /* name   addr pwr reset stby detect*/
//	{"gt2005", 0x3C, 1, 0 ,0, gt2005_detect},
	//{"gc2015", 0x30, 1, 0 ,1, gc2015_detect},
	{"hi253",  0x20, 1, 0 ,1, hi253_detect},
};

int g_i2c_port;
int g_i2c_addr;
struct i2c_adapter *g_adapter;
struct clk *g_csi_clk;
struct regulator *g_iovdd;
struct regulator *g_avdd;

static int gpio_power_en_hdle;
static int gpio_reset_hdle;
static int gpio_stby_hdle;

#define CAM_STBY_ON		0
#define CAM_STBY_OFF	1
#define CAM_PWR_ON		2
#define CAM_PWR_OFF		3

char g_iovdd_str[32];
char g_avdd_str[32];

static int cam_i2c_read(int dev_addr, int reg_addr_len, unsigned char *buf, int len)
{
	int ret=-1;
	struct i2c_msg msgs[2];
	
    //发送写地址
    msgs[0].flags=0; //写消息
    msgs[0].addr=dev_addr;
    msgs[0].len=reg_addr_len;
    msgs[0].buf=&buf[0];
    //接收数据
    msgs[1].flags=I2C_M_RD;//读消息
    msgs[1].addr=dev_addr;
    msgs[1].len=len;
    msgs[1].buf=&buf[reg_addr_len];

    ret=i2c_transfer(g_adapter,msgs, 2);	
    
	return ret;
}

static int cam_i2c_write(int dev_addr, int reg_addr_len, unsigned char *buf, int len)
{
    int ret = -1;
    struct i2c_msg msg;
    
    //发送设备地址
    msg.flags = 0;//写消息
    msg.addr  = dev_addr;
    msg.len   = reg_addr_len+len;
    msg.buf   = buf;        

    ret=i2c_transfer(g_adapter,&msg, 1);

    return ret;
}

static int cam_fetch_config(void)
{
	int ret = 0;
	/* fetch reset/power/standby io issue */
	gpio_power_en_hdle = gpio_request_ex("csi0_para", "csi_power_en");
	if(!gpio_power_en_hdle) {
		pr_warning("touch panel csi_power_en request gpio fail!\n");
	    gpio_power_en_hdle = 0;
	} 
	gpio_reset_hdle = gpio_request_ex("csi0_para", "csi_reset");
	if(!gpio_reset_hdle) {
		pr_warning("touch panel csi_reset request gpio fail!\n");
	    gpio_reset_hdle = 0;
	} 		 
	gpio_stby_hdle = gpio_request_ex("csi0_para", "csi_stby");
	if(!gpio_stby_hdle) {
		pr_warning("touch panel csi_stby request gpio fail!\n");
	    gpio_stby_hdle = 0;
	} 
	
	/* fetch power issue*/
	ret = script_parser_fetch("csi0_para","csi_iovdd", (int *)&g_iovdd_str , 32*sizeof(char));
	if (ret) {
		pr_warning("fetch csi_iovdd from sys_config failed\n");
	} 
	
	ret = script_parser_fetch("csi0_para","csi_avdd", (int *)&g_avdd_str , 32*sizeof(char));
	if (ret) {
		pr_warning("fetch csi_avdd from sys_config failed\n");
	}	
	
	/* fetch i2c and module name*/
	ret = script_parser_fetch("csi0_para","csi_twi_id", &g_i2c_port , sizeof(int));
	if (ret) {
		pr_warning("fetch csi_twi_id from sys_config failed\n");
	}
			
	return ret;
}

static int sensor_power(int on, int power_en_pol, int rest_pol, int stby_pol)
{
	int power_en_on, power_en_off, rest_on, rest_off, stby_on, stby_off;
	
	power_en_on = power_en_pol ? 1 : 0;
	power_en_off = power_en_pol ? 0 : 1;
			
	rest_on = rest_pol ? 1 : 0;
	rest_off = rest_pol ? 0 : 1;
	
	stby_on = stby_pol ? 1 : 0;
	stby_off = stby_pol ? 0 : 1;
	
	//make sure that no device can access i2c bus during sensor initial or power down
	//when using i2c_lock_adpater function, the following codes must not access i2c bus before calling i2c_unlock_adapter
	i2c_lock_adapter(g_adapter);
	
	//insure that clk_disable() and clk_enable() are called in pair 
	switch(on)
	{
		case CAM_STBY_ON:
			//printk("CAM_STBY_ON\r\n");
			//reset off io
			gpio_write_one_pin_value(gpio_reset_hdle, rest_off, "csi_reset");
			mdelay(10);
			//standby on io
			gpio_write_one_pin_value(gpio_stby_hdle, stby_on, "csi_stby");
			mdelay(10);
			gpio_write_one_pin_value(gpio_stby_hdle, stby_off, "csi_stby");
			mdelay(10);
			gpio_write_one_pin_value(gpio_stby_hdle, stby_on, "csi_stby");
			mdelay(10);
			//inactive mclk after stadby in
			clk_disable(g_csi_clk);
			//reset on io
			gpio_write_one_pin_value(gpio_reset_hdle, rest_on, "csi_reset");
			mdelay(10);
			break;
		case CAM_STBY_OFF:
			//printk("CAM_STBY_OFF\r\n");
			//active mclk before stadby out
			clk_enable(g_csi_clk);
			mdelay(10);
			//standby off io
			gpio_write_one_pin_value(gpio_stby_hdle, stby_off, "csi_stby");
			mdelay(10);
			//reset off io
			gpio_write_one_pin_value(gpio_stby_hdle, rest_off, "csi_reset");
			mdelay(10);
			gpio_write_one_pin_value(gpio_stby_hdle, rest_on, "csi_reset");
			mdelay(30);
			gpio_write_one_pin_value(gpio_reset_hdle, rest_off,  "csi_reset");
			mdelay(10);
			break;
		case CAM_PWR_ON:
			//printk("CAM_PWR_ON\r\n");
			//power on reset
			gpio_write_one_pin_value(gpio_stby_hdle, stby_on,  "csi_stby");
			//reset on io
			gpio_write_one_pin_value(gpio_reset_hdle, rest_on,  "csi_reset");
			mdelay(1);
			//active mclk before power on
			clk_enable(g_csi_clk);
			mdelay(10);
			//power supply
			gpio_write_one_pin_value(gpio_power_en_hdle, power_en_on,  "csi_power_en");
			mdelay(10);
			
			regulator_enable(g_avdd);
			mdelay(10);
			regulator_enable(g_iovdd);
			mdelay(10);

			//standby off io
			gpio_write_one_pin_value(gpio_stby_hdle, stby_off,  "csi_stby");
			mdelay(10);
			//reset after power on
			gpio_write_one_pin_value(gpio_reset_hdle, rest_off,  "csi_reset");
			mdelay(10);
			gpio_write_one_pin_value(gpio_reset_hdle, rest_on,  "csi_reset");
			mdelay(30);
			gpio_write_one_pin_value(gpio_reset_hdle, rest_off,  "csi_reset");
			mdelay(10);
			break;
		case CAM_PWR_OFF:
			//printk("CAM_PWR_OFF\r\n");
			//standby and reset io
			gpio_write_one_pin_value(gpio_stby_hdle, stby_on,  "csi_stby");
			mdelay(10);
			gpio_write_one_pin_value(gpio_reset_hdle, rest_on,  "csi_reset");
			mdelay(10);
			//power supply off
			regulator_disable(g_iovdd);
			mdelay(10);
		
			regulator_disable(g_avdd);
			mdelay(10);
			
			gpio_write_one_pin_value(gpio_power_en_hdle, power_en_off,  "csi_power_en");
			mdelay(10);
			//inactive mclk after power off
			clk_disable(g_csi_clk);
			break;
		default:
			return -EINVAL;
	}		

	//remember to unlock i2c adapter, so the device can access the i2c bus again
	i2c_unlock_adapter(g_adapter);	
	return 0;
}

static int gt2005_detect(int i2c_addr)
{
	int ret = -1;
	unsigned char buf[10];
	
	buf[0] = 0x00;
	buf[1] = 0x00;
	ret = cam_i2c_read(i2c_addr, 2, buf, 1);
	if (ret < 0) {
		printk("cam_i2c_read err at gt2005_detect!\n");
		return ret;
	}

	if(buf[2] != 0x51)
		return -ENODEV;
	
	return 0;
		
}



static int mt9d112_detect(int i2c_addr)
{
	int ret = -1;
	unsigned char buf[10];
	
	buf[0] = 0x00;
	buf[1] = 0x00;
	printk("mt9d112i2c_addr=0x%x !\n",i2c_addr);
	ret = cam_i2c_read(i2c_addr, 2, buf, 1);
	if (ret < 0) {
		printk("cam_i2c_read err at mt9d112_detect!\n");
		return ret;
	}
	printk("mt9d112_detect buf[2]=0x%x !\n",buf[2]);
	if(buf[2] != 0x15)
		return -ENODEV;
	
	return 0;	
	
	
	
	
	

}



static int mt9m112_detect(int i2c_addr)
{
	int ret = -1;
	unsigned char buf[10];
	
	buf[0] = 0xfe;
	buf[1] = 0x00; //PAGE 0x00
	ret = cam_i2c_write(i2c_addr, 1, buf, 1);
	if (ret < 0) {
		printk("cam_i2c_write err at gc0308_detect!\n");
		return ret;
	}
	
	buf[0] = 0x00;
	ret = cam_i2c_read(i2c_addr, 1, buf, 1);
	if (ret < 0) {
		printk("cam_i2c_read err at mt9m112_detect!\n");
		return ret;
	}

	if(buf[1] != 0x14)
		return -ENODEV;
	
	return 0;
}









static int hi253_detect(int i2c_addr)
{
	int ret;
	unsigned char buf[10];
	
	buf[0] = 0x03;
	buf[1] = 0x00; //PAGE 0x00
	ret = cam_i2c_write(i2c_addr, 1, buf, 1);
	if (ret < 0) {
		printk("cam_i2c_write err at hi253_detect!\n");
		return ret;
	}
	
	buf[0] = 0x04;
	ret = cam_i2c_read(i2c_addr, 1, buf, 1);
	if (ret < 0) {
		printk("cam_i2c_read err at hi253_detect!\n");
		return ret;
	}

	if(buf[1] != 0x92)
		return -ENODEV;
	
	return 0;	
}

static int gc2015_detect(int i2c_addr)
{
	int ret = -1;
	unsigned char buf[10];
	
	buf[0] = 0xfe;
	buf[1] = 0x00; //PAGE 0x00
	ret = cam_i2c_write(i2c_addr, 1, buf, 1);
	if (ret < 0) {
		printk("cam_i2c_write err at gc2015_detect!\n");
		return ret;
	}
	
	buf[0] = 0x00;
	ret = cam_i2c_read(i2c_addr, 1, buf, 1);
	if (ret < 0) {
		printk("cam_i2c_read err at gc2015_detect!\n");
		return ret;
	}
	
	if(buf[1] != 0x20)
		return -ENODEV;
	
	buf[0] = 0x01;
	ret = cam_i2c_read(i2c_addr, 1, buf, 1);
	if (ret < 0) {
		printk("cam_i2c_read err at gc2015_detect!\n");
		return ret;
	}
	
	if(buf[1] != 0x05)
		return -ENODEV;
	
	return 0;
}

static int gc0308_detect(int i2c_addr)
{
	int ret = -1;
	unsigned char buf[10];
	
	buf[0] = 0xfe;
	buf[1] = 0x00; //PAGE 0x00
	ret = cam_i2c_write(i2c_addr, 1, buf, 1);
	if (ret < 0) {
		printk("cam_i2c_write err at gc0308_detect!\n");
		return ret;
	}
	
	buf[0] = 0x00;
	ret = cam_i2c_read(i2c_addr, 1, buf, 1);
	if (ret < 0) {
		printk("cam_i2c_read err at gc0308_detect!\n");
		return ret;
	}

	if(buf[1] != 0x9b)
		return -ENODEV;
	
	return 0;
}


static int gc0329_detect(int i2c_addr)
{
	int ret = -1;
	unsigned char buf[10];
	
	buf[0] = 0xfc;
	buf[1] = 0x16; //PAGE 0x00
	ret = cam_i2c_write(i2c_addr, 1, buf, 1);
	if (ret < 0) {
		printk("cam_i2c_write err at gc0329_detect!\n");
		return ret;
	}
	
	buf[0] = 0x00;
	ret = cam_i2c_read(i2c_addr, 1, buf, 1);
	if (ret < 0) {
		printk("cam_i2c_read err at gc0329_detect!\n");
		return ret;
	}

	if(buf[1] != 0xc0)
		return -ENODEV;
	
	return 0;
}






int camera_detect(char *cam_name, int *i2c_addr, int isBack)
{
	int i;
	int ret = -1;
	s_camera_list *pCam_list;
	int arry_size;
	
	cam_fetch_config();

 	g_adapter = i2c_get_adapter(g_i2c_port);
 	
 	if (g_adapter == NULL)
 	{
		printk(KERN_ERR "[camera_detect]: can't get i2c adapter\n");
		return ret;
 	}
	
	g_csi_clk = clk_get(NULL,"csi0");
	
	g_iovdd = regulator_get(NULL, g_iovdd_str);
	g_avdd = regulator_get(NULL, g_avdd_str);
		
	if (isBack == 0)
	{
		pCam_list = cam_list;
		arry_size = sizeof(cam_list)/sizeof(s_camera_list);
	}
	else
	{
		pCam_list = cam_b_list;
		arry_size = sizeof(cam_b_list)/sizeof(s_camera_list);
	}
	
	for ( i = 0; i < arry_size; i++)
	{
		sensor_power(CAM_PWR_ON, pCam_list[i].power_en_pol, pCam_list[i].reset_pol, pCam_list[i].stby_pol);
		ret = pCam_list[i].sensor_detect(pCam_list[i].i2c_addr);
		sensor_power(CAM_PWR_OFF, pCam_list[i].power_en_pol, pCam_list[i].reset_pol, pCam_list[i].stby_pol);
		if ( ret == 0)
		{
			if (isBack == 0)
				printk("[camera_detect] camera front is %s!\n",pCam_list[i].cam_name);
			else
				printk("[camera_detect] camera back is %s!\n",pCam_list[i].cam_name);
			strcpy(cam_name, pCam_list[i].cam_name);
			*i2c_addr = pCam_list[i].i2c_addr<<1;
			break;
		}
	}
	
	regulator_put(g_iovdd);		
	regulator_put(g_avdd);
	clk_put(g_csi_clk);
	i2c_put_adapter(g_adapter);
	
	return 0;
}