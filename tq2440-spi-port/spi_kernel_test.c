#include <linux/config.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>

#define TEST_REG 0x02

static int test_read_reg(struct spi_device *spi, int reg)
{
    char buf[2];
    buf[0] = reg << 2;
    buf[1] = 0;
    spi_write_then_read(spi, buf, 2, buf, 2);
    return buf[1] << 8 | buf[0];
}

static int spi_test_probe(struct spi_device *spi)
{
    printk("TEST_REG: 0x%02x\n", test_read_reg(spi, TEST_REG));
    return 0;
}

static int spi_test_remove(struct spi_device *spi)
{
    return 0;
}

static struct spi_driver spi_test_driver =
{
    .probe = spi_test_probe,
    .remove = spi_test_remove,
    .driver = {
        .name = "testHW",
    },
};

static int __init spi_test_init(void)
{
    return spi_register_driver(&spi_test_driver);
}
 
static void __exit spi_test_exit(void)
{
    spi_unregister_driver(&spi_test_driver);
}
 
module_init(spi_test_init);
module_exit(spi_test_exit);
 
MODULE_DESCRIPTION("spi device test");
MODULE_LICENSE("GPL");
 
