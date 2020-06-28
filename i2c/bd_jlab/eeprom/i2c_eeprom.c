#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/capability.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>    /*�ں����ߺ���*/

#include <linux/slab.h>      /*kmalloc*/

static const unsigned short test_i2c_addr_list[] = { 0x51, I2C_CLIENT_END };   /*��Ӧ��·�е�PCA9548������ַ*/

struct i2c_test_st
{
    struct i2c_adapter     *i2c_adap;         /*i2c����������*/
    struct i2c_board_info   i2c_info;         /*i2c��Ϣ����*/
    struct i2c_client      *i2c_client_p;     /*�ҽ��ڿ������ϵ� i2c ����*/
};
static struct i2c_test_st *i2c_test_g;


#define EEPROM_ADDR 0x51

static int read_byte_eeprom(struct i2c_adapter *adapter, unsigned short addr, unsigned char *data)
{
    unsigned char data_addr[2] ={(unsigned char)(addr>>8),(unsigned char)(addr&0xff)}; 
    unsigned char addr_msb = addr>>8;
    unsigned char addr_lsb = addr&0xff;

    struct i2c_msg read_msg[3];                     /*����һ��i2c-msg*/
    if(adapter == NULL)
    {
        printk("adapter is empty\n");
        return -1;
    }
    /*д����Ҫ��ȡ�ĵ�ַ*/ 
    read_msg[0].addr  = EEPROM_ADDR;
    read_msg[0].flags = 0;
    read_msg[0].buf   = data_addr;
    read_msg[0].len   = 2;

    read_msg[1].addr  = EEPROM_ADDR;
    read_msg[1].flags = I2C_M_RD;
    read_msg[1].buf   = data;//&data_addr[1];
    read_msg[1].len   = 1;
    i2c_transfer(adapter , read_msg , 2); 

    return 0;
}

static int write_byte_eeprom(struct i2c_adapter *adapter, unsigned short addr, unsigned char *data)
{
    unsigned char data_addr[2] ={(unsigned char)(addr>>8),(unsigned char)(addr&0xff)}; 
    struct i2c_msg read_msg[3];                     /*����һ��i2c-msg*/
    if(adapter == NULL)
    {
        printk("adapter is empty\n");
        return -1;
    }
    /*д����Ҫ��ȡ�ĵ�ַ*/ 
    read_msg[0].addr  = EEPROM_ADDR;
    read_msg[0].flags = 0;
    read_msg[0].buf   = data_addr;
    read_msg[0].len   = 2;
    /*д������*/
    read_msg[1].addr  = EEPROM_ADDR;
    read_msg[1].flags = 0;
    read_msg[1].buf   = data;
    read_msg[1].len   = 1;  
    i2c_transfer(adapter , read_msg , 2);
    return 0;
}

static int __init i2c_init(void)
{
    unsigned char data=0;
    int i=0;

    printk("enter module \n");
    i2c_test_g = kmalloc(sizeof(struct i2c_test_st),GFP_KERNEL);  
     if(i2c_test_g == NULL)
     {
         return -1;
         printk("kmalloc error \n");
     }  
    memset(&i2c_test_g->i2c_info, 0, sizeof(struct i2c_board_info));                     /*����ṹ��*/

    strlcpy(i2c_test_g->i2c_info.type,"i2c_test", I2C_NAME_SIZE);

    i2c_test_g->i2c_adap = i2c_get_adapter(0);                                           /*���ݱ�ţ���ȡsoc i2c������*/
    if(i2c_test_g->i2c_adap == NULL)
    {
        return -1;
    }
    // i2c_test_g->i2c_client_p= i2c_new_probed_device(i2c_test_g->i2c_adap,              /*�ú�����ɨ��忨�Ϲҽ�0��i2c������test_i2c��ָ���ĵ�ַ*/
    // &i2c_test_g->i2c_info,test_i2c_addr_list, NULL); 

   // printk("scaned a new client %s\n",i2c_test_g->i2c_client_p->name); 
     
     for(i=0;i<100;i++)
     {
         read_byte_eeprom(i2c_test_g->i2c_adap,i,&data);
         printk("first read = %x\n",data);
     }

     for(i=0;i<100;i++)
     {
        write_byte_eeprom(i2c_test_g->i2c_adap,i,&i);
        msleep(100);   
     }

     for(i=0;i<100;i++)
     {
         read_byte_eeprom(i2c_test_g->i2c_adap,i,&data);
         printk("first read = %x\n",data);
     }


    i2c_put_adapter(i2c_test_g->i2c_adap);  
   // printk("%d\n",i2c_test_g->i2c_client_p->addr);                                         /*ʹ����֮���ͷ�*/           
    return 0;
}

static void __exit i2c_exit(void)
{
    //i2c_unregister_device(i2c_test_g->i2c_client_p);                                     /*ɾ��i2c�豸*/    

    kfree(i2c_test_g);
}

module_init(i2c_init);
module_exit(i2c_exit);

MODULE_AUTHOR("yhl");
MODULE_DESCRIPTION("test_queue_exit");
MODULE_LICENSE("GPL");