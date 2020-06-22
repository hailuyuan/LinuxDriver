
#include <linux/module.h> /* 最基本的文件，支持动态添加和卸载模块 */
#include <linux/fs.h> /* 包含了文件操作相关struct的定义，例如大名鼎鼎的struct file_operations; 同时包含了定义struct inode结构体、MINOR、MAJOR的头文件 */
#include <linux/slab.h> /* 定义了kmalloc相关的函数，要用到kmalloc 时需要include此头文件 */
#include <linux/errno.h> /* 包含了对返回值的宏定义，这样用户程序可以用perror输出错误信息 */
#include <linux/types.h> /* 对一些特殊类型的定义，例如dev_t, off_t, pid_t */
#include <linux/cdev.h> /* cdev 结构及相关函数的定义 */
#include <linux/wait.h> /* 等代队列相关头文件，同时它包含了自旋锁的头文件 */
#include <linux/init.h> /* 初始化头文件 */
#include <linux/kernel.h> /* 驱动要写入内核，与内核相关的头文件 */
#include <linux/uaccess.h> /* 包含了copy_to_user、copy_from_user等内核访问用户进程内存地址的函数定义 */
#include <linux/device.h> /* 包含了device、class 等结构的定义 */
#include <linux/io.h> /* 包含了ioremap、iowrite等内核访问IO内存等函数的定义 */
#include <linux/ioctl.h> /* _IO(type, nr)等定义 */
#include <linux/semaphore.h> /* 使用信号量必须的头文件 */
#include <asm/delay.h> /* delay */

#define SPI_CONFIG_REG		0x00000000
#define INTR_STATUS_REG		0x00000004
#define INTRPT_EN_REG		0x00000008
#define INTRPT_DIS_REG		0x0000000C
#define INTRPT_MASK_REG		0x00000010
#define SPI_EN_REG			0x00000014
#define TX_DATA_REG			0x0000001C
#define RX_DATA_REG			0x00000020
#define RX_THRES_REG		0x0000002C

#define SPI_EN_VALUE		1
#define SPI_DIS_VALUE		0


#define MAN_START_MASK		(1 << 15)
#define AUTO_START_MASK		(~MAN_START_MASK)

#define MAN_CS_MASK			(1 <<14)
#define AUTO_CS_MASK		(~MAN_CS_MASK)

#define CS_SELECT_MASK		(~(15 << 10))
#define CS0_MASK			(0 << 10)
#define CS1_MASK			(13 << 10)
#define CS2_MASK			(11 << 10)
#define DE_ASSERT_CS		(15 << 10)
#define DECODE_MASK			(1 << 9)
#define NO_DECODE_MASK		(~(1 << 9))

#define DIV_MASK			(~(7 << 3))
#define DIV4_MSAK			(1 << 3)
#define DIV8_MASK			(2 << 3)
#define DIV16_MASK			(3 << 3)
#define DIV32_MASK			(4 << 3)
#define DIV64_MASK			(5 << 3)
#define DIV128_MASK			(6 << 3)
#define DIV256_MASK			(7 << 3)

#define RX_FIFO_NOT_EMPTY_MASK	(1 << 4)

#define CPHA_INACTIVE_MASK	(1 << 2)
#define CPHA_ACTIVE_MASK	(~CPHA_INACTIVE_MASK)	
#define CPOL_HIGH_MASK		(1 << 1)
#define CPOL_LOW_MASK		(~CPOL_HIGH_MASK)
#define MASTER_MODE_MASK	(1 << 0)
#define SLAVE_MODE_MASK		(~MASTER_MODE_MASK)

#define DIS_INTR_VALUE		0x7F

struct spi_ioc_transfer {
	unsigned char *tx;
	unsigned char *rx;
	int length;
	int dev_cs; // 0,1,2
};

#define SPI_IOC_MAGIC			'k'
#define SPI_IOC_MODE 	_IOWR(SPI_IOC_MAGIC, 5, __u32)
#define SPI_MSGSIZE \
	((((sizeof (struct spi_ioc_transfer))) < (1 << _IOC_SIZEBITS)) \
		? ((sizeof (struct spi_ioc_transfer))) : 0)
#define SPI_IOC_MESSAGE _IOW(SPI_IOC_MAGIC, 0, char[SPI_MSGSIZE])

