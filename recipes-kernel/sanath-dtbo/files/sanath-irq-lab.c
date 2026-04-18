#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/of.h>

struct sanath_irq_data {
    int irq;
    unsigned long counter;
    unsigned long last_irq;
    wait_queue_head_t wq;
};

static irqreturn_t sanath_irq_handler(int irq, void *dev_id)
{
    struct sanath_irq_data *data = dev_id;
    unsigned long now = jiffies;

    /* Ignore if less than 50ms since last interrupt */
    if (now - data->last_irq < msecs_to_jiffies(50))
        return IRQ_HANDLED;

    data->last_irq = now;
    data->counter++;
    pr_info("sanath-irq-lab: IRQ fired, counter = %lu\n", data->counter);
    wake_up_interruptible(&data->wq);

    return IRQ_HANDLED;
}

static int sanath_irq_probe(struct platform_device *pdev)
{
    struct sanath_irq_data *data;
    int ret;

    data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    init_waitqueue_head(&data->wq);

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

module_platform_driver(sanath_irq_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sanath Sapre");
MODULE_DESCRIPTION("Sanath IRQ Lab Platform Driver");
MODULE_VERSION("1.0");
