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
#include <linux/irq.h>    /*含有IRQ_HANDLED\IRQ_TYPE_EDGE_RISING*/


#include <linux/interrupt.h> /*含有request_irq、free_irq函数*/
#include <linux/delay.h>   /*内核延时*/


#include <linux/netdevice.h>   /*网络相关结构体*/
#include <linux/etherdevice.h>
#include <linux/fcntl.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/bitops.h>
#include <linux/ip.h>

#include "jf_mm_opreation.h"

#define JF_MM2S_MODULE_DEBUD
#ifdef JF_MM2S_MODULE_DEBUD
    #define LOG(fmt,args...) printk(KERN_ERR fmt,##args)
#else
    #define LOG(fmt,args...)
#endif

/*函数声明*/
static int jf_mm_operation_open(struct inode *inode, struct file* filp);
static int jf_mm_operation_close(struct inode *inode, struct file *filp);
static ssize_t jf_mm_operation_read (struct file *file_p, char __user * buf, size_t len , loff_t * ppos);
static ssize_t jf_mm_operation_write(struct file *file_p, const char __user * buf, size_t len , loff_t * ppos);
static int jf_dma_controler_mm2s_init(void);
static int jf_dma_controler_mm2s_exit(void);
static int jf_dma_controler_s2mm_init(void);
static int jf_dma_controler_s2mm_exit(void);

static int test_network_init(void);
static int jf_network_receive(struct net_device *dev);
static int jf_network_send(struct sk_buff *skb,struct net_device *dev);

/*私有变量*/
static struct file_operations jf_mm_operation_op=
{
    .owner   = THIS_MODULE,
    .open    = jf_mm_operation_open,
    .release = jf_mm_operation_close,
    .read    = jf_mm_operation_read,
    .write   = jf_mm_operation_write,
};

static struct net_device_ops jf_net_ops = {
    .ndo_start_xmit = jf_network_send,

};



static unsigned int ev_dma_mm2s_finsh;                          /*mm2s传输完成*/
DECLARE_WAIT_QUEUE_HEAD(dma_mm2s_waitq);                        /*注册一个等待队列dma_mm2s_waitq,等待中断发生*/

static unsigned int ev_dma_s2mm_finsh;                          /*s2mm传输完成*/
DECLARE_WAIT_QUEUE_HEAD(dma_s2mm_waitq);                        /*注册一个等待队列dma_s2mm_waitq,等待中断发生*/


static struct jf_mm_operation_driver driver;                    /*驱动设备结构体*/
static struct jf_mm_opreation_dma *dma_device_mm2s;             /*DMA mm2s控制器操作结构体*/
static struct jf_mm_opreation_dma *dma_device_s2mm;             /*DMA s2mm控制器操作结构体*/


static struct net_device *jf_network_dev;                      /*网络设备结构*/

/*内部接口*/

/*dam mm2s 中断服务函数*/
static irqreturn_t  jf_mm_dma_write_irq(int irq, void *dev_id)
{
    LOG("%s\n",__FUNCTION__);

    *(dma_device_mm2s->access_address_virtual + MM2S_DMASR_OFFSET) |= MM2S_DMASR_IOC_IRQ_FLAG_MASK; /* Clear interrupt flag */
    *(dma_device_mm2s->access_address_virtual + MM2S_DMASR_OFFSET) |= MM2S_DMASR_ERR_IRQ_FLAG_MASK;
   // wake_up_interruptible(&dma_mm2s_waitq);                                                      /* 唤醒休眠的进程，即调用read函数的进程*/
    ev_dma_mm2s_finsh = 1; 
       
    return IRQ_HANDLED;
}

