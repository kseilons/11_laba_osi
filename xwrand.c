#include <linux/module.h> // required by all modules
#include <linux/kernel.h> // required for sysinfo
#include <linux/init.h> // used by modul_init, module_exit matcros
#include <linux/jiffies.h> // where jiffies and its helpers reside
//#include <linux/kthread.h> kernel threads
#include <linux/slab.h> 
//#include <linux/sched.h>
//#include <linux/workqueue.h> // work queue
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h> //macro used to work with
#include <linux/uaccess.h> //userspace 

MODULE_DESCRIPTION("Xorwow Pseudo-Random Generator Demo");
MODULE_AUTHOR("kseilons");
MODULE_VERSION("0.1");
MODULE_LICENSE("Dual MIT/GPL");

static char *seed = "452764634:706985783:2521395330:1263432680:2960490940:268079354";
module_param(seed, charp, 0440);
MODULE_PARM_DESC(seed, "Xorwow initial state in form of string \"a:b:c:d:e:cnt\", "
		       "where a ... cnt are 32-bit unsigned integer values");


static int major  = 0;
module_param(major, int, 0);
MODULE_PARM_DESC(major, "Character device major number or 0 for automatic allocation");

static const int minor = 0;
static const char devname[] = "xwrand";
static const size_t buf_sz = 4 * 1024;
static dev_t xw_dev = 0; //Used to our chardev
static struct cdev xw_cdev; //Maps device to file_operations, ...

struct xwrand_state {
	u32 a, b, c, d, e;
	u32 cnt;
};

typedef struct xwrand_state xwrand_t;



static uint32_t xwrand(xwrand_t *state) {
	u32 t = state-> e;
	u32 s = state->a;
	state->e = state->d;
	state->d = state->c;
	state->c = state->b;
	state->d = s;
	t ^= t >> 2;
	t ^= t << 1;
	t ^= s ^ (s << 4);
	state->a= t;
	state->cnt += 362437;
	return t + state->cnt;

}

static xwrand_t gstate = { 0 };






static void __exit mod_exit(void) {

	pr_info("Deleting cdev\n");
	cdev_del(&xw_cdev);
	pr_info("Unregistering chardev\n");
	unregister_chrdev_region(xw_dev, 1);
	pr_info("Module exit\n");
}

int xw_open(struct inode *inode, struct file* file) {
	u64 jff;
	xwrand_t *state = kmalloc(sizeof(*state), GFP_KERNEL);
	if (NULL == state)
		return -ENOMEM;
	*state = gstate;
	state->a ^= xwrand(&gstate);
	state->b ^= ((u64)file) & 0xFFFFFFFF;
	state->c ^= ((u64)file) >> 32;
	jff = get_jiffies_64();
	state->d ^= ((u64)jff) & 0xFFFFFFFF;
	state->e ^= ((u64)jff) >> 32;
	xwrand(state);
	file->private_data = state;
	pr_info("REMOVEME: File open (%p)\n", file);
	return 0;

}

int xw_release(struct inode *inode, struct file *file) {
	kfree(file->private_data);
	pr_info("REMOVEME: File close (%p)\n", file);
	return 0;
}

ssize_t xw_read (struct file * file, char __user *buf, size_t count, loff_t *offset) {
	ssize_t nread = 0;
	ssize_t i = 0;
	ssize_t rest = count %buf_sz;
	ssize_t restsz = rest % sizeof(u32) ? rest /sizeof(u32) : rest /sizeof(u32) + 1;
	u32 *kbuf = kmalloc(buf_sz + 1, GFP_KERNEL);
	xwrand_t *state = file->private_data;
	for(i = 0; i < count / buf_sz; ++i) {
		ssize_t j = 0;
		for(j = 0; j < buf_sz/sizeof(u32); j++)
			kbuf[j] = xwrand(state);
		if(copy_to_user(buf + nread, kbuf, buf_sz))
			goto read_fail;
		nread += buf_sz;
	}
	
	for (i = 0; i < restsz; i++) {
		kbuf[i] = xwrand(state);
	}
	if(copy_to_user(buf+ nread, kbuf, rest)) 
		goto read_fail;
	nread += rest;
	*offset += nread;
	if (nread != count) {
		goto read_fail;
	}
	kfree(kbuf);
	pr_info("REMOVEME: Read %lu bt\n", (unsigned long)nread);
	
	return nread;
read_fail:
	kfree(kbuf);
	pr_err("Read failed. Exp %lu, gor %lu\n", (unsigned long)count, (unsigned long)nread);
	return -EFAULT;
}

/*
loff_t xw_llseek(struct file *file, loff_t offset, int origin) {
	return 0;
}
*/



static struct file_operations xw_fops = {
	.owner = THIS_MODULE,
	.open = &xw_open,
	.release = &xw_release, 
	.read = &xw_read, 
//	.llseek = &xw_llseek,
};

static int __init mod_init(void)
{
	char _unused;
	int err;
	pr_info("REMOVEME: Module init with param: \"%s\"\n", seed);
	if (6 != sscanf(seed, "%u:%u:%u:%u:%u:%u%c", &(gstate.a), &(gstate.b), &(gstate.c),
		&(gstate.d), &(gstate.e), &(gstate.cnt), &(_unused))){ 
			pr_err("Wrong module param: seed\n");
			return -1;
		}
	pr_info("REMOVEME: Parsed params: %u:%u:%u:%u:%u:%u\n", &gstate.a, &gstate.b, &gstate.c,
		&gstate.d, &gstate.e, &gstate.cnt);
	
	if (0 != major) {
		xw_dev = MKDEV(major, minor);	
		err = register_chrdev_region(xw_dev, 1, devname);
		
	} else {
		pr_warn("Major set to %d. Using automatic allocation\n", major);
		err = alloc_chrdev_region(&xw_dev, minor, 1, devname); 
	}
	if (err) {
		pr_err("Error registering device\n");
		return -1;
	}
	pr_info("Registered device with %d:%d\n", MAJOR(xw_dev), MINOR(xw_dev));
	cdev_init(&xw_cdev, &xw_fops);
	if (0 != cdev_add(&xw_cdev, xw_dev,1)) {
		pr_err("Cdev add failde. Unregistering chardev %d:%d\n",
			MAJOR(xw_dev), MINOR(xw_dev));
		unregister_chrdev_region(xw_dev, 1);
		return -1;
	}
	
	return 0;
}


module_init(mod_init);
module_exit(mod_exit);






