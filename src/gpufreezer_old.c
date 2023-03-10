#include <linux/module.h>
#include <linux/slab.h>
#include <linux/usb.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Artem Sarkisov");
MODULE_VERSION("1.0");

// Wrapper for usb_device_id with added list_head field to track devices.
typedef struct int_usb_device
{
    struct usb_device_id dev_id;
    struct list_head list_node;
} int_usb_device_t;

bool is_gpu_down = false;

struct usb_device_id allowed_devs[] = {
    {USB_DEVICE(0x13fe, 0x4301)},  // 0x13fe, 0x4300
};

// Declare and init the head node of the linked list.
LIST_HEAD(connected_devices);

// Match device with device id.
static bool is_dev_matched(struct usb_device *dev, const struct usb_device_id *dev_id)
{
    // Check idVendor and idProduct, which are used.
    if (dev_id->idVendor != dev->descriptor.idVendor || dev_id->idProduct != dev->descriptor.idProduct)
    {
        return false;
    }

    return true;
}

// Match device id with device id.
static bool is_dev_id_matched(struct usb_device_id *new_dev_id, const struct usb_device_id *dev_id)
{
    // Check idVendor and idProduct, which are used.
    if (dev_id->idVendor != new_dev_id->idVendor || dev_id->idProduct != new_dev_id->idProduct)
    {
        return false;
    }

    return true;
}

// Check if device is in allowed devices list.
static bool *is_dev_allowed(struct usb_device_id *dev)
{
    unsigned long allowed_devs_len = sizeof(allowed_devs) / sizeof(struct usb_device_id);

    int i;
    for (i = 0; i < allowed_devs_len; i++)
    {
        if (is_dev_id_matched(dev, &allowed_devs[i]))
        {
            return true;
        }
    }

    return false;
}

// Check if changed device is acknowledged.
static int count_not_acked_devs(void)
{
    int_usb_device_t *temp;
    int count = 0;

    list_for_each_entry(temp, &connected_devices, list_node)
    {
        if (!is_dev_allowed(&temp->dev_id))
        {
            count++;
        }
    }

    return count;
}

// Add connected device to list of tracked devices.
static void add_int_usb_dev(struct usb_device *dev)
{
    int_usb_device_t *new_usb_device = (int_usb_device_t *)kmalloc(sizeof(int_usb_device_t), GFP_KERNEL);
    struct usb_device_id new_id = {USB_DEVICE(dev->descriptor.idVendor, dev->descriptor.idProduct)};
    new_usb_device->dev_id = new_id;
    list_add_tail(&new_usb_device->list_node, &connected_devices);
}

// Delete device from list of tracked devices.
static void delete_int_usb_dev(struct usb_device *dev)
{
    int_usb_device_t *device, *temp;
    list_for_each_entry_safe(device, temp, &connected_devices, list_node)
    {
        if (is_dev_matched(dev, &device->dev_id))
        {
            list_del(&device->list_node);
            kfree(device);
        }
    }
}

// Handler for USB insertion.
static void usb_dev_insert(struct usb_device *dev)
{
    printk(KERN_INFO "gpufreezer: device connected with PID '%d' and VID '%d'\n",
           dev->descriptor.idProduct, dev->descriptor.idVendor);
    add_int_usb_dev(dev);
    int not_acked_devs = count_not_acked_devs();

    if (!not_acked_devs)
    {
        printk(KERN_INFO "gpufreezer: there are no any not allowed devices connected, skipping GPU freezing\n");
    }
    else
    {
        if (!is_gpu_down)
        {
            printk(KERN_INFO "gpufreezer: there are %d not allowed devices connected, freezing GPU\n", not_acked_devs);

            char *argv[] = {"/sbin/modprobe", "-r", "nvidia_uvm", NULL};
            char *envp[] = {"HOME=/", "TERM=linux", "PATH=/sbin:/bin:/usr/sbin:/usr/bin", NULL};
            if (call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC > 0))
            {
                printk(KERN_WARNING "gpufreezer: unable to freeze GPU\n");
            }
            else
            {
                printk(KERN_INFO "gpufreezer: GPU is frozen\n");
                is_gpu_down = true;
            }
        }
    }
}

// Handler for USB removal.
static void usb_dev_remove(struct usb_device *dev)
{
    printk(KERN_INFO "gpufreezer: device disconnected with PID '%d' and VID '%d'\n",
           dev->descriptor.idProduct, dev->descriptor.idVendor);
    delete_int_usb_dev(dev);
    int not_acked_devs = count_not_acked_devs();

    if (not_acked_devs)
    {
        printk(KERN_INFO "gpufreezer: there are %d not allowed devices connected, nothing to do\n", not_acked_devs);
    }
    else
    {
        if (is_gpu_down)
        {
            printk(KERN_INFO "gpufreezer: every not allowed devices are disconnected, bringing GPU back\n");

            char *argv[] = {"/sbin/modprobe", "nvidia_uvm", NULL};
            char *envp[] = {"HOME=/", "TERM=linux", "PATH=/sbin:/bin:/usr/sbin:/usr/bin", NULL};
            if (call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC > 0))
            {
                printk(KERN_WARNING "gpufreezer: unable to activate GPU\n");
            }
            else
            {
                printk(KERN_INFO "gpufreezer: GPU is available now\n");
                is_gpu_down = false;
            }
        }
    }
}

// Handler for event's notifier.
static int notify(struct notifier_block *self, unsigned long action, void *dev)
{
    // Events, which our notifier react.
    switch (action)
    {
    case USB_DEVICE_ADD:
        usb_dev_insert(dev);
        break;
    case USB_DEVICE_REMOVE:
        usb_dev_remove(dev);
        break;
    default:
        break;
    }

    return 0;
}

// React on different notifies.
static struct notifier_block usb_notify = {
    .notifier_call = notify,
};

// Module init function.
static int __init gpufreezer_init(void)
{
    usb_register_notify(&usb_notify);
    printk(KERN_INFO "gpufreezer: module loaded\n");
    return 0;
}

// Module exit function.
static void __exit gpufreezer_exit(void)
{
    usb_unregister_notify(&usb_notify);
    printk(KERN_INFO "gpufreezer: module unloaded\n");
}

module_init(gpufreezer_init);
module_exit(gpufreezer_exit);