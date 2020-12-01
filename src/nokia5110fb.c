#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fb.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

#define DEVICE_NAME     "nokia5110fb"
#define BUFSIZE 4096
#define LCDWIDTH 84
#define LCDHEIGHT 48

#define PCD8544_POWERDOWN 0x04
#define PCD8544_ENTRYMODE 0x02
#define PCD8544_EXTENDEDINSTRUCTION 0x01

#define PCD8544_DISPLAYBLANK 0x0
#define PCD8544_DISPLAYNORMAL 0x4
#define PCD8544_DISPLAYALLON 0x1
#define PCD8544_DISPLAYINVERTED 0x5

// H = 0
#define PCD8544_FUNCTIONSET 0x20
#define PCD8544_DISPLAYCONTROL 0x08
#define PCD8544_SETYADDR 0x40
#define PCD8544_SETXADDR 0x80

// H = 1
#define PCD8544_SETTEMP 0x04
#define PCD8544_SETBIAS 0x10
#define PCD8544_SETVOP 0x80

int pcd8544_buffer[LCDWIDTH * LCDHEIGHT / 8] = {0,};

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
        .xres =		84,
        .yres =		48,
        .xres_virtual =	84,
        .yres_virtual =	48,
        .bits_per_pixel =1,
        .activate =	FB_ACTIVATE_NOW,
        .height =	-1,
        .width =	-1,
        .vmode =	FB_VMODE_NONINTERLACED,
};

static struct fb_fix_screeninfo nokia5110fb_fix __initdata = {
        .id =		"Nokia 5110",
        .smem_len =	(84*6),
        .type =		FB_TYPE_PACKED_PIXELS,
        .visual =	FB_VISUAL_PSEUDOCOLOR,
        .line_length =	1024,
};

static const struct fb_ops nokia5110fb_ops = {
        .owner = THIS_MODULE,
        .read = nokia5110fb_read, // read operation
        .write = nokia5110fb_write, // write operation
//        .ioctl = nokia5110fb_ioctl, //control operation
        .mmap = nokia5110fb_mmap, // mapping operation
        .open = nokia5110fb_open, // open operation
        .release = nokia5110fb_release, // close operation
};

void shiftOut(int val)
{
    int i;
    int j;
    for (i = 0; i < 8; i++)  {
        gpio_set_value(DIN, !!(val & (1 << (7 - i))));

        gpio_set_value(CLK, 1);
        for (j = 400; j > 0; j--); // clock speed, anyone? (LCD Max CLK input: 4MHz)
        gpio_set_value(CLK, 0);
    }
}

void LCDspiwrite(int c)
{
    gpio_set_value(CE, 0);
    shiftOut(c);
    gpio_set_value(CE, 1);
}

void LCDcommand(int c)
{
    gpio_set_value( DC, 0);
    LCDspiwrite(c);
}

void LCDdata(int c)
{
    gpio_set_value(DC, 1);
    LCDspiwrite(c);
}

void LCDdisplay(void)
{
    int col, maxcol, p;

    for(p = 0; p < 6; p++)
    {
        LCDcommand(PCD8544_SETYADDR | p);
        // start at the beginning of the row
        col = 0;
        maxcol = LCDWIDTH-1;
        LCDcommand(PCD8544_SETXADDR | col);
        for(; col <= maxcol; col++) {
            //uart_putw_dec(col);
            //uart_putchar(' ');
            LCDdata(pcd8544_buffer[(LCDWIDTH*p)+col]);
        }
    }
    LCDcommand(PCD8544_SETYADDR );  // no idea why this is necessary but it is to finish the last byte?

}

void LCDshowbuffer(void)
{
//    int i;
//    for (i = 0; i < LCDWIDTH * LCDHEIGHT / 8; i++  )
//        pcd8544_buffer[i] = pi_logo[i];
    LCDdisplay();
}

void nokia5110_setup(void)
{
    gpio_direction_output(RST, 0);
    gpio_direction_output(CE, 0);
    gpio_direction_output(BL, 0);
    gpio_direction_output(DC, 0);
    gpio_direction_output(DIN, 0);
    gpio_direction_output(CLK, 0);
    gpio_set_value(RST, 0);
    int j;
    for (j = 400; j > 0; j--);
    gpio_set_value(RST, 1);
    gpio_set_value(CE, 1);
    gpio_set_value(BL, 1);

    // get into the EXTENDED mode!
    LCDcommand(PCD8544_FUNCTIONSET | PCD8544_EXTENDEDINSTRUCTION );

    // LCD bias select (4 is optimal?)
    LCDcommand(PCD8544_SETBIAS | 0x4);

    // set VOP
    int contrast = 55;
    if (contrast > 0x7f)
        contrast = 0x7f;

    LCDcommand( PCD8544_SETVOP | contrast); // Experimentally determined

    // normal mode
    LCDcommand(PCD8544_FUNCTIONSET);

    // Set display to Normal
    LCDcommand(PCD8544_DISPLAYCONTROL | PCD8544_DISPLAYNORMAL);

    // set up a bounding box for screen updates

    LCDshowbuffer();
}

static int nokia5110fb_read(struct file *file_pointer, char *buf, size_t count, loff_t *f_pos) {

    int retval;
    char byte;
    struct fb_info *nokia5110fb_info = file_pointer->private_data;

    for (retval = 0; retval < 84*48; ++retval) {
        *(pcd8544_buffer+BUFSIZE*retval) = nokia5110fb_info->fix->smem_start;
    }
    return retval;
}

static ssize_t nokia5110fb_write(struct file *file_pointer, const char *buf, size_t count, loff_t *f_pos) {
    struct fb_info *nokia5110fb_info = file_pointer->private_data;

    memset(pcd8544_buffer, 0, BUF_SIZE);
    if (copy_from_user(pcd8544_buffer, buf, count) != 0)
        return -EFAULT;
    pcd8544_buffer[count] = '\0';

    LCDdisplay();
    f_pos += count;
    return count;
}

static int nokia5110fb_open(struct inode *inode, struct file *file_pointer) {
    static struct fb_info nokia5110fb_info;
    return 0;
}


static int nokia5110fb_release(struct inode *inode, struct file *file_pointer) {
    nokia5110fb_info = NULL;
    return 0;
}

static int __init nokia5110fb_init(void)
{
    nokia5110_setup();

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
MODULE_AUTHOR("Dubei Marian, Rumezhak Taras");
MODULE_DESCRIPTION("Nokia 5110 Display Framebuffer Driver - Linux device driver for Raspberry Pi");