/*dam s2mm 中断服务函数*/
static irqreturn_t  jf_mm_dma_read_irq(int irq, void *dev_id)
{
     LOG("%s\n",__FUNCTION__);
    *(dma_device_s2mm->access_address_virtual + S2MM_DMASR_OFFSET) |= S2MM_DMASR_IOC_IRQ_FLAG_MASK; /* Clear interrupt flag */
    *(dma_device_s2mm->access_address_virtual + S2MM_DMASR_OFFSET) |= S2MM_DMASR_ERR_IRQ_FLAG_MASK;


    
    if ((*(dma_device_s2mm->access_address_virtual + S2MM_DMASR_OFFSET) & S2MM_DMASR_DMASlvErr_MASK)) {

      LOG("%s\n","S2MM_DMASR_DMASlvErr_MASK");
		return 0;
	}

	if ((*(dma_device_s2mm->access_address_virtual + S2MM_DMASR_OFFSET) & S2MM_DMASR_DMADecErr_MASK)) {
		LOG("%s\n","S2MM_DMASR_DMADecErr_MASK");
		return 0;
	}
   // wake_up_interruptible(&dma_s2mm_waitq);                                                      /* 唤醒休眠的进程，即调用read函数的进程*/
    ev_dma_s2mm_finsh = 1; 

    jf_network_receive(jf_network_dev);    /*接收网络数据包*/
  
    *(dma_device_s2mm->access_address_virtual + S2MM_LENGTH_OFFSET) = 8000;
    return IRQ_HANDLED;
}

static ssize_t jf_mm_operation_read(struct file *file_p, char __user * buf, size_t len , loff_t * ppos)
{
    size_t  rev_len=0;
  
  
    *(dma_device_s2mm->access_address_virtual + S2MM_LENGTH_OFFSET) = 8000;       /*设置期望接收的长度*/

    LOG("len is %d\n",len);


    wait_event_interruptible(dma_s2mm_waitq, ev_dma_s2mm_finsh);                 /*等待中断服务函数的唤醒*/
    
    LOG("%s\n","weak up success\n");

    rev_len = *(dma_device_s2mm->access_address_virtual + S2MM_LENGTH_OFFSET);   /*获取实际读取的长度*/
   
    LOG("rev_len is %d\n",rev_len);

    copy_to_user(buf,(char *)dma_device_s2mm->ddr_address_virtual+4,rev_len-4);     /*拷贝数据到用户空间*/
    

    ev_dma_s2mm_finsh = 0;

    return rev_len;
}

static ssize_t jf_mm_operation_write(struct file *file_p,const char __user * buf, size_t len , loff_t * ppos)
{
    int ret;
    LOG("Enter %s",__FUNCTION__);
    LOG("write dat rel length is %d\n",len);
   // copy_from_user(dma_device_mm2s->access_address_virtual,buf,len);                    //debug 1   ddr地址写成设备地址
  
     *(uint16_t*)dma_device_mm2s->ddr_address_virtual = 0x04a0;           /*mahl帧头*/
     *(uint16_t*)(dma_device_mm2s->ddr_address_virtual+2) = len+4;
     *(uint16_t*)(dma_device_mm2s->ddr_address_virtual+4) = len;          /*有效数据长度*/

    ret = copy_from_user((char *)dma_device_mm2s->ddr_address_virtual+6,buf,len);
    if(ret<0)
    {
        LOG("%s\n","copy_from_user error");
        return -1;
    }
    *(dma_device_mm2s->access_address_virtual + MM2S_LENGTH_OFFSET) = 32;                /*根据pg021描述，此寄存器已一旦被设置，传输就会开始*/
    
    wait_event_interruptible(dma_mm2s_waitq, ev_dma_mm2s_finsh);                    /*就当前进程放入等待队列，等待传输中断发生，唤醒该进程*/
    ev_dma_mm2s_finsh = 0;
    return len;
}
/*
    初始化DMA mm2s控制器
*/

