#include "uapi/linux/fb.h"
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/io.h>

#include <asm/div64.h>

static struct fb_ops myfb_ops = {
	.owner		= THIS_MODULE,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
};

static struct fb_info *myfb_info;

static unsigned int pseudo_palette[16];

int __init lcd_drc_init(void)
{
    dma_addr_t phy_addr;

    /* 1、分配 fb_info */
    myfb_info = framebuffer_alloc(0, NULL);
    /* 2、设置 fb_info */
    /* a. var: LCD分辨率、颜色格式 */
    myfb_info->var.xres = 1024;
    myfb_info->var.yres = 600;
    myfb_info->var.xres_virtual = myfb_info->var.xres;
    myfb_info->var.yres_virtual = myfb_info->var.yres;

    myfb_info->var.bits_per_pixel = 16;
    //rgb565
    myfb_info->var.red.offset = 11;
    myfb_info->var.red.length = 5;
    myfb_info->var.green.offset = 5;
    myfb_info->var.green.length = 6;
    myfb_info->var.blue.offset = 0;
    myfb_info->var.blue.length = 5;
    
    /* b. fix */
    strcpy(myfb_info->fix.id, "100ask_lcd");
    myfb_info->fix.type = FB_TYPE_PACKED_PIXELS;
    myfb_info->fix.visual = FB_VISUAL_TRUECOLOR;
    /* 显存长度 */
    myfb_info->fix.smem_len = myfb_info->var.xres * myfb_info->var.yres * myfb_info->var.bits_per_pixel / 8;
    if (myfb_info->var.bits_per_pixel == 24) {
        myfb_info->fix.smem_len = myfb_info->var.xres * myfb_info->var.yres * 4;
    }

    /* 申请显存内存 */
    myfb_info->screen_base = dma_alloc_writecombine(NULL, myfb_info->fix.smem_len,
						   &phy_addr, GFP_KERNEL);
    myfb_info->fix.smem_start = phy_addr;

    /* c. fbops */
    myfb_info->fbops = &myfb_ops;
    myfb_info->pseudo_palette = pseudo_palette;
    
    /* 3、注册 fb_info */
    register_framebuffer(myfb_info);
    

    return 0;
}

void __exit lcd_drc_exit(void)
{
    /* 1、注销 fb_info */
    unregister_framebuffer(myfb_info);

    /* 2、释放 fb_info */
	framebuffer_release(myfb_info);
}

/* 入口与出口 */
module_init(lcd_drc_init);
module_exit(lcd_drc_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("lighting master");