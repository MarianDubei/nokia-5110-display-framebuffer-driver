#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>

#define DEVICE_NAME     "digit_screen" /* name that will be assigned to this device in /dev fs */
#define BUF_SIZE        4096
#define NUM_COM         4 /* number of commands that this driver support */
#define MINORS_NUM      1


const int pins[] = {
        2, // segA
        3, // segB
        4, // segC
        17, // segD
        27, // segE
        22, // segF
        10, // segG
};
const int masks[] = {
//      A     B     C     D     E     F     G
        0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40,
};
const int digit_bitmap[] = {
//      0     1     2     3     4     5     6     7     8     9     A     B     C     D     E     F
        0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x67, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71,
};

static int digit_screen_open(struct inode *inode, struct file *file_pointer);

static int digit_screen_release(struct inode *inode, struct file *file_pointer);

static ssize_t digit_screen_read(struct file *file_pointer, char *buf, size_t count, loff_t *f_pos);

static ssize_t digit_screen_write(struct file *file_pointer, const char *buf, size_t count, loff_t *f_pos);


static struct file_operations digit_screen_fops = {
        .owner = THIS_MODULE,
        .open = digit_screen_open,
        .release = digit_screen_release,
        .read = digit_screen_read,
        .write = digit_screen_write,
};


struct digit_screen_dev {
    struct cdev cdev;
    char display_value;
};


static int digit_screen_init(void);

static void digit_screen_exit(void);


struct digit_screen_dev *digit_screen_dev_pointer;

static char *device_buffer = NULL;

static dev_t first;

static ssize_t display_value_show(struct class *cls, struct class_attribute *attr, char *buf){
    // TODO
    char *x[] = { "Enabled", "Disabled" };
    printk("In enable_show function\n");

    return sprintf(buf, "%s\n", x[0]);
};

static ssize_t display_value_store(struct class *cls, struct class_attribute *attr, const char *buf, size_t count){
    // TODO
    printk("In enable_store function\n");

    return 1;
};

static CLASS_ATTR_RW(display_value);

static struct attribute *class_attr_attrs[] = { &class_attr_display_value.attr, NULL };

ATTRIBUTE_GROUPS(class_attr);

static struct class digit_screen_class = {
    .name = DEVICE_NAME,
    .owner = THIS_MODULE,
    .class_groups = class_attr_groups,
};


void set_to_screen(char c[]) {
    // TODO

    int i;
//    int val = digit_bitmap[atoi(c[0]) - 48];
    int val = digit_bitmap[c[0]-48];
    printk(KERN_INFO
    "[DIGIT_SCREEN] - val - %d\n", val);
    int pins_len = sizeof pins / sizeof *pins;
    for (i = 0; i < pins_len; ++i) {
//        fp = file_open("/dev/gpio_lkm/GPIO%d", pins[i], O_WRONLY, 0);
        gpio_set_value(pins[i], 0);
    }
    int masks_len = sizeof masks / sizeof *masks;
    for ( i = 0; i < masks_len; ++i) {
        if ((masks[i] & val) == masks[i]) {
            printk(KERN_INFO "[DIGIT_SCREEN] - i - %d\n", i);
//            fp = file_open("/dev/gpio_lkm/GPIO%d", pins[i], O_WRONLY, 0);
            gpio_set_value(pins[i], 1);
        }
    }

}

static int digit_screen_open(struct inode *inode, struct file *file_pointer) {
    struct digit_screen_dev *digit_screen_dev_pointer;

    digit_screen_dev_pointer = container_of(inode->i_cdev, struct digit_screen_dev, cdev);

    file_pointer->private_data = digit_screen_dev_pointer;

    return 0;
}

static int digit_screen_release(struct inode *inode, struct file *file_pointer) {
    file_pointer->private_data = NULL;

    return 0;
}

static ssize_t digit_screen_read(struct file *file_pointer, char *buf, size_t count, loff_t *f_pos) {

    ssize_t retval;
    char byte;
    struct digit_screen_dev *digit_screen_dev_pointer = file_pointer->private_data;

    for (retval = 0; retval < count; ++retval) {
        byte = '0' + digit_screen_dev_pointer->display_value;

        if (put_user(byte, buf + retval))
            break;
    }
    return retval;
}

static ssize_t digit_screen_write(struct file *file_pointer, const char *buf, size_t count, loff_t *f_pos) {
    struct digit_screen_dev *digit_screen_dev_pointer = file_pointer->private_data;

    memset(device_buffer, 0, BUF_SIZE);
    if (copy_from_user(device_buffer, buf, count) != 0)
        return -EFAULT;
    device_buffer[count] = '\0';
    digit_screen_dev_pointer->display_value = device_buffer[0]-48;

    int i;
    for ( i=0; i<BUF_SIZE;i++){
        printk(KERN_INFO "[DIGIT_SCREEN] - val - %d\n", device_buffer[i]);
    }
    set_to_screen(device_buffer);

    f_pos += count;
    return count;

}


static int __init digit_screen_init(void)
{
    int ret = 0;

    if (alloc_chrdev_region(&first, 0, MINORS_NUM, DEVICE_NAME) < 0)
    {
        printk(KERN_DEBUG "Cannot register device\n");
        return -1;
    }

//    if ((digit_screen_class = class_create( THIS_MODULE, DEVICE_NAME)) == NULL)
    if (class_register(&digit_screen_class) < 0)
    {
        printk(KERN_DEBUG "Cannot create class %s\n", DEVICE_NAME);
        unregister_chrdev_region(first, MINORS_NUM);
        return -EINVAL;
    }

    digit_screen_dev_pointer = kmalloc(sizeof(struct digit_screen_dev), GFP_KERNEL);
    if (!digit_screen_dev_pointer)
    {
        printk(KERN_DEBUG "[DIGIT_SCREEN]Bad kmalloc\n");
        return -ENOMEM;
    }


    digit_screen_dev_pointer->display_value = '1'-48;
    digit_screen_dev_pointer->cdev.owner = THIS_MODULE;

    cdev_init(&digit_screen_dev_pointer->cdev, &digit_screen_fops);


    if ((ret = cdev_add( &digit_screen_dev_pointer->cdev, first, 1)))
    {
        printk (KERN_ALERT "[DIGIT_SCREEN] - Error %d adding cdev\n", ret);
        device_destroy (&digit_screen_class, first);
        class_destroy(&digit_screen_class);
        unregister_chrdev_region(first, MINORS_NUM);
        return ret;
    }

    if (device_create( &digit_screen_class, NULL, first, NULL, "digit_screen") == NULL)
    {
        class_destroy(&digit_screen_class);
        unregister_chrdev_region(first, MINORS_NUM);

        return -1;
    }

    device_buffer = (char *)kmalloc(BUF_SIZE, GFP_KERNEL);

    printk("[DIGIT_SCREEN] - Driver initialized\n");

    return 0;
}

static void __exit digit_screen_exit(void)
{
    unregister_chrdev_region(first, MINORS_NUM);

    kfree(digit_screen_dev_pointer);
    kfree(device_buffer);

    device_destroy ( &digit_screen_class, MKDEV(MAJOR(first), MINOR(first)));

    class_destroy(&digit_screen_class);

    printk(KERN_INFO "[DIGIT_SCREEN] - Raspberry Pi 7-segment driver removed\n");
}


module_init(digit_screen_init);
module_exit(digit_screen_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Roman Okhrimenko <mrromanjoe@gmail.com>");
MODULE_DESCRIPTION("GPIO Loadable Kernel Module - Linux device driver for Raspberry Pi");