static int jf_dma_controler_mm2s_init(void)
{
    int i=0;    //循环变量

    dma_device_mm2s = kmalloc(sizeof(struct jf_mm_opreation_dma),GFP_KERNEL);
    if(dma_device_mm2s == NULL)
    {
        LOG("%s,%s\n",__FUNCTION__,"kmalloc error");
    }
    dma_device_mm2s->ddr_mapSize          = DDR_SRC_MAP_SIZE;                 /*设置DMA映射内存大小*/
    //dma_device_mm2s->ddr_address_phy      = (void *)DDR_SRC_BASE_ADDR;      /*设置内存地址*/
    dma_device_mm2s->access_address_phy   = (void *)DMA_MM2S_REG_MAP_BASE;         /*DMA控制器基物理地址*/
    dma_device_mm2s->mapSzie              = DMA_MM2S_REG_MAP_SIZE;                 /*控制器映射大小*/
    dma_device_mm2s->dma_write_irq        = JF_DMA_MM2S_IRQ;                  /*中断号*/
    
    LOG("init 1 succes\n");

    /*step 1 mapping address */
    dma_device_mm2s->access_address_virtual   = (volatile unsigned long *)ioremap((unsigned int)dma_device_mm2s->access_address_phy, dma_device_mm2s->mapSzie);
    
    //dma_device_mm2s->ddr_address_virtual    = (volatile unsigned long *)ioremap(DDR_SRC_BASE_ADDR, dma_device_mm2s->ddr_mapSize);
    dma_device_mm2s->ddr_address_virtual      = (unsigned long)dma_alloc_writecombine(NULL,dma_device_mm2s->ddr_mapSize, (dma_addr_t *)&(dma_device_mm2s->ddr_address_phy), GFP_KERNEL);

    LOG("map dma address is %p\n",dma_device_mm2s->ddr_address_virtual);

    LOG("init 2 succes\n");
    /* step 2: Reset */
	*dma_device_mm2s->access_address_virtual =*dma_device_mm2s->access_address_virtual | MM2S_DMACR_RESET_MASK;
	while (*dma_device_mm2s->access_address_virtual & MM2S_DMACR_RESET_MASK);
    LOG("init 3 succes\n");
    /* step 3: Running */
	*dma_device_mm2s->access_address_virtual = *dma_device_mm2s->access_address_virtual | MM2S_DMACR_RS_MASK;
    msleep(1); // 1000us
	do {
        msleep(1); // 1000us
        i++;
		if (i>10) {
			LOG("Fail to run dma!\n");
			return -1;
		}
	}while (*(dma_device_mm2s->access_address_virtual + MM2S_DMASR_OFFSET) & MM2S_DMASR_HALTED_MASK);

    LOG("init 4 succes\n");
	/* step 4: Enable interrupt */
	*dma_device_mm2s->access_address_virtual = *dma_device_mm2s->access_address_virtual | MM2S_DMACR_IOC_IRQ_ENABLE_MASK;    /*使能传输完成中断*/
	*dma_device_mm2s->access_address_virtual = *dma_device_mm2s->access_address_virtual | MM2S_DMACR_ERR_IRQ_ENABLE_MASK;    /*使能错误中断*/

	/* step 5: Setting source address */
	*(dma_device_mm2s->access_address_virtual + MM2S_SA_LSB_OFFSET) = dma_device_mm2s->ddr_address_phy;           /*设置DDR地址*/
    LOG("init 5 succes\n");
    /*6 request system interrupt */
    if(request_irq(dma_device_mm2s->dma_write_irq, jf_mm_dma_write_irq,0x00,"dma_mm2s",NULL)) 
    {
        LOG("Can't request_irq\n");
        return -EBUSY;
    }
    LOG("init 6 succes\n");

    return 0;
}
/*
  初始化DMA s2mm控制器
*/
static int jf_dma_controler_s2mm_init(void)
{
    int i=0;    //循环变量
    LOG("Enter %s",__FUNCTION__);

    dma_device_s2mm = kmalloc(sizeof(struct jf_mm_opreation_dma),GFP_KERNEL);
    if(dma_device_s2mm == NULL)
    {
        LOG("%s,%s\n",__FUNCTION__,"kmalloc error");
    }   
    dma_device_s2mm->ddr_mapSize          = DDR_DST_MAP_SIZE;                      /*设置DMA映射内存大小*/
    dma_device_s2mm->access_address_phy   = (void *)DMA_S2MM_REG_MAP_BASE;         /*DMA控制器基物理地址*/
    dma_device_s2mm->mapSzie              = DMA_S2MM_REG_MAP_SIZE;                 /*控制器映射大小*/
    dma_device_s2mm->dma_write_irq        = JF_DMA_S2MM_IRQ;                       /*中断号*/
    
    LOG("init 1 succes\n");

    /*step 1 mapping address */
    dma_device_s2mm->access_address_virtual = (volatile unsigned long *)ioremap((unsigned int)dma_device_s2mm->access_address_phy, dma_device_s2mm->mapSzie);

    dma_device_s2mm->ddr_address_virtual    = (unsigned long)dma_alloc_writecombine(NULL,dma_device_s2mm->ddr_mapSize, (dma_addr_t *)&(dma_device_s2mm->ddr_address_phy), GFP_KERNEL);
    if(dma_device_s2mm->ddr_address_virtual == NULL)
    {
        LOG("dma_device_s2mm error \n");
    }

    LOG("%p\n",dma_device_s2mm->ddr_address_virtual);

    LOG("init 2 succes\n");
    /* step 2: Reset */
	*dma_device_s2mm->access_address_virtual =*dma_device_s2mm->access_address_virtual | S2MM_DMACR_RESET_MASK;
	while (*dma_device_s2mm->access_address_virtual & S2MM_DMACR_RESET_MASK);
    LOG("init 3 succes\n");
    /* step 3: Running */
	*dma_device_s2mm->access_address_virtual = *dma_device_s2mm->access_address_virtual | S2MM_DMACR_RS_MASK;
    msleep(1); // 1000us
	do {
        msleep(1); // 1000us
        i++;
		if (i>10) {
			LOG("Fail to run dma!\n");
			return -1;
		}
	}while (*(dma_device_s2mm->access_address_virtual + S2MM_DMASR_OFFSET) & S2MM_DMASR_HALTED_MASK);

    LOG("init 4 succes\n");
	/* step 4: Enable interrupt */
	*dma_device_s2mm->access_address_virtual = *dma_device_s2mm->access_address_virtual | S2MM_DMACR_IOC_IRQ_ENABLE_MASK;    /*使能传输完成中断*/
	*dma_device_s2mm->access_address_virtual = *dma_device_s2mm->access_address_virtual | S2MM_DMACR_ERR_IRQ_ENABLE_MASK;    /*使能错误中断*/

	/* step 5: Setting source address */
	*(dma_device_s2mm->access_address_virtual + S2MM_DA_LSB_OFFSET) = dma_device_s2mm->ddr_address_phy;                      /*设置DDR地址*/
    LOG("init 5 succes\n");
    /*6 request system interrupt */
    if(request_irq(dma_device_s2mm->dma_write_irq, jf_mm_dma_read_irq,0x00,"dma_s2mm",NULL)) 
    {
        LOG("Can't request_irq\n");
        return -EBUSY;
    }
    LOG("init 6 succes\n");

    *(dma_device_s2mm->access_address_virtual + S2MM_LENGTH_OFFSET) = 8000;     /*设置期望接收的长度*/



    return 0;
}

