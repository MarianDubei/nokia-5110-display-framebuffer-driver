#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fb.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>

#define DEVICE_NAME     "nokia5110fb"

const int RST = 25;
const int CE = 8;
const int DC = 23;
const int DIN = 10;
const int CLK = 11;
//const int VCC = ;
const int BL = 24;
//const int GND = ;


static struct fb_info nokia5110fb_info;

static const struct fb_var_screeninfo nokia5110fb_defined = {
        .xres =		1024,
        .yres =		768,
        .xres_virtual =	1024,
        .yres_virtual =	768,
        .bits_per_pixel =8,
        .activate =	FB_ACTIVATE_NOW,
        .height =	-1,
        .width =	-1,
        .vmode =	FB_VMODE_NONINTERLACED,
};

static struct fb_fix_screeninfo nokia5110fb_fix __initdata = {
        .id =		"Nokia 5110",
        .smem_len =	(1024*768),
        .type =		FB_TYPE_PACKED_PIXELS,
        .visual =	FB_VISUAL_PSEUDOCOLOR,
        .line_length =	1024,
};

static const struct fb_ops nokia5110fb_ops = {
        .owner		    = THIS_MODULE,
        .fb_fillrect	= cfb_fillrect,
        .fb_copyarea	= cfb_copyarea,
        .fb_imageblit	= cfb_imageblit,
};

static int __init nokia5110fb_init(void)
{
    gpio_set_value(RST, 1);
    gpio_set_value(CE, 1);
    gpio_set_value(BL, 1);

    nokia5110fb_info.fbops = &nokia5110fb_ops;
//    nokia5110fb_info.screen_base = (char *)nokia5110fb_fix.smem_start;
    nokia5110fb_info.var = nokia5110fb_defined;
    nokia5110fb_info.fix = nokia5110fb_fix;
    nokia5110fb_info.flags = FBINFO_DEFAULT;

    fb_alloc_cmap(&nokia5110fb_info.cmap, 255, 0);

    if (register_framebuffer(&nokia5110fb_info) < 0)
        return 1;
    printk(KERN_INFO "[NOKIA5110FB] - Nokia 5110 Display Framebuffer Driver initialized\n");
    return 0;
}

static void __exit nokia5110fb_exit(void)
{
    unregister_framebuffer(&nokia5110fb_info);
    printk(KERN_INFO "[NOKIA5110FB] - Nokia 5110 Display Framebuffer Driver removed\n");
}

module_init(nokia5110fb_init);
module_exit(nokia5110fb_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Roman Okhrimenko <mrromanjoe@gmail.com>");
MODULE_DESCRIPTION("Nokia 5110 Display Framebuffer Driver - Linux device driver for Raspberry Pi");
