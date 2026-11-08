#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-ioctl.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
/*
** Function Prototypes
*/
//Timer Variable
#define TIMEOUT 33    //milliseconds
void timer_callback(struct timer_list *data);
struct sanath_buffer {
	struct vb2_buffer   vb;
	struct list_head    list;
};
struct sanath_capture_dev {
	struct v4l2_device   v4l2_dev;
	struct video_device  vdev;
	struct vb2_queue     queue;
	struct mutex         lock;
	struct list_head     buf_list;
	spinlock_t           buf_lock;
	struct timer_list etx_timer;
	uint16_t frame_count;
};
static int start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct sanath_capture_dev *priv = vb2_get_drv_priv(vq);
	mod_timer(&priv->etx_timer, jiffies + msecs_to_jiffies(TIMEOUT));
	return 0;
}
static void stop_streaming(struct vb2_queue *vq)
{
	struct sanath_capture_dev *priv = vb2_get_drv_priv(vq);
	struct sanath_buffer *buf, *tmp;
	unsigned long flags;
    del_timer_sync(&priv->etx_timer);
	spin_lock_irqsave(&priv->buf_lock, flags);
	list_for_each_entry_safe(buf, tmp, &priv->buf_list, list) {
		list_del(&buf->list);
		vb2_buffer_done(&buf->vb, VB2_BUF_STATE_ERROR);
	}
	spin_unlock_irqrestore(&priv->buf_lock, flags);
}
static int queue_setup(struct vb2_queue *vq,
			unsigned int *num_buffers, unsigned int *num_planes,
			unsigned int sizes[], struct device *alloc_devs[])
{
	*num_planes = 1;
	sizes[0] = 640 * 480 * 2; /* YUYV, hardcoded for stub */
	return 0;
}
static void buffer_queue(struct vb2_buffer *vb)
{
	struct sanath_capture_dev *priv = vb2_get_drv_priv(vb->vb2_queue);
	struct sanath_buffer *buf = container_of(vb, struct sanath_buffer, vb);
	unsigned long flags;
	spin_lock_irqsave(&priv->buf_lock, flags);
	list_add_tail(&buf->list, &priv->buf_list);
	spin_unlock_irqrestore(&priv->buf_lock, flags);
}
static int buffer_prepare(struct vb2_buffer *vb)
{
	vb2_set_plane_payload(vb, 0, 640 * 480 * 2);
	return 0;
}
static const struct vb2_ops sanath_vb2_ops = {
	.queue_setup	 = queue_setup,
	.buf_prepare	 = buffer_prepare,
	.buf_queue	 	 = buffer_queue,
	.wait_prepare	 = vb2_ops_wait_prepare,
	.wait_finish	 = vb2_ops_wait_finish,
	.start_streaming = start_streaming,
	.stop_streaming	 = stop_streaming,
};
static const struct v4l2_file_operations sanath_vdev_fops = {
	.owner          = THIS_MODULE,
	.open           = v4l2_fh_open,
	.release        = vb2_fop_release,
	.poll           = vb2_fop_poll,
	.mmap           = vb2_fop_mmap,
	.unlocked_ioctl = video_ioctl2,
};
static int vidioc_querycap(struct file *file, void *priv,
						struct v4l2_capability *cap)
{
	strscpy(cap->driver, "sanath-capture", sizeof(cap->driver));
	strscpy(cap->card, "Sanath Capture", sizeof(cap->card));
	strscpy(cap->bus_info, "platform:sanath-capture", sizeof(cap->bus_info));
	return 0;
}
static int vidioc_enum_fmt(struct file *file, void *priv,
						struct v4l2_fmtdesc *f)
{
	if (f->index > 0)
		return -EINVAL;
	f->pixelformat = V4L2_PIX_FMT_YUYV;
	return 0;
}
static int vidioc_g_fmt(struct file *file, void *priv,
						struct v4l2_format *f)
{
	f->fmt.pix.width        = 640;
	f->fmt.pix.height       = 480;
	f->fmt.pix.pixelformat  = V4L2_PIX_FMT_YUYV;
	f->fmt.pix.field        = V4L2_FIELD_NONE;
	f->fmt.pix.bytesperline = 640 * 2;
	f->fmt.pix.sizeimage    = 640 * 480 * 2;
	f->fmt.pix.colorspace   = V4L2_COLORSPACE_SRGB;
	return 0;
}
static int vidioc_s_fmt(struct file *file, void *priv,
						struct v4l2_format *f)
{
	return vidioc_g_fmt(file, priv, f);
}
static const struct v4l2_ioctl_ops sanath_vdev_ioctl_ops = {
	.vidioc_reqbufs       	 = vb2_ioctl_reqbufs,
	.vidioc_querybuf      	 = vb2_ioctl_querybuf,
	.vidioc_qbuf          	 = vb2_ioctl_qbuf,
	.vidioc_dqbuf         	 = vb2_ioctl_dqbuf,
	.vidioc_streamon      	 = vb2_ioctl_streamon,
	.vidioc_streamoff     	 = vb2_ioctl_streamoff,
	.vidioc_querycap 	  	 = vidioc_querycap,
	.vidioc_enum_fmt_vid_cap = vidioc_enum_fmt,
	.vidioc_g_fmt_vid_cap    = vidioc_g_fmt,
	.vidioc_s_fmt_vid_cap    = vidioc_s_fmt,
};
static int sanath_v4l2_probe(struct platform_device *pdev)
{
	struct sanath_capture_dev *priv;
	int ret;
	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	platform_set_drvdata(pdev, priv);
	mutex_init(&priv->lock);
	spin_lock_init(&priv->buf_lock);
	/* setup your timer to call my_timer_callback */
    timer_setup(&priv->etx_timer, timer_callback, 0);
	INIT_LIST_HEAD(&priv->buf_list);
	ret = v4l2_device_register(&pdev->dev, &priv->v4l2_dev);
	if (ret)
	{
		pr_info("Unable to register v4l2 device.\n");
		goto err_reg;
	}
	priv->queue.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	priv->queue.io_modes = VB2_MMAP;
	priv->queue.ops = &sanath_vb2_ops;
	priv->queue.mem_ops = &vb2_vmalloc_memops;
	priv->queue.buf_struct_size = sizeof(struct sanath_buffer);
	priv->queue.lock            = &priv->lock;
	priv->queue.drv_priv        = priv;
	priv->queue.dev = &pdev->dev;
	priv->queue.timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	ret = vb2_queue_init(&priv->queue);
	if (ret)
		goto err_init;
	strscpy(priv->vdev.name, "Sanath-Capture", sizeof(priv->vdev.name));
	priv->vdev.fops = &sanath_vdev_fops;
	priv->vdev.ioctl_ops = &sanath_vdev_ioctl_ops;
	priv->vdev.v4l2_dev = &priv->v4l2_dev;
	priv->vdev.lock = &priv->lock;
	priv->vdev.queue = &priv->queue;
	priv->vdev.device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
	priv->vdev.release = video_device_release_empty;
	video_set_drvdata(&priv->vdev, priv);
	ret = video_register_device(&priv->vdev, VFL_TYPE_VIDEO, 50);
	if (ret)
		goto err_vb2;
	
	return 0;
err_vb2:
	vb2_queue_release(&priv->queue);
err_init:
	v4l2_device_unregister(&priv->v4l2_dev);
err_reg:
	return ret;
}
static int sanath_v4l2_remove(struct platform_device *pdev)
{
	struct sanath_capture_dev *priv = platform_get_drvdata(pdev);
	video_unregister_device(&priv->vdev);
	vb2_queue_release(&priv->queue);
	v4l2_device_unregister(&priv->v4l2_dev);
	return 0;
}
static const struct of_device_id sanath_v4l2_of_match[] = {
	{ .compatible = "sanath-capture-lab", },
	{ }
};
MODULE_DEVICE_TABLE(of, sanath_v4l2_of_match);
static struct platform_driver sanath_v4l2_driver = {
	.probe  = sanath_v4l2_probe,
	.remove = sanath_v4l2_remove,
	.driver = {
		.name           = "sanath-capture-lab",
		.of_match_table = sanath_v4l2_of_match,
	},
};
void timer_callback(struct timer_list * data)
{
	struct sanath_capture_dev *priv = container_of(data, struct sanath_capture_dev, etx_timer);
	struct sanath_buffer *buf;
	unsigned long flags;
	spin_lock_irqsave(&priv->buf_lock, flags);
	//list_for_each_entry_safe(buf, tmp, &priv->buf_list, list)
	if(!list_empty(&priv->buf_list))
	{
		buf = list_first_entry(&priv->buf_list, struct sanath_buffer, list);
		// Get the kernel virtual address for plane 0
		void *vaddr = vb2_plane_vaddr(&buf->vb, 0);
		memset(vaddr, priv->frame_count & 0xFF, 640*480*2);
		priv->frame_count++;
		list_del(&buf->list);
		vb2_buffer_done(&buf->vb, VB2_BUF_STATE_DONE);
	}
	spin_unlock_irqrestore(&priv->buf_lock, flags);
	mod_timer(&priv->etx_timer, jiffies + msecs_to_jiffies(TIMEOUT));
}
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
