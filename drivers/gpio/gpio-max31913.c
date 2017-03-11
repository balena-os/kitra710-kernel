/*
 * MAX31913 GPIO gpio digital input driver
 * 
 */


#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/spi/spi.h>
#include <linux/spi/max31913.h>
#include <linux/gpio.h>
#include <asm/byteorder.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

#define DRIVER_NAME "max31913"


/*
 * Pin configurations
 */
#define PIN_CONFIG_MASK 0x03
#define PIN_CONFIG_IN_PULLUP 0x03
#define PIN_CONFIG_IN_WO_PULLUP 0x02
#define PIN_CONFIG_OUT 0x01

#define PIN_NUMBER 6

//#define SPI_WORD_8 //option by hardware
#define SPI_WORD_16

//#define MAXIM_GPIO_OUT    0
#define MAXIM_GPIO_IN     1

/*
 * Some registers must be read back to modify.
 * To save time we cache them here in memory
 */
struct max31913 {
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

#ifdef MAXIM_GPIO_OUT
static int max31913_parityCheck( struct max31913 *mc )
{

    int ret;
    bool _p0 , _p1 , _p2, _np0 = 0 ; 
    mc->port_config &= 0xFF00 ; 
    mc->cksum = ( u8 ) (mc->port_config >> 8 ) ; 
    printk("max31913->check IN : %d - \n" , mc->cksum) ;
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
    printk("max31913->p0: %d - \n" , _p0) ;
    printk("max31913->p1: %d - \n" , _p1) ;
    printk("max31913->p2: %d - \n" , _p2) ;
    printk("max31913->np0: %d - \n" , _np0) ;
    
    mc->port_config |= mc->cksum ; 
    ret = true ; //TBI 
    return ret ;

}


static int max31913_write_config(struct max31913 *mc)
{
    
    int ret;
    ret = spi_write(mc->spi, (const u8 *)&mc->port_config, sizeof(mc->port_config));

    ret |= spi_read(mc->spi, (u8 *)&mc->port_status, sizeof(mc->port_status));
    printk("max31913->gpio write : %d -  read : %d - \n" , mc->port_config, mc->port_status) ;
   
                
    return ret ;
}


//spi_write_then_read
static int __max31913_set(struct max31913 *mc, unsigned offset, int value)
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
        max31913_parityCheck(mc) ; 
	return max31913_write_config(mc);
}


static void max31913_set(struct gpio_chip *chip, unsigned offset, int value)
{
	struct max31913 *mc = container_of(chip, struct max31913, chip);

	mutex_lock(&mc->lock);

	__max31913_set(mc, offset, value);

	mutex_unlock(&mc->lock);

        printk("max31913->gpio set offset : %d -  value : %d - \n" , offset, value) ;
}
#endif

static void max31913_set(struct gpio_chip *chip, unsigned offset, int value)
{


        printk("max31913->gpio set offset : %d -  value : %d - \n" , offset, value) ;
        printk("max31913->gpio ERROR : GPIO read only ! ") ;
}

static int max31913_get(struct gpio_chip *chip, unsigned offset)
{
	struct max31913 *mc = container_of(chip, struct max31913, chip);
	int status = 0 ;
        int ret ; 

	mutex_lock(&mc->lock);

	/* REVISIT reading this clears any IRQ ... */
	ret = spi_read(mc->spi, (u8 *)&mc->port_status, sizeof(mc->port_status));
         
        mutex_unlock(&mc->lock);
        
        
        printk("max31913->gpio read : %d - \n" ,  mc->port_status) ;
        
        if ( ret < 0 ) 
        {
            ret = 0 ; 
            printk("max31913->SPI read Error : %d - \n" ,  ret) ; 
            return ret ;
        }
        // read 16 bit register with CRC- or 8 bit without CRC
        // Anyway the status of GPIO is on MSB byte
        if ( ( (mc->port_status >> 8) & (1 << offset)) > 0 )
        {
            status = 1 ;
        }    
        // Check CRC polinomial 
        //TBI
        
	return status;
}

static const struct of_device_id max31913_spi_of_match[] = {
	{
		.compatible = "maxim,max31913",
		.data = (void *) 0,
	},

	{ },
};
MODULE_DEVICE_TABLE(of, max31913_spi_of_match);


