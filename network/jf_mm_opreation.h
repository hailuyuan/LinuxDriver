#ifndef __JF__MM__OPREATION__
#define __JF__MM__OPREATION__


#include <linux/dma-mapping.h>


/*debug support*/
//#define JF_MM2S_MODULE_DEBUD

/*宏定义*/
#define DEVICE_NAME "jf_mm_operation"  /*设备名称*/
#define FILE_NAME   "jf_mm_op"         /*设备节点名称*/

#define JF_DMA_MM2S_IRQ  168         /*写中断*/

#define DDR_SRC_MAP_SIZE 	0x400000         /* DMA缓冲区大小 2^23 Bytes */


#define DMA_MM2S_REG_MAP_BASE    0x40400000      /*mm2s（写外设）DMA控制器基地址*/
#define	DMA_MM2S_REG_MAP_SIZE	0x10000             /*mm2s（写外设）DMA控制器区域大小*/

#define	MM2S_DMACR_OFFSET			(0x00U / sizeof(unsigned int))    /*MDA控制寄存器*/
#define MM2S_DMASR_OFFSET			(0x04U / sizeof(unsigned int))    /*DMA状态寄存器*/
#define MM2S_SA_LSB_OFFSET			(0x18U / sizeof(unsigned int))    /*源地址*/
#define MM2S_SA_MSB_OFFSET			(0x1CU / sizeof(unsigned int))    /*64位源地址*/
#define	MM2S_LENGTH_OFFSET			(0x28U / sizeof(unsigned int))    /*传输长度*/

/* MM2S_DMACR Register MASK */
#define MM2S_DMACR_RS_MASK					0x1U                      /*RUN/STOP*/
#define	MM2S_DMACR_RESET_MASK				(0x1U << 2)               /*复位寄存器*/
#define MM2S_DMACR_IOC_IRQ_ENABLE_MASK		(0x1U << 12)              /*中断使能*/
#define MM2S_DMACR_ERR_IRQ_ENABLE_MASK		(0x1U << 14)              /*错误中断*/

/* MM2S_DMASR Register MASK */
#define MM2S_DMASR_HALTED_MASK				0x1U                     /*查询DMA通道状态*/
#define MM2S_DMASR_IDLE_MASK				(0x1U << 1)              /*空闲查xun*/
#define MM2S_DMASR_DMASlvErr_MASK			(0x1U << 5)              /*DMA从机错误*/
#define MM2S_DMASR_DMADecErr_MASK			(0x1U << 6)              /*DMA编码错误*/
#define MM2S_DMASR_IOC_IRQ_FLAG_MASK		(0x1U << 12)             /*传输完成中断*/
#define MM2S_DMASR_ERR_IRQ_FLAG_MASK		(0x1U << 14)             /*DMA错误中断*/




#define JF_DMA_S2MM_IRQ  169         /*读中断*/

#define DMA_S2MM_REG_MAP_BASE    0x40410030          /*s2mm（读外设）DMA控制器基地址*/
#define	DMA_S2MM_REG_MAP_SIZE	 0x10000             /*s2mm （读外设）DMA控制器区域大小*/

#define WIDTH_BUFF_LEN_REG	23 /* 23bits */
#define DDR_DST_MAP_SIZE 	0x400000 /* 2^23 Bytes */

/* S2MM channel of axi dma register */
#define	S2MM_DMACR_OFFSET			(0x00U / sizeof(unsigned int))
#define S2MM_DMASR_OFFSET			(0x04U / sizeof(unsigned int))
#define S2MM_DA_LSB_OFFSET			(0x18U / sizeof(unsigned int))
#define S2MM_DA_MSB_OFFSET			(0x1CU / sizeof(unsigned int))
#define	S2MM_LENGTH_OFFSET			(0x28U / sizeof(unsigned int))

/* S2MM_DMACR Register MASK */
#define S2MM_DMACR_RS_MASK					0x1U
#define	S2MM_DMACR_RESET_MASK				(0x1U << 2)
#define S2MM_DMACR_IOC_IRQ_ENABLE_MASK		(0x1U << 12)
#define S2MM_DMACR_ERR_IRQ_ENABLE_MASK		(0x1U << 14)

/* S2MM_DMASR Register MASK */
#define S2MM_DMASR_HALTED_MASK				0x1U
#define S2MM_DMASR_IDLE_MASK				(0x1U << 1)
#define S2MM_DMASR_DMASlvErr_MASK			(0x1U << 5)
#define S2MM_DMASR_DMADecErr_MASK			(0x1U << 6)
#define S2MM_DMASR_IOC_IRQ_FLAG_MASK		(0x1U << 12)
#define S2MM_DMASR_ERR_IRQ_FLAG_MASK		(0x1U << 14)


/*结构体申明*/

struct jf_mm_operation_driver
{
    struct cdev   cdev;             /*字符设备结构体*/
    struct class  *mm_class;        /*创建设备文件 */
    struct device *mm_device;       /*创建设备文件 */   
};

struct jf_mm_opreation_dma{
	volatile unsigned long    *access_address_virtual;    /*DMA控制器虚拟地址*/
    volatile unsigned long    *access_address_phy;        /*DMA控制器物理地址*/
	unsigned int mapSzie;                                 /*DMA控制器地址映射大小*/

	volatile unsigned long   ddr_address_phy;            /*映射ddr内存物理地址*/
    volatile unsigned long   ddr_address_virtual;         /*映射ddr内存虚拟地址*/
	unsigned int ddr_mapSize;       /*映射ddr内存大小*/

    int dma_write_irq;

};

#endif