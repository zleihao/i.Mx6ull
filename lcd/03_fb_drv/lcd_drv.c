#include "linux/device.h"
#include "linux/gpio/consumer.h"
#include "linux/init.h"
#include "linux/of.h"
#include "linux/printk.h"
#include "linux/slab.h"
#include <asm-generic/errno-base.h>
#include <linux/busfreq-imx.h>
#include <linux/console.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/pinctrl/consumer.h>
#include <linux/fb.h>
#include <linux/mxcfb.h>
#include <linux/regulator/consumer.h>
#include <video/of_display_timing.h>
#include <video/videomode.h>
#include <linux/uaccess.h>

/*****************************硬件相关的*****************************************/
struct imx6ull_lcdif {
  volatile unsigned int CTRL;                              
  volatile unsigned int CTRL_SET;                        
  volatile unsigned int CTRL_CLR;                         
  volatile unsigned int CTRL_TOG;                         
  volatile unsigned int CTRL1;                             
  volatile unsigned int CTRL1_SET;                         
  volatile unsigned int CTRL1_CLR;                       
  volatile unsigned int CTRL1_TOG;                       
  volatile unsigned int CTRL2;                            
  volatile unsigned int CTRL2_SET;                       
  volatile unsigned int CTRL2_CLR;                        
  volatile unsigned int CTRL2_TOG;                        
  volatile unsigned int TRANSFER_COUNT;   
       unsigned char RESERVED_0[12];
  volatile unsigned int CUR_BUF;                          
       unsigned char RESERVED_1[12];
  volatile unsigned int NEXT_BUF;                        
       unsigned char RESERVED_2[12];
  volatile unsigned int TIMING;                          
       unsigned char RESERVED_3[12];
  volatile unsigned int VDCTRL0;                         
  volatile unsigned int VDCTRL0_SET;                      
  volatile unsigned int VDCTRL0_CLR;                     
  volatile unsigned int VDCTRL0_TOG;                     
  volatile unsigned int VDCTRL1;                          
       unsigned char RESERVED_4[12];
  volatile unsigned int VDCTRL2;                          
       unsigned char RESERVED_5[12];
  volatile unsigned int VDCTRL3;                          
       unsigned char RESERVED_6[12];
  volatile unsigned int VDCTRL4;                           
       unsigned char RESERVED_7[12];
  volatile unsigned int DVICTRL0;    
  	   unsigned char RESERVED_8[12];
  volatile unsigned int DVICTRL1;                         
       unsigned char RESERVED_9[12];
  volatile unsigned int DVICTRL2;                        
       unsigned char RESERVED_10[12];
  volatile unsigned int DVICTRL3;                        
       unsigned char RESERVED_11[12];
  volatile unsigned int DVICTRL4;                          
       unsigned char RESERVED_12[12];
  volatile unsigned int CSC_COEFF0;  
  	   unsigned char RESERVED_13[12];
  volatile unsigned int CSC_COEFF1;                        
       unsigned char RESERVED_14[12];
  volatile unsigned int CSC_COEFF2;                        
       unsigned char RESERVED_15[12];
  volatile unsigned int CSC_COEFF3;                        
       unsigned char RESERVED_16[12];
  volatile unsigned int CSC_COEFF4;   
  	   unsigned char RESERVED_17[12];
  volatile unsigned int CSC_OFFSET;  
       unsigned char RESERVED_18[12];
  volatile unsigned int CSC_LIMIT;  
       unsigned char RESERVED_19[12];
  volatile unsigned int DATA;                              
       unsigned char RESERVED_20[12];
  volatile unsigned int BM_ERROR_STAT;                     
       unsigned char RESERVED_21[12];
  volatile unsigned int CRC_STAT;                        
       unsigned char RESERVED_22[12];
  volatile  unsigned int STAT;                             
       unsigned char RESERVED_23[76];
  volatile unsigned int THRES;                             
       unsigned char RESERVED_24[12];
  volatile unsigned int AS_CTRL;                           
       unsigned char RESERVED_25[12];
  volatile unsigned int AS_BUF;                            
       unsigned char RESERVED_26[12];
  volatile unsigned int AS_NEXT_BUF;                     
       unsigned char RESERVED_27[12];
  volatile unsigned int AS_CLRKEYLOW;                    
       unsigned char RESERVED_28[12];
  volatile unsigned int AS_CLRKEYHIGH;                   
       unsigned char RESERVED_29[12];
  volatile unsigned int SYNC_DELAY;                      
};