static int max31913_probe(struct spi_device *spi)
{
	struct max31913 *mc;
	struct max31913_platform_data *pdata;
	int ret;
  
        const struct			of_device_id *match;
        u32				spi_max_frequency ;
        int                             cs_gpio ;
        u32                             base_gpio ;
        int				status ;
        
        printk("max31913->plat_data \n") ; 
	pdata = dev_get_platdata(&spi->dev);
        printk("max31913->probe \n") ; 

        match = of_match_device(of_match_ptr(max31913_spi_of_match), &spi->dev);
	if (match) {

            printk("max31913->match  \n" ) ; 
            cs_gpio = of_get_named_gpio(spi->dev.of_node,
			    "cs-gpio", 0);
             
            status = of_property_read_u32(spi->dev.of_node,
			    "spi-max-frequency", &spi_max_frequency);

            status = of_property_read_u32(spi->dev.of_node,
			    "base", &base_gpio);

           

             printk("max31913->frequency : %d - cs-gpio : %d - \n" ,spi_max_frequency,cs_gpio) ;
             printk("max31913->base gpio : %d - \n" ,base_gpio) ;
         }
         else
         {
             printk("max31913->no match  \n" ) ; 
             dev_dbg(&spi->dev, "incorrect or missing platform data\n");
             return -EINVAL;
         }
             
/*
	if (!pdata || !pdata->base) {
		dev_dbg(&spi->dev, "incorrect or missing platform data\n");
		return -EINVAL;
	}
*/

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
        printk("max31913->setup ok \n" ) ;

	mc = devm_kzalloc(&spi->dev, sizeof(struct max31913), GFP_KERNEL);
	if (!mc)
		return -ENOMEM;

	mutex_init(&mc->lock);

	spi_set_drvdata(spi, mc);

	mc->spi = spi;

	mc->chip.label = DRIVER_NAME,
	mc->chip.set = max31913_set;
        mc->chip.get = max31913_get;
	mc->chip.base = base_gpio ;//from DTS
	mc->chip.ngpio = PIN_NUMBER;
	mc->chip.can_sleep = true;
	mc->chip.dev = &spi->dev;
	mc->chip.owner = THIS_MODULE;
       
	// Set CS GPIO
        gpio_request(cs_gpio, "cs_spi_2") ;
        
        gpio_direction_output(cs_gpio, 0) ;
        gpio_export(cs_gpio , true ) ; 
        

        /* Test Periph SPI   */
        ret |= spi_read(mc->spi, (u8 *)&mc->port_status, sizeof(mc->port_status)); 

	if (ret) {
		dev_err(&spi->dev, "Failed reading to " DRIVER_NAME ": %d\n",
			ret);
		goto exit_destroy;
	}

        printk("max31913->read ok completed %d .. \n", mc->port_status ) ;

	ret = gpiochip_add(&mc->chip);
	if (ret)
		goto exit_destroy;

	return ret;

exit_destroy:
	mutex_destroy(&mc->lock);
	return ret;
}

static int max31913_remove(struct spi_device *spi)
{
	struct max31913 *mc;

        printk("max31913->remove") ; 
	mc = spi_get_drvdata(spi);
	if (!mc)
		return -ENODEV;

	gpiochip_remove(&mc->chip);
	mutex_destroy(&mc->lock);

	return 0;
}

static const struct spi_device_id max31913_ids[] = {
	{ "max31913", 0 },
	
	{ },
};
MODULE_DEVICE_TABLE(spi, max31913_ids);


static struct spi_driver max31913_driver = {
	.driver = {
		.name		= DRIVER_NAME,
		.owner		= THIS_MODULE,
                .of_match_table = of_match_ptr(max31913_spi_of_match),
	},
	.probe		= max31913_probe,
	.remove		= max31913_remove,
        .id_table	= max31913_ids,
};

static int __init max31913_init(void)
{
        printk("max31913->init \n" ) ; 
	return spi_register_driver(&max31913_driver);
}
/* register after spi postcore initcall and before
 * subsys initcalls that may rely on these GPIOs
 */
subsys_initcall(max31913_init);

static void __exit max31913_exit(void)
{
        printk("max31913->unregister \n") ; 
	spi_unregister_driver(&max31913_driver);
}
module_exit(max31913_exit);

MODULE_AUTHOR("kalpa srl");
MODULE_LICENSE("GPL v2");