static int jf_dma_controler_s2mm_exit(void)
{
    /* 1、注销中断*/
     free_irq(dma_device_s2mm->dma_write_irq,NULL);                                          
     *dma_device_s2mm->access_address_virtual = *dma_device_s2mm->access_address_virtual & (~MM2S_DMACR_RS_MASK);               
     dma_free_writecombine(NULL, dma_device_s2mm->ddr_mapSize,(void *)dma_device_s2mm->ddr_address_virtual,(dma_addr_t)dma_device_s2mm->ddr_address_phy);
     iounmap((unsigned long *)dma_device_s2mm->access_address_phy);                                                             
     kfree(dma_device_s2mm);

     return 0;
}

static int jf_dma_controler_mm2s_exit(void)
{
    /* 1、注销中断*/
     free_irq(dma_device_mm2s->dma_write_irq,NULL);                                                                               //debug 2 使用为共享中断时候，第二个参数必须是NULL，不然会出现异常
     *dma_device_mm2s->access_address_virtual = *dma_device_mm2s->access_address_virtual & (~MM2S_DMACR_RS_MASK);                  /*stop dma controler*/
     dma_free_writecombine(NULL, dma_device_mm2s->ddr_mapSize,(void *)dma_device_mm2s->ddr_address_virtual,(dma_addr_t)dma_device_mm2s->ddr_address_phy); /*释放DMA缓冲区*/
     iounmap((unsigned long *)dma_device_mm2s->access_address_phy);                                                              /*取消dma控制器映射*/
     kfree(dma_device_mm2s);

     return 0;
}