static void lcd_controller_enable(struct imx6ull_lcdif *lcdif)
{
	lcdif->CTRL |= (1<<0);
}

static int lcd_controller_init(struct imx6ull_lcdif *lcdif, struct display_timing *dt, int lcd_bpp, int fb_bpp, unsigned int fb_phy)
{
	int lcd_data_bus_width;
	int fb_width;
	int vsync_pol = 0;
	int hsync_pol = 0;
	int de_pol = 0;
	int clk_pol = 0;

	if (dt->flags & DISPLAY_FLAGS_HSYNC_HIGH)
		hsync_pol = 1;
	if (dt->flags & DISPLAY_FLAGS_VSYNC_HIGH)
		vsync_pol = 1;
	if (dt->flags & DISPLAY_FLAGS_DE_HIGH)
		de_pol = 1;
	if (dt->flags & DISPLAY_FLAGS_PIXDATA_POSEDGE)
		clk_pol = 1;
	
	if (lcd_bpp == 24)
		lcd_data_bus_width = 0x3;
	else if (lcd_bpp == 18)
		lcd_data_bus_width = 0x2;
	else if (lcd_bpp == 8)
		lcd_data_bus_width = 0x1;
	else if (lcd_bpp == 16)
		lcd_data_bus_width = 0x0;
	else
		return -1;

	if (fb_bpp == 24 || fb_bpp == 32)
		fb_width = 0x3;
	else if (fb_bpp == 18)
		fb_width = 0x2;
	else if (fb_bpp == 8)
		fb_width = 0x1;
	else if (fb_bpp == 16)
		fb_width = 0x0;
	else
		return -1;

	/* 
     * 初始化LCD控制器的CTRL寄存器
     * [19]       :  1      : DOTCLK和DVI modes需要设置为1 
     * [17]       :  1      : 设置为1工作在DOTCLK模式
     * [15:14]    : 00      : 输入数据不交换（小端模式）默认就为0，不需设置
     * [13:12]    : 00      : CSC数据不交换（小端模式）默认就为0，不需设置
     * [11:10]    : 11		: 数据总线为24bit
     * [9:8]    根据显示屏资源文件bpp来设置：8位0x1 ， 16位0x0 ，24位0x3
     * [5]        :  1      : 设置elcdif工作在主机模式
     * [1]        :  0      : 24位数据均是有效数据，默认就为0，不需设置
	 */	
	lcdif->CTRL = (0<<30) | (0<<29) | (0<<28) | (1<<19) | (1<<17) | (lcd_data_bus_width << 10) |\
	              (fb_width << 8) | (1<<5);

	/*
	* 设置ELCDIF的寄存器CTRL1
	* 根据bpp设置，bpp为24或32才设置
	* [19:16]  : 111  :表示ARGB传输格式模式下，传输24位无压缩数据，A通道不用传输）
	*/	  
	if(fb_bpp == 24 || fb_bpp == 32)
	{	  
		  lcdif->CTRL1 &= ~(0xf << 16); 
		  lcdif->CTRL1 |=  (0x7 << 16); 
	}
	else
		lcdif->CTRL1 |= (0xf << 16); 
	  
	/*
	* 设置ELCDIF的寄存器TRANSFER_COUNT寄存器
	* [31:16]  : 垂直方向上的像素个数  
	* [15:0]   : 水平方向上的像素个数
	*/
	lcdif->TRANSFER_COUNT  = (dt->vactive.typ << 16) | (dt->hactive.typ << 0);

	/*
	* 设置ELCDIF的VDCTRL0寄存器
	* [29] 0 : VSYNC输出  ，默认为0，无需设置
	* [28] 1 : 在DOTCLK模式下，设置1硬件会产生使能ENABLE输出
	* [27] 0 : VSYNC低电平有效	,根据屏幕配置文件将其设置为0
	* [26] 0 : HSYNC低电平有效 , 根据屏幕配置文件将其设置为0
	* [25] 1 : DOTCLK下降沿有效 ，根据屏幕配置文件将其设置为1
	* [24] 1 : ENABLE信号高电平有效，根据屏幕配置文件将其设置为1
	* [21] 1 : 帧同步周期单位，DOTCLK mode设置为1
	* [20] 1 : 帧同步脉冲宽度单位，DOTCLK mode设置为1
	* [17:0] :  vysnc脉冲宽度 
	*/
	  lcdif->VDCTRL0 = (1 << 28)|( vsync_pol << 27)\
					  |( hsync_pol << 26)\
					  |( clk_pol << 25)\
					  |( de_pol << 24)\
					  |(1 << 21)|(1 << 20)|( dt->vsync_len.typ << 0);

	/*
	* 设置ELCDIF的VDCTRL1寄存器
	* 设置垂直方向的总周期:上黑框tvb+垂直同步脉冲tvp+垂直有效高度yres+下黑框tvf
	*/	  
	lcdif->VDCTRL1 = dt->vback_porch.typ + dt->vsync_len.typ + dt->vactive.typ + dt->vfront_porch.typ;  

	/*
	* 设置ELCDIF的VDCTRL2寄存器
	* [18:31]  : 水平同步信号脉冲宽度
	* [17: 0]   : 水平方向总周期
	* 设置水平方向的总周期:左黑框thb+水平同步脉冲thp+水平有效高度xres+右黑框thf
	*/ 

	lcdif->VDCTRL2 = (dt->hsync_len.typ << 18) | (dt->hback_porch.typ + dt->hsync_len.typ + dt->hactive.typ + dt->hfront_porch.typ);

	/*
	* 设置ELCDIF的VDCTRL3寄存器
	* [27:16] ：水平方向上的等待时钟数 =thb + thp
	* [15:0]  : 垂直方向上的等待时钟数 = tvb + tvp
	*/ 

	lcdif->VDCTRL3 = ((dt->hback_porch.typ + dt->hsync_len.typ) << 16) | (dt->vback_porch.typ + dt->vsync_len.typ);

	/*
	* 设置ELCDIF的VDCTRL4寄存器
	* [18]	   使用VSHYNC、HSYNC、DOTCLK模式此为置1
	* [17:0]  : 水平方向的宽度
	*/ 

	lcdif->VDCTRL4 = (1<<18) | (dt->hactive.typ);

	/*
	* 设置ELCDIF的CUR_BUF和NEXT_BUF寄存器
	* CUR_BUF	 :	当前显存地址
	* NEXT_BUF :	下一帧显存地址
	* 方便运算，都设置为同一个显存地址
	*/ 

	lcdif->CUR_BUF  =  fb_phy;
	lcdif->NEXT_BUF =  fb_phy;

     return 0;
}



