#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/capability.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/mutex.h>

#include <linux/slab.h>      /*kmalloc*/

static const unsigned short test_i2c_addr_list[] = { 0x74, I2C_CLIENT_END };   /*对应电路中的PCA9548器件地址*/

struct i2c_test_st
{
    struct i2c_adapter     *i2c_adap;         /*i2c适配器引用*/
    struct i2c_board_info   i2c_info;         /*i2c信息引用*/
    struct i2c_client      *i2c_client_p;     /*挂接在控制器上的 i2c 器件*/
};
static struct i2c_test_st *i2c_test_g;

static int __init i2c_init(void)
{
   
    i2c_test_g = kmalloc(GFP_KERNEL,sizeof(struct i2c_test_st));   
    if(i2c_test_g == NULL)
    {
        return -1;
    }  
    memset(&i2c_test_g->i2c_info, 0, sizeof(struct i2c_board_info));                     /*置零结构体*/
    strscpy(i2c_test_g->i2c_info.type, "i2c_test", sizeof(i2c_test_g->i2c_info.type));   /*平台设备*/

    i2c_test_g->i2c_adap = i2c_get_adapter(0);                                           /*根据编号，获取soc i2c控制器*/
    if(i2c_test_g->i2c_adap == NULL)
    {
        return -1;
    }
    i2c_test_g->i2c_client_p= i2c_new_scanned_device(i2c_test_g->i2c_adap,              /*该函数会扫面板卡上挂接0号i2c控制器test_i2c所指定的地址*/
     &i2c_test_g->i2c_info,test_i2c_addr_list, NULL);  
    i2c_put_adapter(i2c_test_g->i2c_adap);        
    printk("%d\n",i2c_test_g->i2c_client_p->addr);                                         /*使用完之后，释放*/           
    return 0;
}

static void __exit i2c_exit(void)
{
    i2c_unregister_device(i2c_test_g->i2c_client_p);                                     /*删除i2c设备*/    

    kfree(i2c_test_g);
}

module_init(i2c_init);
module_exit(i2c_exit);

MODULE_AUTHOR("13820023307@163.com");
MODULE_DESCRIPTION("I2C test driver");
MODULE_LICENSE("GPL");