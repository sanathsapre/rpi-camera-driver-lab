/***************************************************************************//**
*  \file       01_msg_queue_driver.c
*
*  \details    Simple Linux device driver kernel buffer
*
*  \author     Sanath Sapre
*
*******************************************************************************/
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
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

#define ROW_SIZE 16
#define MEM_SIZE 128
#define DONE     1

dev_t dev = 0;
static struct class *dev_class;
static struct cdev etx_cdev;

typedef struct kernel_logger {
    uint8_t kernel_buffer[ROW_SIZE][MEM_SIZE];
    uint8_t read_indexer;
    uint8_t write_indexer;
    struct mutex etx_mutex;
    uint8_t count;
} kernel_logger_t;

kernel_logger_t kernel_logger;
static DECLARE_WAIT_QUEUE_HEAD(etx_wait_queue);

/*
** Function Prototypes
*/
static int __init etx_driver_init(void);
static void __exit etx_driver_exit(void);
static int etx_open(struct inode *inode, struct file *file);
static int etx_release(struct inode *inode, struct file *file);
static ssize_t etx_read(struct file *filp, char __user *buf, size_t len, loff_t *off);
static ssize_t etx_write(struct file *filp, const char __user *buf, size_t len, loff_t *off);

/*
** File Operations
*/
static struct file_operations fops = {
    .owner   = THIS_MODULE,
    .read    = etx_read,
    .write   = etx_write,
    .open    = etx_open,
    .release = etx_release,
};

/*
** ===== DEVNODE COMPATIBILITY (STRICT MATCHING) =====
*/

/* Kernel 6.x (const struct device *) */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,0,0)

static char *etx_devnode(const struct device *dev, umode_t *mode)
{
    if (mode)
        *mode = 0666;
    return NULL;
}

#else   /* Kernel 5.x and below */

/* Kernel 5.x (non-const struct device *) */
static char *etx_devnode(struct device *dev, umode_t *mode)
{
    if (mode)
        *mode = 0666;
    return NULL;
}

#endif

/*
** Open
*/
static int etx_open(struct inode *inode, struct file *file)
{
    int *done = kmalloc(sizeof(int), GFP_KERNEL);

    if (!done) {
        pr_err("Malloc failed\n");
        return -ENOMEM;
    }

    *done = 0;
    file->private_data = done;

    pr_info("Device Opened\n");
    return 0;
}

/*
** Release
*/
static int etx_release(struct inode *inode, struct file *file)
{
    kfree(file->private_data);
    file->private_data = NULL;

    pr_info("Device Closed\n");
    return 0;
}

/*
** Read
*/
static ssize_t etx_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
    ssize_t ret = 0;
    int *done = filp->private_data;

    if (*done)
        return 0;

    mutex_lock(&kernel_logger.etx_mutex);

    while (kernel_logger.count == 0) {
        mutex_unlock(&kernel_logger.etx_mutex);

        if (wait_event_interruptible_exclusive(etx_wait_queue,
                                               kernel_logger.count > 0))
            return -ERESTARTSYS;

        mutex_lock(&kernel_logger.etx_mutex);
    }

    if (copy_to_user(buf,
                     kernel_logger.kernel_buffer[kernel_logger.read_indexer],
                     MEM_SIZE)) {
        pr_err("Read error\n");
        ret = -EFAULT;
        goto out;
    }

    ret = MEM_SIZE;

    memset(kernel_logger.kernel_buffer[kernel_logger.read_indexer], 0, MEM_SIZE);
    kernel_logger.read_indexer = (kernel_logger.read_indexer + 1) % ROW_SIZE;
    kernel_logger.count--;
    *done = DONE;

out:
    mutex_unlock(&kernel_logger.etx_mutex);
    return ret;
}

/*
** Write
*/
static ssize_t etx_write(struct file *filp,
                         const char __user *buf,
                         size_t len,
                         loff_t *off)
{
    size_t copy_len = min(len, (size_t)(MEM_SIZE - 1));
    ssize_t ret = 0;

    mutex_lock(&kernel_logger.etx_mutex);

    if (copy_from_user(kernel_logger.kernel_buffer[kernel_logger.write_indexer],
                       buf,
                       copy_len)) {
        pr_err("Write error\n");
        ret = -EFAULT;
        goto out;
    }

    kernel_logger.kernel_buffer[kernel_logger.write_indexer][copy_len] = '\0';
    ret = copy_len;

    kernel_logger.write_indexer =
        (kernel_logger.write_indexer + 1) % ROW_SIZE;

    if (kernel_logger.count < ROW_SIZE)
        kernel_logger.count++;
    else
        kernel_logger.read_indexer =
            (kernel_logger.read_indexer + 1) % ROW_SIZE;

    wake_up_interruptible(&etx_wait_queue);

out:
    mutex_unlock(&kernel_logger.etx_mutex);
    return ret;
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

    /* class_create compatibility */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,4,0)
        dev_class = class_create("sanath_class");
#else
        dev_class = class_create(THIS_MODULE, "sanath_class");
#endif

    if (IS_ERR(dev_class)) {
        pr_err("Cannot create class\n");
        goto r_class;
    }

    dev_class->devnode = etx_devnode;

    if (IS_ERR(device_create(dev_class, NULL, dev, NULL, "sanath_queue"))) {
        pr_err("Cannot create device\n");
        goto r_device;
    }

    mutex_init(&kernel_logger.etx_mutex);

    pr_info("Driver Inserted\n");
    return 0;

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
    device_destroy(dev_class, dev);
    class_destroy(dev_class);
    cdev_del(&etx_cdev);
    unregister_chrdev_region(dev, 1);

    pr_info("Driver Removed\n");
}

module_init(etx_driver_init);
module_exit(etx_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sanath Sapre");
MODULE_DESCRIPTION("Kernel Logger");
MODULE_VERSION("2.0");