/*
1、DMA控制器初始化
2、中断初始化
*/

static int jf_mm_operation_open(struct inode *inode, struct file* filp)
{
    LOG("Enter %s",__FUNCTION__);
    
    return 0;
}

static int jf_mm_operation_close(struct inode *inode, struct file *filp)
{
    LOG("Enter %s",__FUNCTION__);
    
    return 0;
}


 /*
 1、创建字符设备
 2、创建设备节点
*/
static int jf_mm_operation_init(void)
{

    int ret;
    LOG("Enter %s\n",__FUNCTION__);        

    cdev_init(&driver.cdev,&jf_mm_operation_op);               /*注册字符设备*/
    ret = alloc_chrdev_region(&driver.cdev.dev, 0, 1, DEVICE_NAME);  /*分配字符设备*/
    if(ret != 0)
        goto alloc_err;

    ret = cdev_add(&driver.cdev, driver.cdev.dev, 1);                /*添加字符设备*/
    if(ret != 0)
        goto add_err;

    driver.mm_class = class_create(THIS_MODULE, DEVICE_NAME);    
	driver.mm_device= device_create(driver.mm_class, NULL, driver.cdev.dev, NULL, FILE_NAME);   /*创建设备节点*/

    jf_dma_controler_mm2s_init();                                                 /*初始化DMA控制器*/
    jf_dma_controler_s2mm_init(); 

    test_network_init();     /*初始化网络设备*/
    
    return 0; 

    add_err:
        cdev_del(&driver.cdev);
                 
    alloc_err:
        unregister_chrdev_region(driver.cdev.dev, 1);
        device_unregister(driver.mm_device);

    return -1;
}

static void jf_mm_operation_exit(void)
{
    LOG("Exit %s\n",__FUNCTION__);

    jf_dma_controler_mm2s_exit();
    jf_dma_controler_s2mm_exit();

    device_unregister(driver.mm_device);
	class_destroy(driver.mm_class);
	/* 注销字符设备 */
	cdev_del(&driver.cdev);
	/* 释放设备号 */
	unregister_chrdev_region(driver.cdev.dev, 1);



    unregister_netdev(jf_network_dev);
    free_netdev(jf_network_dev);
}