#define SPI_MODE_0			(0 << 1)
#define SPI_MODE_1			(1 << 1)
#define SPI_MODE_2			(2 << 1)
#define SPI_MODE_3			(3 << 1)
#define SPI_MODE_MASK		(~(3 << 1))

#define SPI_DIV4			(1 << 3)
#define SPI_DIV8			(2 << 3)
#define SPI_DIV16			(3 << 3)
#define SPI_DIV32			(4 << 3)
#define SPI_DIV64			(5 << 3)
#define SPI_DIV128			(6 << 3)
#define SPI_DIV256			(7 << 3)
#define SPI_DIV_MASK		(~(7 << 3))

#define DEVICE_NAME		"spi0"
#define FILE_NAME		"spidev0"
#define SPI0_ADDR		0xE0006000  /* spi总线控制器硬件起始地址 */
#define SPI0_MAP_SIZE	0xFC        /* spi总线控制器硬件地址范围 */

struct spi_dev_t {
	struct cdev cdev; /* spi字符设备结构体 */
	struct class *spi_class; /* 用于自动创建设备文件 */
	struct device *spi_device; /* 用于自动创建设备文件 */
	struct semaphore wr_lock; /* 读写控制锁 */
};
static struct spi_dev_t spi_dev; /* 设备相关结构体实例 */

static volatile void __iomem *base_addr = NULL; /* spi控制器寄存器地址 */

static inline u32 readregs(unsigned long addr)
{
	return readl(base_addr + addr);
}

static inline void writeregs(unsigned long addr, unsigned int data)
{
	writel(data, base_addr + addr);
}


/* RF打开函数 */
static int spi_open(struct inode *inode, struct file* filp)
{
	struct spi_dev_t *dev = container_of(inode->i_cdev, struct spi_dev_t, cdev); /* 获取设备结构体指针 */

	filp->private_data = dev; /* 将设备结构体指针赋给文件私有数据指针 */

	try_module_get(THIS_MODULE); /* 增加模块使用计数 */

	return 0; /* 打开成功 */
}

/* RF关闭函数 */
static int spi_release(struct inode *inode, struct file *filp)
{
	module_put(THIS_MODULE); /* 减少模块使用计数 */

	return 0; /* 关闭成功 */
}

static long spi_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{	
	struct spi_dev_t *dev = filp->private_data; /* 获取设备结构体指针 */
	struct spi_ioc_transfer *transfer = NULL;
	int ix = 0, rc = 0, timeout = 10000; 

	/* 获取读写控制锁 */
	if (down_interruptible(&dev->wr_lock))
		return -ERESTARTSYS;
	
	switch (cmd)
	{
	case SPI_IOC_MODE: // 模式设置
		writeregs(SPI_CONFIG_REG, (readregs(SPI_CONFIG_REG) & SPI_MODE_MASK) | (arg & ~SPI_MODE_MASK)); 
		if (arg & ~SPI_DIV_MASK) { // 分频设置
			writeregs(SPI_CONFIG_REG, (readregs(SPI_CONFIG_REG) & SPI_DIV_MASK) | (arg & ~SPI_DIV_MASK)); 
		}
		break;
	case SPI_IOC_MESSAGE: // 数据传输
		transfer = (struct spi_ioc_transfer *)arg;
		
		writeregs(RX_THRES_REG, transfer->length); // 发送和接收数据字节数
		
		switch (transfer->dev_cs)
		{
		case 0:
			writeregs(SPI_CONFIG_REG, (readregs(SPI_CONFIG_REG) & CS_SELECT_MASK) | CS0_MASK); // 选择从机0
			break;
		case 1:
			writeregs(SPI_CONFIG_REG, (readregs(SPI_CONFIG_REG) & CS_SELECT_MASK) | CS1_MASK); // 选择从机1
			break;
		case 2:
			writeregs(SPI_CONFIG_REG, (readregs(SPI_CONFIG_REG) & CS_SELECT_MASK) | CS2_MASK); // 选择从机2
			break;
		default:
			writeregs(SPI_CONFIG_REG, (readregs(SPI_CONFIG_REG) & CS_SELECT_MASK) | CS0_MASK); // 选择从机0
			break;
		}
		
		writeregs(SPI_EN_REG, SPI_EN_VALUE); // 使能spi控制器
		
		for (ix = 0; ix < transfer->length; ++ix) {
			writeregs(TX_DATA_REG, transfer->tx[ix]); // 发送数据
		}
			
		while (!(readregs(INTR_STATUS_REG) & RX_FIFO_NOT_EMPTY_MASK)) { // 等待数据传输完成
			timeout = timeout - 1; 
			if (timeout <= 0) {
				rc = ETXTBSY;
				break;
			}
			udelay(1); // 1 us
		}

		writeregs(SPI_EN_REG, SPI_DIS_VALUE); // 禁止spi控制器
		
		writeregs(SPI_CONFIG_REG, (readregs(SPI_CONFIG_REG) & CS_SELECT_MASK) | DE_ASSERT_CS); // 片选无效

		for (ix = 0; ix < transfer->length; ++ix) { // 读数据
			transfer->rx[ix] = readregs(RX_DATA_REG);
		}
		
		break;
	default:
		rc = -EINVAL; // 无效参数
		break;
	}

	up(&dev->wr_lock);	/* 释放读写控制锁 */

	return rc; // 返回
}

