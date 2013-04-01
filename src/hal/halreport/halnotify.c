#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include "halnotify.h"

#ifdef USE_RTAI
#include "rtai.h"

// we cant call sysfs_notify() from an RT thread directly
// so queue a service request which gets executed as the kernel resumes from RT
static  int srq_for_notify;
#endif

#define AUTHOR "Michael Haberler <git@mah.priv.at>"
#define DESCRIPTION "'halnotify' event notifier"
#define VERSION "0.1"
#define HAL_NOTIFY "halnotify"

#define dbg(format, arg...) do { \
        if (debug) pr_info(HAL_NOTIFY ": %s: " format , __FUNCTION__ , ## arg); \
    } while (0)
#define err(format, arg...) pr_err(HAL_NOTIFY ": " format, ## arg)
#define info(format, arg...) pr_info(HAL_NOTIFY ": " format, ## arg)
#ifndef pr_warn
#define pr_warn pr_warning
#endif
#define warn(format, arg...) pr_warn(HAL_NOTIFY ": " format, ## arg)

static bool debug = false;
module_param(debug, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "enable debug info (default: false)");

struct halnotify_attr {
    struct attribute attr;
    atomic_t  value;
};

static struct halnotify_attr mask_attr = {
    .attr.name="mask",
    .attr.mode = 0644,
    .value = ATOMIC_INIT(0)
};

static struct halnotify_attr count_attr = {
    .attr.name="count",
    .attr.mode = 0644,
    .value = ATOMIC_INIT(0)
};

static struct halnotify_attr changed_attr = {
    .attr.name="changed",
    .attr.mode = 0644,
    .value = ATOMIC_INIT(0)
};

static struct attribute * halnotify_attr[] = {
    &mask_attr.attr,
    &count_attr.attr,
    &changed_attr.attr,
    NULL
};

static ssize_t attr_show(struct kobject *kobj, struct attribute *attr,
			 char *buf)
{
    struct halnotify_attr *ha = container_of(attr, struct halnotify_attr, attr);

    dbg("%s", ha->attr.name);
    return scnprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&ha->value));
}

static ssize_t attr_store(struct kobject *kobj, struct attribute *attr,
        const char *buf, size_t len)
{
    struct halnotify_attr *ha = container_of(attr, struct halnotify_attr, attr);
    int new;
    sscanf(buf, "%d", &new);

    dbg("%s %d", ha->attr.name, new);

    sysfs_notify(kobj,NULL, ha->attr.name);
    atomic_set(&ha->value,new);
    return sizeof(int);
}

static struct kobject *ha_kobj;

static struct sysfs_ops notify_ops = {
    .show = attr_show,
    .store = attr_store
};

static struct kobj_type mytype = {
    .sysfs_ops = &notify_ops,
    .default_attrs = halnotify_attr,
};

static  void halnotify_service(void)
{
    sysfs_notify(ha_kobj, NULL, "mask");
}

static int __init halnotify_init(void)
{
    int err = -1;

    dbg("");
    ha_kobj = kzalloc(sizeof(*ha_kobj), GFP_KERNEL);
    if (ha_kobj) {
        kobject_init(ha_kobj, &mytype);
        if (kobject_add(ha_kobj, NULL, "%s", HAL_NOTIFY)) {
             err = -1;
             err("kobject_add failed\n");
             kobject_put(ha_kobj);
             ha_kobj = NULL;
        }
        err = 0;
    }
#ifdef USE_RTAI
    srq_for_notify = rt_request_srq (0, halnotify_service, 0);
    dbg("using %d for srq_for_notify",srq_for_notify);
#endif
    return err;
}

static void __exit halnotify_exit(void)
{
    dbg("");
    if (ha_kobj) {
        kobject_put(ha_kobj);
        kfree(ha_kobj);
    }
#ifdef USE_RTAI
    int retval = rt_free_srq (srq_for_notify);
    if (retval)
	rtapi_print_msg(RTAPI_MSG_ERR,"rt_free_srq(%x) failed: %d",
			srq_for_notify, retval);
#endif
}

int hal_notify(int rt, int mask)
{
    int changed, prev;

    atomic_inc(&count_attr.value);

    // see if any new bits set
    prev = atomic_read(&mask_attr.value);
    changed = mask & ~prev;

    if (changed) {
	atomic_set(&mask_attr.value, mask | prev);
	atomic_inc(&changed_attr.value);
	if (rt) {
#ifdef USE_RTAI
	    rt_pend_linux_srq (srq_for_notify);
#endif
	} else {
	    halnotify_service();
	}
	return changed; // the bits which were set
    }
    return 0; // no change
}

EXPORT_SYMBOL(hal_notify);
module_init(halnotify_init);
module_exit(halnotify_exit);
MODULE_LICENSE("GPL");