/******************************************************************************/

/* mylcd 相关操作变量 */
struct mylcd_driver_info {
	struct gpio_desc *bl_gpio;
	struct fb_info *fb_info;
	struct platform_device *pdev;
	struct display_timing *dt;

	struct clk *clk_pix;
	struct clk *clk_axi;
	dma_addr_t phy_addr;
	u32 bits_per_pixel;
	unsigned int ld_intf_width;
};

/*该结构体为全局的*/
static struct fb_ops myfb_ops = {
	.owner		= THIS_MODULE,
  /* 下面三个函数驱动已经写好 */
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
};

/* 从设备树的到lcd参数 */
int mylcd_init_fbinfo_dt(struct mylcd_driver_info *host)
{
	int ret = 0;
	struct device_node *display_np;
	struct platform_device *pdev = host->pdev;
	struct device_node *np = host->pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	struct display_timings *timings = NULL;
	u32 width;

	/* 得到display节点 */
	display_np = of_parse_phandle(np, "display", 0);
	if (!display_np) {
		dev_err(dev, "failed to find display phandle\n");
		return -ENOENT;
	}

	/* 获取 bus-width */
	ret = of_property_read_u32(display_np, "bus-width", &width);
	if (ret < 0) {
		dev_err(dev, "failed to get property bus-width\n");
		goto put_display_node;
	}

	/* 获取 bits-per-pixel*/
	ret = of_property_read_u32(display_np, "bits-per-pixel",
				   &host->bits_per_pixel);
	if (ret < 0) {
		dev_err(dev, "failed to get property bits-per-pixel\n");
		goto put_display_node;
	}

	timings = of_get_display_timings(display_np);
	if (!timings) {
		dev_err(dev, "failed to get display timings\n");
		ret = -ENOENT;
		goto put_display_node;
	}
	host->dt = timings->timings[timings->native_mode];
	
	return 0;

put_display_node:
	of_node_put(display_np);
	return ret;
}

