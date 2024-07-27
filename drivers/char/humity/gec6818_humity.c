/*---------------------------------------
*功能描述:  温湿度驱动程序 
*创建者：   粤嵌技术部
*创建时间： 2016,06,01
---------------------------------------
*修改日志：
*修改内容：
*修改人：
*修改时间：
----------------------------------------*/

/*************************************************
*头文件
*************************************************/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <asm/mach-types.h>
#include <linux/gpio.h>
#include <asm/gpio.h>
#include <asm/delay.h>
#include <linux/clk.h>
#include <mach/gpio.h>
#include <mach/soc.h>
#include <mach/platform.h>

#include <linux/slab.h>
#include <linux/input.h>
#include <linux/errno.h>
#include <linux/serio.h>
#include <linux/wait.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <mach/gpio.h>
#include <linux/gpio.h>




#define DEVICE_NAME "humidity"    //设备名字

unsigned long receive_value;
unsigned long receive_jy;

/*
 * HUMITY -> GPDC(29)
 */

#define	CFG_IO_X6818_Humity				(PAD_GPIO_C + 29)
/*************************************************
*管教设置为输入
*************************************************/
int data_in(void)
{
	gpio_direction_input(CFG_IO_X6818_Humity);
	return gpio_get_value(CFG_IO_X6818_Humity);
}

/*************************************************
*管教设置为输出
*************************************************/
void data_out(int data)
{
	gpio_direction_output(CFG_IO_X6818_Humity, data);
}

/*************************************************
*温湿度数据读取
*主机拉低，延时20ms。主机拉高，
*主机拉高，延时40ms。
*设置为输入端口，等待数据位拉低
*设置为输入端口，等待数据位拉高
*数据位
*************************************************/
void humidity_read_data(void)
{
	unsigned int u32i = 0;
	unsigned int flag = 0;
	receive_value = 0;
	receive_jy = 0;
	//printk("humidity_read_data\n");
    	data_out(0);
	mdelay(40);
	data_out(1);
	//udelay(40);  
	data_in();
	while(gpio_get_value(CFG_IO_X6818_Humity) && (flag <100))   //waiting for  dht11's response
	{
		flag++;
	}
	flag = 0;
	//udelay(80);	//dht11 response last about 80us
	while((!gpio_get_value(CFG_IO_X6818_Humity)) && (flag <100)) //waiting for dht11'response  end
	{
		flag++;
	}
	flag = 0;
	//udelay(80);  //dht11 pull up about 80us 
	while(gpio_get_value(CFG_IO_X6818_Humity)&& (flag <100))  //waiting for dht11 pull up end
	{
		flag++;
	}
	flag = 0;
	for (u32i=0x80000000; u32i>0; u32i>>=1)
	{			
		while((!gpio_get_value(CFG_IO_X6818_Humity)) && (flag <100)) //waiting for dht11'response  end
		{
			flag++;
		}
		flag = 0;
		udelay(50);
		if( data_in() == 1)
		{
			receive_value |= u32i;
			while(gpio_get_value(CFG_IO_X6818_Humity)&& (flag <100))  //waiting for dht11 pull up end
			{
				flag++;
			}
			flag = 0;
		}

		continue;				
	}

	#if 1
	for (u32i=0x80; u32i>0; u32i>>=1)
	{
		while((!gpio_get_value(CFG_IO_X6818_Humity)) && (flag <100)) //waiting for dht11'response  end
		{
			flag++;
		}
		flag = 0;
		udelay(50);
		if( data_in() == 1)
		{
			receive_jy |= u32i;
			while(gpio_get_value(CFG_IO_X6818_Humity)&& (flag <100))  //waiting for dht11 pull up end
			{
				flag++;
			}
			flag = 0;
		}

		continue;				
	}
	#endif
	data_out(1);

		
}

/*************************************************
*温湿度数据读取
*************************************************/
static ssize_t gec6818_humidiy_read(struct file *file, char __user *buf, size_t size, loff_t *off)
{
	unsigned char tempz = 0;
	unsigned char tempx = 0;
	unsigned char humidityz = 0;
	unsigned char humidityx = 0;
        unsigned char  ecc,jy;
	int ret = 0;

        humidity_read_data();
 
	printk("=============receive_value= %lx  receive_jy= %lx \n", receive_value, receive_jy);
        humidityz = (receive_value & 0xff000000)>>24;
	humidityx = (receive_value & 0x00ff0000)>>16;
	tempz = (receive_value & 0x0000ff00)>>8;
	tempx = (receive_value & 0x000000ff);
        jy = receive_jy & 0xff;
        
        ecc = humidityz + humidityx + tempz + tempx;
        printk("=============ecc=%x  jy=%x \n",ecc,jy);
        if(ecc != jy)
            return -EAGAIN;
	ret = copy_to_user(buf,&receive_value,sizeof (receive_value));
	if (ret != 0)
		return -EAGAIN;
	return 0;
}

static int gec6818_humidiy_open(struct inode *inode, struct file *file)
{
	printk("open in kernel\n");
	return 0;
}

/*static void gec6818_humidiy_release(struct inode *inode, struct file *file)
{
	//printk("");
}
*/
/*************************************************
*文件操作集
*************************************************/
static struct file_operations gec6818_humidity_dev_fops = {
	.owner			= THIS_MODULE,
	.open = gec6818_humidiy_open,
	.read = gec6818_humidiy_read,
	//.release = gec6818_humidiy_release
};

/*************************************************
*杂项设备
*************************************************/
static struct miscdevice gec6818_humidity_dev = {
	.minor			= MISC_DYNAMIC_MINOR,
	.name			= DEVICE_NAME,
	.fops			= &gec6818_humidity_dev_fops,
};

/********************************************************************
*驱动的初始化函数--->从内核中申请资源（内核、中断、设备号、锁....）
********************************************************************/
static int __init gec6818_humidity_dev_init(void) {
	int ret;
	ret = gpio_request(CFG_IO_X6818_Humity, "humidity");
		if (ret) {
			printk("%s: request GPIO %d for humidity failed, ret = %d\n", DEVICE_NAME,
					CFG_IO_X6818_Humity, ret);
			return ret;
		}

		gpio_direction_output(CFG_IO_X6818_Humity, 1);
	
	

	ret = misc_register(&gec6818_humidity_dev);

	printk(DEVICE_NAME"\tinitialized\n");
	return ret;
}

/*****************************************************************
*驱动退出函数 --->将申请的资源还给内核
*****************************************************************/
static void __exit gec6818_humidity_dev_exit(void)
{
	gpio_free(CFG_IO_X6818_Humity);
	misc_deregister(&gec6818_humidity_dev);
}

module_init(gec6818_humidity_dev_init);             //驱动的入口函数会调用一个用户的初始化函数
module_exit(gec6818_humidity_dev_exit);             //驱动的出口函数会调用一个用户的退出函数

//驱动的描述信息： #modinfo  *.ko , 驱动的描述信息并不是必需的。
MODULE_AUTHOR("ZOROE@GEC");                         //驱动的作者
MODULE_DESCRIPTION("Step_Motor of driver");         //驱动的描述
MODULE_LICENSE("GPL");                              //遵循的协议


