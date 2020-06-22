
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/capability.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/mutex.h>

#include <linux/delay.h>

static int i2c_test_drv_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
    

	struct i2c_adapter *adapter = client->adapter;  /*获取适配器*/
	u8 ctl_reg=0x0;                                 /*PCA9548 Control Register*/
	struct i2c_msg test_msg[2];                     /*构建一个i2c-msg*/
	u8 set_val = 0xff;                              /*打开PCA9548所有通道*/
	int    status;

	union i2c_smbus_data data;

	bool i2c_fn_i2c;
    i2c_fn_i2c = i2c_check_functionality(adapter, I2C_FUNC_I2C);
    if(i2c_fn_i2c)
	{
		printk("i2c_check_functionality ok\n");
	}

	test_msg[0].addr    =     0x74;   
	test_msg[0].flags   =     0;  
	test_msg[0].buf     =     &set_val;
    test_msg[0].len     =     1;
	i2c_transfer(adapter , &test_msg[0] , 1);           /*写入PCA9548控制寄存器*/
	printk("PCA9548 Control Register =%x\n",ctl_reg);

	printk("i2c_test_drv_probe\n");

	ctl_reg=0;
	test_msg[1].addr    =     0x74;   
	test_msg[1].flags   =     I2C_M_RD;  
	test_msg[1].buf     =     &ctl_reg;
    test_msg[1].len     =     1;
	i2c_transfer(adapter , &test_msg[1] , 1);           /*读取PCA9548控制寄存器*/

	printk("PCA9548 Control Register = %x\n",ctl_reg);



    pid_reg             =     0xf6;                      /*0xf5  = 0x75*/

	test_msg[1].addr    =     0x39;   
	test_msg[1].buf     =     &set_val;
    test_msg[1].len     =     1;
	
	test_msg[0].addr    =     0x39;   
	test_msg[0].flags   =     I2C_M_RD;  
	test_msg[0].buf     =     &ctl_reg;
	test_msg[0].len     =     1;
	i2c_transfer(adapter,r_msgs,2);              /*读取ADV7511PID寄存器*/

	data.byte=0x40;
    //status = i2c_smbus_xfer(adapter,0x39,client->flags ,I2C_SMBUS_WRITE, 0x41,I2C_SMBUS_BYTE_DATA, &data);

    

	//status = i2c_smbus_xfer(adapter,0x39,client->flags ,I2C_SMBUS_WRITE, 0x41,I2C_SMBUS_BYTE_DATA, &data);
   //  msleep(1000);
	//status = i2c_smbus_xfer(adapter,0x39,client->flags ,I2C_SMBUS_READ, 0x06,I2C_SMBUS_BYTE_DATA, &data);
 //i2c_smbus_xfer(struct i2c_adapter *adapter, u16 addr,unsigned short flags, char read_write,u8 command, int protocol, union i2c_smbus_data *data)

	printk("ADV7511 Control Register = %x\n",ctl_reg);


    return 0;
}
static int i2c_test_drv_remove(struct i2c_client *client)
{
    printk("eeprom_remove\n");
    return 0;

}

static struct i2c_device_id i2c_test_drv_idtable[] = {
	{ "i2c_test", 0 },                                       /* 第二个成员是私有数据，这里不需要 */
	{ }
  };
MODULE_DEVICE_TABLE(i2c, i2c_test_drv_idtable);

static struct i2c_driver i2c_test_drv = {
	.driver		= {
		.owner		= THIS_MODULE,
		.name		= "i2c_test_example",                    /*这里的名字不重要*/
	},
	.id_table	= i2c_test_drv_idtable,
	.probe		= i2c_test_drv_probe,
	.remove		= i2c_test_drv_remove,
};

static int __init i2c_init(void)
{
   return i2c_add_driver(&i2c_test_drv);
}

static void __exit i2c_exit(void)
{
    i2c_del_driver(&i2c_test_drv);
}

module_init(i2c_init);
module_exit(i2c_exit);

MODULE_AUTHOR("13820023307@163.com");
MODULE_DESCRIPTION("I2C test driver");
MODULE_LICENSE("GPL");