int mylcd_init_fbinfo(struct mylcd_driver_info *host)
{
	int err = 0;
	struct fb_info *myfb_info = host->fb_info;
	struct device *dev = &host->pdev->dev;
	
	/* 得到lcd相关的参数 */
	err = mylcd_init_fbinfo_dt(host);
	if (err != 0) {
		return err;
	}

	/* 初始化fb_info */

	/* 无需从设备树获取的值 */
	myfb_info->fix.type = FB_TYPE_PACKED_PIXELS;
	myfb_info->fix.visual = FB_VISUAL_TRUECOLOR;

	/* 一个像素点的位宽 */
	myfb_info->var.bits_per_pixel = 16;
	/* 以rgb565为例设置RGB的位置与控制bit宽度 */
	myfb_info->var.red.offset = 11;
	myfb_info->var.red.length = 5;
	myfb_info->var.green.offset = 5;
	myfb_info->var.green.length = 6;
	myfb_info->var.blue.offset = 0;
	myfb_info->var.blue.length = 5;

	/* 根据设备树改变而改变的值 */
	myfb_info->var.xres = host->dt->hactive.typ;
	myfb_info->var.yres = host->dt->vactive.typ;
	myfb_info->var.xres_virtual = myfb_info->var.xres;
	myfb_info->var.yres_virtual = myfb_info->var.yres;

	/* 显存长度 */
	myfb_info->fix.smem_len = myfb_info->var.xres * myfb_info->var.yres * myfb_info->var.bits_per_pixel / 8;
	//当一个像素点的位宽等于24时需要重新设置
	if (myfb_info->var.bits_per_pixel == 24) {
		myfb_info->fix.smem_len = myfb_info->var.xres * myfb_info->var.yres * 4;
	}

	/* 申请显存内存 */
	myfb_info->screen_base = dma_alloc_writecombine(NULL, myfb_info->fix.smem_len, &host->phy_addr, GFP_KERNEL);
	if (myfb_info->screen_base == 0) {
		dev_err(dev, "Unable to allocate framebuffer memory\n");
		myfb_info->fix.smem_len = 0;
		myfb_info->fix.smem_start = 0;
		return -EBUSY;
	}
	myfb_info->fix.smem_start = host->phy_addr;

	//当一个像素点的位宽等于24时需要重新设置
	myfb_info->fix.line_length = myfb_info->var.xres * myfb_info->var.bits_per_pixel / 8;
	if (myfb_info->var.bits_per_pixel == 24)
		myfb_info->fix.line_length = myfb_info->var.xres * 4;

	/* 初始化fops */
	myfb_info->fbops = &myfb_ops;
 
	return 0;
}

/* mylcd platform */
static const struct of_device_id mylcd_dt_ids[] = {
	{ .compatible = "100ask,lcd_drv", },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, mylcd_dt_ids);