const struct file_operations fops = {
	.open = spi_open,
	.release = spi_release,
	.unlocked_ioctl = spi_ioctl,
};

/* RF设备驱动加载函数 */
static int __init spi_init(void)
{
	/* 初始化字符设备 */
	cdev_init(&spi_dev.cdev, &fops);
	/* alloc_chrdev_region申请一个动态主设备号，并申请一系列次设备号。baseminor为起始次设备号，count为次设备号的数量 */
	/* 动态申请设备号 */
	alloc_chrdev_region(&spi_dev.cdev.dev, 0, 1, DEVICE_NAME);
	/* 注册字符设备 */
	cdev_add(&spi_dev.cdev, spi_dev.cdev.dev, 1);
	/* 自动创建设备节点 */
	spi_dev.spi_class = class_create(THIS_MODULE, DEVICE_NAME);
	spi_dev.spi_device = device_create(spi_dev.spi_class, NULL, spi_dev.cdev.dev, NULL, FILE_NAME);

	sema_init(&spi_dev.wr_lock, 1); /* 初始化为互斥信号量-读写控制所 */

	/* Address mapping using ioremap*/
	base_addr = (volatile unsigned long *)ioremap(SPI0_ADDR, SPI0_MAP_SIZE);

	writeregs(SPI_CONFIG_REG, (readregs(SPI_CONFIG_REG) & CS_SELECT_MASK) | DE_ASSERT_CS); // 禁止所有片选

	writeregs(SPI_CONFIG_REG, readregs(SPI_CONFIG_REG) & NO_DECODE_MASK); // 没有外部译码器

	writeregs(SPI_CONFIG_REG, (readregs(SPI_CONFIG_REG) & DIV_MASK) | DIV32_MASK); // 分频器设置

	writeregs(SPI_CONFIG_REG, readregs(SPI_CONFIG_REG) & CPHA_ACTIVE_MASK); // CPHA设置

	writeregs(SPI_CONFIG_REG, readregs(SPI_CONFIG_REG) & CPOL_LOW_MASK); // CPOL设置

	writeregs(SPI_CONFIG_REG, readregs(SPI_CONFIG_REG) & AUTO_START_MASK); // 自动开始数据传输

	writeregs(SPI_CONFIG_REG, readregs(SPI_CONFIG_REG) | MAN_CS_MASK); // 手动片选

	writeregs(INTRPT_DIS_REG, DIS_INTR_VALUE); // 禁止所有中断

	writeregs(SPI_CONFIG_REG, readregs(SPI_CONFIG_REG) | MASTER_MODE_MASK); // SPI模式设置
	
	printk(KERN_INFO "Succeed to load %s drive module\n", DEVICE_NAME);
	
	return 0;
}

/* RF设备驱动卸载函数 */
static void __exit spi_exit(void)
{
	/* 删除设备文件节点 */
	device_unregister(spi_dev.spi_device);
	class_destroy(spi_dev.spi_class);
	/* 注销字符设备 */
	cdev_del(&spi_dev.cdev);
	/* 释放设备号 */
	unregister_chrdev_region(spi_dev.cdev.dev, 1);

	/* Release mapping using iounmap */	
	iounmap(base_addr); 
	
	printk(KERN_INFO "Succeed to unload %s drive module\n", DEVICE_NAME);
}

module_init(spi_init); /* 执行insmod命令时调用spi_init函数 */
module_exit(spi_exit); /* 执行rmmod命令是调用spi_exit函数 */
MODULE_LICENSE("GPL"); /* 许可证 */


