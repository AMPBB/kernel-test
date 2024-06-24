#include"mychar.h"


#include<linux/cdev.h>
#include<linux/fs.h>
#include<linux/device.h>
#include<linux/module.h>
#include<linux/delay.h>
#include<linux/workqueue.h>
#include<linux/slab.h>
#include<linux/kthread.h>

typedef struct mychar_dev_s {
    struct cdev cdev;
    struct class *class;
    int reserved;
} mychar_dev;

static mychar_dev md_realize;

static loff_t mychar_llseek(struct file* f, loff_t offset, int whence) {
    return offset+whence;
}

static ssize_t mychar_read(struct file* f, char __user *buf, size_t cnt, loff_t* poffset) {
    int ret;
    ret=copy_to_user(buf,"mychar",6);
    if(ret) {
        pr_err("%s,%d\n",__func__,__LINE__);
    }
    return 6;
}

static ssize_t mychar_write(struct file* f, const char __user* buf, size_t cnt, loff_t* poffset) {
    char tbuf[7];
    int ret;
    ret=copy_from_user(tbuf,buf,6);
    if(ret) {
        pr_err("%s,%d\n",__func__,__LINE__);
    }
    pr_info("%s,%d,%s\n",__func__,__LINE__,tbuf);
    return 6;
}

static int mychar_open(struct inode* pinode,struct file* f) {
    pr_info("%s,%d,open\n",__func__,__LINE__);
    return 0;
}

static int mychar_release(struct inode* pinode,struct file* f) {
    pr_info("%s,%d,release\n",__func__,__LINE__);
    return 0;
}

static struct file_operations mychar_fops = {
    .owner		= THIS_MODULE,
	.llseek		= mychar_llseek,
	.read		= mychar_read,
	.write		= mychar_write,
	.open		= mychar_open,
    .release    = mychar_release,
};

// static struct work_struct wq_work;
static struct workqueue_struct *wq_mychar;
typedef struct mychar_work_s {
    struct work_struct work;
    int data;
} mychar_work;
// static mychar_work mychar_wq_work;
static void wq_work_func(struct work_struct *work) {
    mychar_work *mw=NULL;
    mw=container_of(work,mychar_work,work); // need nulptr check
    pr_info("%s,%d,%d\n",__func__,__LINE__,mw->data);
    kfree(mw);
}

static ssize_t wq_test_store(struct device *dev, struct  device_attribute *attr, const char *buf, size_t size) {
    int d;
    sscanf(buf,"nu:%d\n",&d);
    pr_info("%s,%d,%s",__func__,__LINE__,buf);
    for(;d>0;--d) {
        mychar_work *mwt=NULL;
        mwt=(mychar_work*)kzalloc(sizeof(mychar_work)*1,GFP_KERNEL);
        mwt->data=d;
        INIT_WORK(&mwt->work,wq_work_func);
        queue_work(wq_mychar,&mwt->work);
    }
    return size;
}

static int kthread_test_func(void *data) {
    int cnt;
    while(!kthread_should_stop()) {
        // pr_info("%s,%d,%d",__func__,__LINE__,cnt);
        mdelay(100);
    }
    return 0;
}

static struct task_struct *kthread_test=NULL;
static ssize_t kthread_store(struct device *dev, struct  device_attribute *attr, const char *buf, size_t size) {
    int trigger;
    sscanf(buf,"tri:%d\n",&trigger);
    if(1==trigger) {
        if(NULL!=kthread_test) {
            pr_info("%s,%d,already created kthread!\n",__func__,__LINE__);
        } else {
            kthread_test = kthread_run(kthread_test_func,NULL,"kthread_mychar");
            if(NULL==kthread_test) {
                pr_err("%s,%d,kthread create failed!\n",__func__,__LINE__);
            }
        }
    } else if(0==trigger) {
        if(NULL==kthread_test) {
            pr_err("%s,%d,no kthread created!\n\n",__func__,__LINE__);
        } else {
            kthread_stop(kthread_test);
            pr_info("%s,%d,kthread already stop!\n",__func__,__LINE__);
        }
    } else {
        pr_err("%s,%d,tri:x\nx should be 0 or 1\n",__func__,__LINE__);
    }
    return size;
}

static DEVICE_ATTR(wq_test, S_IWUSR, NULL, wq_test_store);
static DEVICE_ATTR(kthread, S_IWUSR, NULL, kthread_store);
static struct attribute *mychar_attrs[] = {
    &dev_attr_wq_test.attr,
    &dev_attr_kthread.attr,
    NULL,
};
static const struct attribute_group mychar_attr_grp = {
    .attrs = mychar_attrs,
};

static int mychar_dev_no;
static struct device *pmychar_dev;
#define DEV_NAME "MyCharDev"
#define CLASS_NAME "MyCharClass"

static int __init mychar_init(void) {
    int ret;

    pr_info("%s,%d,enter\n",__func__,__LINE__);
    cdev_init(&md_realize.cdev,&mychar_fops); pr_info("%s,%d,cdev_init\n",__func__,__LINE__);
    md_realize.cdev.owner=THIS_MODULE;

    alloc_chrdev_region(&mychar_dev_no,0,1,DEV_NAME); pr_info("%s,%d,alloc_chrdev_region\n",__func__,__LINE__);
    ret=cdev_add(&md_realize.cdev,mychar_dev_no,1); pr_info("%s,%d,cdev_add\n",__func__,__LINE__);
    if(ret) {
        pr_err("%s,%d\n",__func__,__LINE__);
        return -ENODEV;
    }

    md_realize.class=class_create(THIS_MODULE,CLASS_NAME);pr_info("%s,%d,class_create\n",__func__,__LINE__);
    pmychar_dev=device_create(md_realize.class,NULL,mychar_dev_no,NULL,DEV_NAME); pr_info("%s,%d,device_create\n",__func__,__LINE__);

    ret=sysfs_create_group(&pmychar_dev->kobj,&mychar_attr_grp);
    if(ret) {
        pr_err("%s,%d\n",__func__,__LINE__);
        return -ENODEV;
    }

    wq_mychar=alloc_workqueue("wq_mychar_tag",0,0); //need nulptr check

    pr_info("%s,%d,exit\n",__func__,__LINE__);

    return ret;
}

static void __exit mychar_exit(void) {
    pr_info("%s,%d,enter\n",__func__,__LINE__);
    destroy_workqueue(wq_mychar);
    sysfs_remove_group(&pmychar_dev->kobj,&mychar_attr_grp);
    device_destroy(md_realize.class,mychar_dev_no); pr_info("%s,%d,device_destroy\n",__func__,__LINE__);
    class_destroy(md_realize.class); pr_info("%s,%d,class_destroy\n",__func__,__LINE__);
    cdev_del(&md_realize.cdev); pr_info("%s,%d,cdev_del\n",__func__,__LINE__);
    unregister_chrdev_region(mychar_dev_no,1); pr_info("%s,%d,unregister_chrdev_region\n",__func__,__LINE__);
    pr_info("%s,%d,exit\n",__func__,__LINE__);
}

module_init(mychar_init);
module_exit(mychar_exit);

MODULE_AUTHOR("ampbb <1942692994@qq.com>");
MODULE_DESCRIPTION("MyChar driver and device");
MODULE_VERSION("0.1");
MODULE_LICENSE("GPL");
