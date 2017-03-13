/*
 * VNI8200 GPIO digital output driver
 * 
 */


#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/spi/spi.h>
#include <linux/spi/vni8200.h>
#include <linux/gpio.h>
#include <asm/byteorder.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

#define DRIVER_NAME "vni8200"

/*
 * Pin configurations
 */
#define PIN_CONFIG_MASK 0x03
#define PIN_CONFIG_IN_PULLUP 0x03
#define PIN_CONFIG_IN_WO_PULLUP 0x02
#define PIN_CONFIG_OUT 0x01

#define PIN_NUMBER 6

//#define SPI_WORD_8 // option by hardware
#define SPI_WORD_16

/*
 * Some registers must be read back to modify.
 * To save time we cache them here in memory
 */
struct vni8200 {
	struct mutex	lock;	/* protect from simultaneous accesses */
#ifdef  SPI_WORD_8
	u8		port_config;
        u8              port_status ; 
#else
        u16		port_config;
        u16             port_status ;
#endif
        u8              cksum ;
	struct gpio_chip chip;
	struct spi_device *spi;
};


static int vni8200_parityCheck( struct vni8200 *mc )
{

    int ret;
    bool _p0 , _p1 , _p2, _np0 = 0 ; 
    mc->port_config &= 0xFF00 ; 
    mc->cksum = ( u8 ) (mc->port_config >> 8 ) ; 
    printk("vni8200->check IN : %d - \n" , mc->cksum) ;
    _p0 =  ((mc->cksum & 0x01)>0) ^ ((mc->cksum & 0x02)>0) ^ ((mc->cksum & 0x04)>0) ^ ((mc->cksum & 0x08)>0) ; 
    _p0 ^= ((mc->cksum & 0x10)>0) ^ ((mc->cksum & 0x20)>0) ^ ((mc->cksum & 0x40)>0) ^ ((mc->cksum & 0x80)>0) ;
    _p1 =  ((mc->cksum & 0x02)>0) ^ ((mc->cksum & 0x08)>0) ^ ((mc->cksum & 0x20)>0) ^ ((mc->cksum & 0x80)>0) ; 
    _p2 =  ((mc->cksum & 0x01)>0) ^ ((mc->cksum & 0x04)>0) ^ ((mc->cksum & 0x10)>0) ^ ((mc->cksum & 0x40)>0) ; 
    mc->cksum = 0 ;
    mc->cksum |= ( _p0 << 1 ) ;
    mc->cksum |= ( _p1 << 2 ) ;
    mc->cksum |= ( _p2 << 3) ;
    _np0 = !_p0 ;
    mc->cksum |= _np0 ;
    //printk("vni8200->p0: %d - \n" , _p0) ;
    //printk("vni8200->p1: %d - \n" , _p1) ;
    //printk("vni8200->p2: %d - \n" , _p2) ;
    //printk("vni8200->np0: %d - \n" , _np0) ;
    
    mc->port_config |= mc->cksum ; 
    ret = true ; //TBI 
    return ret ;

}


static int vni8200_write_config(struct vni8200 *mc)
{
    //su16 word = 0  ;
    int ret;
    ret = spi_write(mc->spi, (const u8 *)&mc->port_config, sizeof(mc->port_config));

    ret |= spi_read(mc->spi, (u8 *)&mc->port_status, sizeof(mc->port_status));
    printk("vni8200->gpio write : %d -  read : %d - \n" , mc->port_config, mc->port_status) ;
  
                
    return ret ;
}

//spi_write_then_read
static int __vni8200_set(struct vni8200 *mc, unsigned offset, int value)
{

#ifdef  SPI_WORD_8
	if (value)
		mc->port_config |= 1 << offset;
	else
		mc->port_config &= ~(1 << offset);
#else
        mc->port_config &= 0xFF00 ; //erase previous parity check
        if (value)
		mc->port_config |= ((1 << offset) << 8) ;
	else
        {  
		mc->port_config &= (~(1 << offset)) << 8   ;
                
        }

#endif
        vni8200_parityCheck(mc) ; 
	return vni8200_write_config(mc);
}


static void vni8200_set(struct gpio_chip *chip, unsigned offset, int value)
{
	struct vni8200 *mc = container_of(chip, struct vni8200, chip);

	mutex_lock(&mc->lock);

	__vni8200_set(mc, offset, value);

	mutex_unlock(&mc->lock);

        printk("vni8200->gpio set offset : %d -  value : %d - \n" , offset, value) ;
}




//#ifdef CONFIG_SPI_MASTER
static const struct of_device_id vni8200_spi_of_match[] = {
	{
		.compatible = "st,vni8200",
		.data = (void *) 0,
	},

	{ },
};
MODULE_DEVICE_TABLE(of, vni8200_spi_of_match);
//#endif




