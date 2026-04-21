#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/of.h>
#include <linux/atomic.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/stddef.h>
#include <linux/version.h>

struct sanath_irq_data {
    int irq;
    atomic_t counter;
    unsigned long last_irq;
    struct work_struct work;
};

dev_t dev = 0;
static struct class *dev_class;
static struct cdev etx_cdev;
static struct sanath_irq_data *g_data;

/*
** Function Prototypes
*/
static int __init etx_driver_init(void);
static void __exit etx_driver_exit(void);
static int etx_open(struct inode *inode, struct file *file);
static int etx_release(struct inode *inode, struct file *file);
static ssize_t etx_read(struct file *filp, char __user *buf, size_t len, loff_t *off);

/*
** File Operations
*/
static struct file_operations fops = {
    .owner   = THIS_MODULE,
    .read    = etx_read,
    .open    = etx_open,
    .release = etx_release,
};

static irqreturn_t sanath_irq_handler(int irq, void *dev_id)
{
    struct sanath_irq_data *data = dev_id;
    unsigned long now = jiffies;

    /* Ignore if less than 50ms since last interrupt */
    if (now - data->last_irq < msecs_to_jiffies(50))
        return IRQ_HANDLED;
    data->last_irq = now;
    schedule_work(&data->work);

    return IRQ_HANDLED;
}

static void sanath_irq_work(struct work_struct *work)
{
    struct sanath_irq_data *data = container_of(work, struct sanath_irq_data, work);
    // now you have access to data->counter, data->wq, etc.
    atomic_inc(&data->counter);
    pr_info("sanath-irq-lab: IRQ fired, counter = %d\n", atomic_read(&data->counter));
}

static int sanath_irq_probe(struct platform_device *pdev)
{
    struct sanath_irq_data *data;
    int ret;

    data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;
    g_data = data;
    INIT_WORK(&data->work, sanath_irq_work);

    data->irq = platform_get_irq(pdev, 0);
    if (data->irq < 0) {
        dev_err(&pdev->dev, "Failed to get IRQ\n");
        return data->irq;
    }

    ret = devm_request_irq(&pdev->dev, data->irq, sanath_irq_handler,
                           IRQF_TRIGGER_FALLING, "sanath-irq-lab", data);
    if (ret) {
        dev_err(&pdev->dev, "Failed to request IRQ\n");
        return ret;
    }

    platform_set_drvdata(pdev, data);

    dev_info(&pdev->dev, "sanath-irq-lab probed successfully\n");
    return 0;
}

static int sanath_irq_remove(struct platform_device *pdev)
{
    struct sanath_irq_data *data = platform_get_drvdata(pdev);
    cancel_work_sync(&data->work);
    dev_info(&pdev->dev, "sanath-irq-lab removed\n");
    return 0;
}

static const struct of_device_id sanath_irq_of_match[] = {
    { .compatible = "sanath-irq-lab", },
    { }
};
MODULE_DEVICE_TABLE(of, sanath_irq_of_match);

static struct platform_driver sanath_irq_driver = {
    .probe  = sanath_irq_probe,
    .remove = sanath_irq_remove,
    .driver = {
        .name           = "sanath-irq-lab",
        .of_match_table = sanath_irq_of_match,
    },
};

static char *etx_devnode(struct device *dev, umode_t *mode)
{
    if (mode)
        *mode = 0666;
    return NULL;
}

/*
** Init
*/
static int __init etx_driver_init(void)
{
    if (alloc_chrdev_region(&dev, 0, 1, "etx_Dev") < 0) {
        pr_err("Cannot allocate major number\n");
        return -1;
    }

    pr_info("Major=%d Minor=%d\n", MAJOR(dev), MINOR(dev));

    cdev_init(&etx_cdev, &fops);

    if (cdev_add(&etx_cdev, dev, 1) < 0) {
        pr_err("Cannot add device\n");
        goto r_class;
    }

        dev_class = class_create(THIS_MODULE, "sanath_class");


    if (IS_ERR(dev_class)) {
        pr_err("Cannot create class\n");
        goto r_class;
    }

    dev_class->devnode = etx_devnode;

    if (IS_ERR(device_create(dev_class, NULL, dev, NULL, "sanath_queue"))) {
        pr_err("Cannot create device\n");
        goto r_device;
    }

    pr_info("Driver Inserted\n");
        /* register platform driver */
    return platform_driver_register(&sanath_irq_driver);

r_device:
    class_destroy(dev_class);
r_class:
    unregister_chrdev_region(dev, 1);
    return -1;
}

/*
** Exit
*/
static void __exit etx_driver_exit(void)
{
    platform_driver_unregister(&sanath_irq_driver);
    device_destroy(dev_class, dev);
    class_destroy(dev_class);
    cdev_del(&etx_cdev);
    unregister_chrdev_region(dev, 1);
    pr_info("Driver Removed\n");
}

static int etx_open(struct inode *inode, struct file *file)
{
    pr_info("device_opened");
    return 0;
}

static int etx_release(struct inode *inode, struct file *file)
{
    pr_info("Device Closed\n");
    return 0;
}

static ssize_t etx_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
    char kbuf[32];
    int kbuf_len;

    if (*off > 0)
        return 0;

    if (!g_data)
        return -ENODEV;

    kbuf_len = snprintf(kbuf, sizeof(kbuf), "%d\n", atomic_read(&g_data->counter));

    if (copy_to_user(buf, kbuf, kbuf_len))
        return -EFAULT;

    *off += kbuf_len;
    return kbuf_len;
}

module_init(etx_driver_init);
module_exit(etx_driver_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sanath Sapre");
MODULE_DESCRIPTION("Sanath IRQ Lab Platform Driver");
MODULE_VERSION("1.0");