/*
   网络相关驱动程序
*/
static int test_network_init(void)
{
    LOG("Enter %s\n",__FUNCTION__);     

    jf_network_dev = alloc_etherdev(0);     /*分配一个网络设备结构*/  

    jf_network_dev->netdev_ops = &jf_net_ops; /*发送数据包函数*/

    jf_network_dev->dev_addr[0]        =0x11;             //设置MAC地址
    jf_network_dev->dev_addr[1]        =0x22;
    jf_network_dev->dev_addr[2]        =0x33;
    jf_network_dev->dev_addr[3]        =0x44;
    jf_network_dev->dev_addr[4]        =0x55;
    jf_network_dev->dev_addr[5]        =0x66;

    jf_network_dev->flags    |= IFF_NOARP;        //不使用ARP（地址解析协议）
	jf_network_dev->features |= NETIF_F_IP_CSUM;

    register_netdev(jf_network_dev);
     *(dma_device_s2mm->access_address_virtual + S2MM_LENGTH_OFFSET) =65535;         /*设置期望接收的长度*/

    return 0;
}
static int jf_network_send(struct sk_buff *skb,struct net_device *dev)
{

     LOG("Enter %s\n",__FUNCTION__);

     netif_stop_queue(dev);  

     *(uint16_t*)dma_device_mm2s->ddr_address_virtual = 0x04a0;           /*mahl帧头*/
     *(uint16_t*)(dma_device_mm2s->ddr_address_virtual+2) = skb->len+4;
     *(uint16_t*)(dma_device_mm2s->ddr_address_virtual+4) = skb->len;    /*有效数据长度*/

     memcpy((unsigned char *)dma_device_mm2s->ddr_address_virtual+6,skb->data,skb->len);

     LOG("skb data len is %d\n",skb->len);

    *(dma_device_mm2s->access_address_virtual + MM2S_LENGTH_OFFSET) = skb->len+6;        /*根据pg021描述，此寄存器已一旦被设置，传输就会开始*/

    dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb->len;
	dev->trans_start     =jiffies;

	netif_wake_queue(dev);                                                                /* 数据全部发送出去后，唤醒网卡的队列*/
    //dev_kfree_skb(skb);                                                           

	return 0;
}
static unsigned char buf[16000];
static int jf_network_receive(struct net_device *dev)
{   

     /*data fam*/
    /*********************************************/
    /* tatle(2)|length(2)|data(length)           */
    /*********************************************/
    static uint16_t      length =0;               /*需要接收数据长度*/
    static char          received_ok = 1;         /*一帧数据接收完成*/
    static uint16_t      RxLen =0;                /*长度计数*/
           uint16_t      len;                     /*单次接收长度*/
    struct sk_buff      *skb;                     /*网络数据包*/
    unsigned char       *rdptr;
    unsigned char       rev_times=0;
    LOG("Enter %s\n",__FUNCTION__);
    LOG("%d\n",(int)(*(uint16_t *)(dma_device_s2mm->ddr_address_virtual)));
    if(1 == received_ok)                          /*数据接收完成*/
    {  
        length = *(uint16_t*)(dma_device_s2mm->ddr_address_virtual+4);               /*接下来要接收的数据长度*/
        if(length > 1500)
        {
            return -1;
        }
        LOG("length is %d\n",length);   
    }

    if(length >= *(dma_device_s2mm->access_address_virtual + S2MM_LENGTH_OFFSET)-6)
    {
        if(length > 1500)
        {
            return -1;
        }
        received_ok = 1;
        memcpy(buf,dma_device_s2mm->ddr_address_virtual+6,length);
    }
    else 
    {
        received_ok = 0;  
        rev_times++;
        //rev_times==1?len =(*(dma_device_s2mm->access_address_virtual + S2MM_LENGTH_OFFSET)-6):(*(dma_device_s2mm->access_address_virtual + S2MM_LENGTH_OFFSET)-4);  
        len = (*(dma_device_s2mm->access_address_virtual + S2MM_LENGTH_OFFSET)-4);
        RxLen+=len;                                                                                      /*记录接收到的数据长度*/
        LOG("len is %d\n",RxLen);
    }
    LOG("RxLen is %d\n",RxLen);

    if(received_ok==0 && RxLen < length)                                                                 /*一次没有接收完成*/
    {
         rev_times==1?memcpy(buf+RxLen,dma_device_s2mm->ddr_address_virtual+6,len-6):memcpy(buf+RxLen,dma_device_s2mm->ddr_address_virtual+4,len-4);
         return 0;                   
    }
    if(RxLen >= length)
    {
        memcpy(buf+RxLen,dma_device_s2mm->ddr_address_virtual+4,len-4);
        RxLen  = 0;
        received_ok = 1;
        rev_times=0;
    }
    if(received_ok == 1)
    {
        LOG("%s and real data length is %d \n","received_ok",length);

        if((skb = netdev_alloc_skb(dev, length +2)) != NULL)
        {
                rdptr = (u8 *) skb_put(skb, length);
                memcpy(rdptr,buf,length);                                      //测试使用两次拷贝，之后优化
                skb->protocol = eth_type_trans(skb, dev);
                skb->dev=dev;
                skb->ip_summed = CHECKSUM_UNNECESSARY;
                dev->stats.rx_bytes += skb->len; 
                dev->stats.rx_packets++;
                netif_rx(skb); 
               // dev_kfree_skb(skb);                                       //暂时未找到释放的方法，只要释放就会发生空指针异常
                length = 0;
        }
    }
    return 0;
}


module_init(jf_mm_operation_init);
module_exit(jf_mm_operation_exit);
MODULE_AUTHOR("yhl");
MODULE_DESCRIPTION("jf_mm_operation");
MODULE_LICENSE("GPL");