static int mylcd_probe(struct platform_device *pdev)
{
	int err = 0;
	struct mylcd_driver_info *host;
	struct fb_info *fb_info;
	struct resource *res;
	struct imx6ull_lcdif *lcdif;
	
	host = devm_kzalloc(&pdev->dev, sizeof(struct mylcd_driver_info), GFP_KERNEL);
	if (!host) {
		dev_err(&pdev->dev, "Failed to allocate IO resource\n");
		return -ENOMEM;
	}

	/* 从设备树得到背光灯gpio */
	host->bl_gpio = devm_gpiod_get(&pdev->dev, "backlight", 0);
	if (IS_ERR(host->bl_gpio)) {
		return PTR_ERR(host->bl_gpio);
	}
	
	/* 设置方向为输出 */
	err = gpiod_direction_output(host->bl_gpio, 1);
	if (err) {
		err = -EPERM;
		goto FAIL_SET_GPIO_DIR;
	}
	
	/* 分配 fb_info */
	fb_info = framebuffer_alloc(sizeof(struct fb_info), &pdev->dev);
	if (!fb_info) {
		dev_err(&pdev->dev, "Failed to allocate fbdev\n");
		err = -ENOMEM;
		goto FAIL_SET_GPIO_DIR;
	}
	
	host->fb_info = fb_info;
	host->pdev = pdev;
	/* 设置私有变量 */
	platform_set_drvdata(pdev, host);

	/* 得到时钟 */
	host->clk_pix = devm_clk_get(&pdev->dev, "pix");
	if (IS_ERR(host->clk_pix)) {
		host->clk_pix = NULL;
		err = PTR_ERR(host->clk_pix);
		goto FAIL_FB_RELEASE;
	}

	host->clk_axi = devm_clk_get(&pdev->dev, "axi");
	if (IS_ERR(host->clk_axi)) {
		host->clk_axi = NULL;
		err = PTR_ERR(host->clk_axi);
		goto FAIL_FB_RELEASE;
	}

	/* 必须含有 */
	fb_info->pseudo_palette = devm_kzalloc(&pdev->dev, sizeof(u32) * 16,
					       GFP_KERNEL);
	if (!fb_info->pseudo_palette) {
		err = -ENOMEM;
		goto FAIL_FB_RELEASE;
	}

	/* 初始化fb_info */
	err = mylcd_init_fbinfo(host);
	if (err != 0) {
		goto FAIL_FB_INIT;
	}

	/* 设置速率 */
	clk_set_rate(host->clk_pix, host->dt->pixelclock.typ);

	/* 使能pix */
	clk_prepare_enable(host->clk_pix);
	clk_prepare_enable(host->clk_axi);

	/* 注册fb_info */
	register_framebuffer(host->fb_info);

	/* 硬件操作 */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Cannot get memory IO resource\n");
		err = -ENODEV;
		goto FAIL_IORESOURCE_MEM;
	}
	lcdif = devm_ioremap_resource(&pdev->dev, res);
	
	lcd_controller_init(lcdif, host->dt, host->bits_per_pixel, 16, host->phy_addr);

	/* 使能lcd控制器 */
	lcd_controller_enable(lcdif);

	/* 打开背光 */
	gpiod_set_value(host->bl_gpio, 1);

	return 0;

FAIL_IORESOURCE_MEM:
	unregister_framebuffer(fb_info);
FAIL_FB_INIT:
	devm_kfree(&pdev->dev, fb_info->pseudo_palette);
FAIL_FB_RELEASE:
	framebuffer_release(fb_info);
FAIL_SET_GPIO_DIR:
	devm_kfree(&pdev->dev, host);
	return err;
}

static int mylcd_remove(struct platform_device *pdev)
{
	int ret = 0;
	struct mylcd_driver_info *host = platform_get_drvdata(pdev);
	struct fb_info *fb_info = host->fb_info;

	unregister_framebuffer(fb_info);

	platform_set_drvdata(pdev, NULL);

	devm_kfree(&pdev->dev, fb_info->pseudo_palette);
	framebuffer_release(fb_info);
	devm_kfree(&pdev->dev, host);

	/* 卸载fb_info */
	return ret;
}

static struct platform_driver mylcd_driver = {
	.probe = mylcd_probe,
	.remove = mylcd_remove,
	.driver = {
		   .name = "mylcd",
		   .of_match_table = mylcd_dt_ids,
	},
};


/* 入口 */
int __init mylcd_driver_init(void)
{
	return platform_driver_register(&mylcd_driver);
}

/* 出口 */
void __exit mylcd_driver_exit(void)
{
	platform_driver_unregister(&mylcd_driver);
}

/* 注册 */
module_init(mylcd_driver_init);
module_exit(mylcd_driver_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("lighting master");