static int vni8200_probe(struct spi_device *spi)
{
	struct vni8200 *mc;
	struct vni8200_platform_data *pdata;
	int ret;
        //su16 word = 0  ; 
        //u8 halfword = 0  ; 
        const struct			of_device_id *match;
        u32				spi_max_frequency ;
        int                             cs_gpio ;
        u32                             base_gpio ;
        int				status ;
        
        printk("vni8200->plat_data \n") ; 
	pdata = dev_get_platdata(&spi->dev);
        printk("vni8200->probe \n") ; 

        match = of_match_device(of_match_ptr(vni8200_spi_of_match), &spi->dev);
	if (match) {

            printk("vni8200->match  \n" ) ; 
            cs_gpio = of_get_named_gpio(spi->dev.of_node,
			    "cs-gpio", 0);
             
            status = of_property_read_u32(spi->dev.of_node,
			    "spi-max-frequency", &spi_max_frequency);

            status = of_property_read_u32(spi->dev.of_node,
			    "base", &base_gpio);

           

             printk("vni8200->frequency : %d - cs-gpio : %d - \n" ,spi_max_frequency,cs_gpio) ;
             printk("vni8200->base gpio : %d - \n" ,base_gpio) ;
         }
         else
         {
             printk("vni8200->no match  \n" ) ; 
             dev_dbg(&spi->dev, "incorrect or missing platform data\n");
             return -EINVAL;
         }
             
/*
	if (!pdata || !pdata->base) {
		dev_dbg(&spi->dev, "incorrect or missing platform data\n");
		return -EINVAL;
	}
*/

        //VNI8200 specific configuration
        /* bits_per_word cannot be configured in platform data */
#ifdef  SPI_WORD_8
        spi->bits_per_word = 8;
#else
	spi->bits_per_word = 16;
#endif
        spi->mode = SPI_MODE_0 ;
        spi->max_speed_hz = 1 * 1000 * 1000 ;
        //spi->chip_select = 0 ;
        spi->cs_gpio = cs_gpio; 

	ret = spi_setup(spi); 
        
	if (ret < 0)
		return ret;

	mc = devm_kzalloc(&spi->dev, sizeof(struct vni8200), GFP_KERNEL);
	if (!mc)
		return -ENOMEM;

	mutex_init(&mc->lock);

	spi_set_drvdata(spi, mc);

	mc->spi = spi;

	mc->chip.label = DRIVER_NAME,
	mc->chip.set = vni8200_set;
	mc->chip.base = base_gpio ;//pdata->base;
	mc->chip.ngpio = PIN_NUMBER;
	mc->chip.can_sleep = true;
	mc->chip.dev = &spi->dev;
	mc->chip.owner = THIS_MODULE;
        
        // Test init config
#ifdef  SPI_WORD_8
        mc->port_config = 0x01;
        mc->cksum = 0 ;

#else
        mc->port_config = 0x010A;
#endif

	// Set CS
        gpio_request(cs_gpio, "cs_spi_2") ;
        
        gpio_direction_output(cs_gpio, 0) ;
        gpio_export(cs_gpio , true ) ; 
        
	/* write twice, because during initialisation the first setting
	 * is just for testing SPI communication, and the second is the
	 * "real" configuration
	 */
	//ret = vni8200_write_config(mc);
        //ret = spi_write(mc->spi, &mc->port_config, sizeof(mc->port_config));

	if (ret) {
		dev_err(&spi->dev, "Failed writing to " DRIVER_NAME ": %d\n",
			ret);
		goto exit_destroy;
	}
        printk("vni8200->write ok completed \n" ) ;


        //ret = spi_read(mc->spi, (u8 *)&mc->port_status, sizeof(mc->port_status));
	if (ret)
		return ret;
        printk("vni8200->read ok completed %d .. \n", mc->port_status ) ;
        printk("vni8200->port config %d ..\n", mc->port_config) ;

	ret = gpiochip_add(&mc->chip);
	if (ret)
		goto exit_destroy;

	return ret;

exit_destroy:
	mutex_destroy(&mc->lock);
	return ret;
}

static int vni8200_remove(struct spi_device *spi)
{
	struct vni8200 *mc;

        printk("vni8200->remove") ; 
	mc = spi_get_drvdata(spi);
	if (!mc)
		return -ENODEV;

	gpiochip_remove(&mc->chip);
	mutex_destroy(&mc->lock);

	return 0;
}

static const struct spi_device_id vni8200_ids[] = {
	{ "vni8200", 0 },
	
	{ },
};
MODULE_DEVICE_TABLE(spi, vni8200_ids);


static struct spi_driver vni8200_driver = {
	.driver = {
		.name		= DRIVER_NAME,
		.owner		= THIS_MODULE,
                .of_match_table = of_match_ptr(vni8200_spi_of_match),
	},
	.probe		= vni8200_probe,
	.remove		= vni8200_remove,
        .id_table	= vni8200_ids,
};

static int __init vni8200_init(void)
{
        printk("vni8200->init \n") ; 
	return spi_register_driver(&vni8200_driver);
}
/* register after spi postcore initcall and before
 * subsys initcalls that may rely on these GPIOs
 */
subsys_initcall(vni8200_init);

static void __exit vni8200_exit(void)
{
        printk("vni8200->unregister \n ") ; 
	spi_unregister_driver(&vni8200_driver);
}
module_exit(vni8200_exit);

MODULE_AUTHOR("kalpa srl");
MODULE_LICENSE("GPL v2");
