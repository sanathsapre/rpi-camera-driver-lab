#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/pm_runtime.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mediabus.h>
#include <media/media-entity.h>

/*
** Function Prototypes
*/
static int __init sanath_v4l2_init(void);
static void __exit sanath_v4l2_exit(void);

struct sanath_v4l2_dev {
    struct v4l2_device v4l2_dev;
    struct v4l2_subdev sd;
    struct media_pad pad;
};

static int sanath_s_stream(struct v4l2_subdev *sd, int enable)
{
    struct platform_device *client;
    int ret = 0;

    client = v4l2_get_subdevdata(sd);

    if (enable) {
		ret = pm_runtime_resume_and_get(&client->dev);
        if (ret < 0)
			goto error_unlock;
        
            dev_info(&client->dev, "sanath-v4l2-lab stream-on is called\n");
	
    } else {
        dev_info(&client->dev, "sanath-v4l2-lab stream-off is called\n");
		pm_runtime_put(&client->dev);
    }
error_unlock:
        return ret;
}

static const struct v4l2_subdev_video_ops sanath_subdev_video_ops = {
	.s_stream =		sanath_s_stream,
};

static const struct v4l2_subdev_core_ops sanath_subdev_core_ops = {

};

static int sanath_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index > 0)
		return -EINVAL;

	code->code = MEDIA_BUS_FMT_SBGGR10_1X10;

	return 0;
}

static const struct v4l2_subdev_pad_ops sanath_subdev_pad_ops = {
    .enum_mbus_code		= sanath_enum_mbus_code,
};

static const struct v4l2_subdev_ops sanath_subdev_ops = {
	.core		= &sanath_subdev_core_ops,
	.video		= &sanath_subdev_video_ops,
	.pad		= &sanath_subdev_pad_ops,
};

static int sanath_v4l2_probe(struct platform_device *pdev)
{
    struct sanath_v4l2_dev *priv;
    int ret;

    priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    platform_set_drvdata(pdev, priv);

    /* register v4l2_device first */
    ret = v4l2_device_register(&pdev->dev, &priv->v4l2_dev);
    if (ret)
        return ret;

    /* init and register subdev */
    v4l2_subdev_init(&priv->sd, &sanath_subdev_ops);
    priv->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
    priv->sd.owner = THIS_MODULE;
    priv->sd.dev = &pdev->dev;
    snprintf(priv->sd.name, sizeof(priv->sd.name), "sanath-v4l2-stub");

    v4l2_set_subdevdata(&priv->sd, pdev);

    priv->pad.flags = MEDIA_PAD_FL_SOURCE;
    ret = media_entity_pads_init(&priv->sd.entity, 1, &priv->pad);
    if (ret)
        goto err_v4l2_unregister;

    ret = v4l2_device_register_subdev(&priv->v4l2_dev, &priv->sd);
    if (ret)
        goto err_v4l2_unregister;
    ret = v4l2_device_register_subdev_nodes(&priv->v4l2_dev);
    if (ret)
        goto err_subdev_unregister;

    pm_runtime_set_active(&pdev->dev);
    pm_runtime_enable(&pdev->dev);
    return 0;

err_subdev_unregister:
    v4l2_device_unregister_subdev(&priv->sd);
err_entity_cleanup:
    media_entity_cleanup(&priv->sd.entity);
err_v4l2_unregister:
    v4l2_device_unregister(&priv->v4l2_dev);
    return ret;
}

static int sanath_v4l2_remove(struct platform_device *pdev)
{
    struct sanath_v4l2_dev *priv = platform_get_drvdata(pdev);
    dev_info(&pdev->dev, "sanath-v4l2-lab removed\n");
    pm_runtime_disable(&pdev->dev);
    v4l2_device_unregister_subdev(&priv->sd);
    v4l2_device_unregister(&priv->v4l2_dev);
    return 0;
}


static int sanath_runtime_suspend(struct device *dev)
{
    dev_info(dev, "runtime suspend\n");
    return 0;
}

static int sanath_runtime_resume(struct device *dev)
{
    dev_info(dev, "runtime resume\n");
    return 0;
}


static const struct dev_pm_ops sanath_pm_ops = {
    .runtime_suspend = sanath_runtime_suspend,
    .runtime_resume  = sanath_runtime_resume,
};

static const struct of_device_id sanath_v4l2_of_match[] = {
    { .compatible = "sanath-v4l2-lab", },
    { }
};
MODULE_DEVICE_TABLE(of, sanath_v4l2_of_match);

static struct platform_driver sanath_v4l2_driver = {
    .probe  = sanath_v4l2_probe,
    .remove = sanath_v4l2_remove,
    .driver = {
        .name           = "sanath-v4l2-lab",
    	.pm = &sanath_pm_ops,
        .of_match_table = sanath_v4l2_of_match,
    },
};

/*
** Init
*/
static int __init sanath_v4l2_init(void)
{
    return platform_driver_register(&sanath_v4l2_driver);
}

/*
** Exit
*/
static void __exit sanath_v4l2_exit(void)
{
    platform_driver_unregister(&sanath_v4l2_driver);
    pr_info("v4l2 Driver Removed\n");
}


module_init(sanath_v4l2_init);
module_exit(sanath_v4l2_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("sanath Sapre");
MODULE_DESCRIPTION("sanath v4l2 Lab Platform Driver");
MODULE_VERSION("1.0");
