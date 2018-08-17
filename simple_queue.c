#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/gfp.h>
#include <linux/poll.h>
#include <asm/uaccess.h>
#include <linux/sched.h> 


#define	SIMPLE_QUEUE_MAJOR	200 //major number of device
#define MAX_DATA	0x10  //max length of queue is 16
 
static	int simple_queue_major = SIMPLE_QUEUE_MAJOR; 

struct simple_queue_dev {
	struct cdev cdev;
	unsigned char data[MAX_DATA]; //队列存储数组
	struct semaphore sem; //避免竞争状态
	unsigned int current_head;    //队列当前头部
	unsigned int current_tail;    //队列当前尾部
	unsigned int current_len;     //队列当前长度
	wait_queue_head_t r_wait;
	wait_queue_head_t w_wait;
};
 
struct	simple_queue_dev *simple_queue_devp; //设备结构体指针
 
int	simple_queue_open(struct inode *inode, struct file *filp)
{
	filp->private_data = simple_queue_devp;
	printk(KERN_ALERT "open the simple_queue device\n");
	return 0;
}

ssize_t	simple_queue_read(struct file *filp, char __user *buf, size_t count, loff_t *f_ops)
{
	struct simple_queue_dev *dev = filp->private_data;
	int ret = 0;
	DECLARE_WAITQUEUE(wait, current); //定义等待队列，方便实现read的阻塞
	if(*f_ops)
		return 0;
	down(&dev->sem);
	add_wait_queue(&dev->r_wait, &wait);//任务添加进等待队列
	while(dev->current_len == 0)
	{
		if(filp->f_flags & O_NONBLOCK)//判断文件是非阻塞还是阻塞的，非阻塞直接返回EAGAIN
		{
			return -EAGAIN;
		}
		__set_current_state(TASK_INTERRUPTIBLE);
		up(&dev->sem);
		schedule();
		if(signal_pending(current)) //判断是否由信号唤醒
		{
			ret = -ERESTARTSYS;
			goto out2;
		}
		down(&dev->sem);
	}
	//队列有数据了且当前头是current_len
	if(copy_to_user(buf, (void *)(dev->data),2)){
		ret = -EFAULT;
		goto out;
	} else 
	{
		printk(KERN_ALERT "cat current data is: %c\n",dev->data[dev->current_tail]);//cat一次输出一个int
		dev->current_len -= 2;
		dev->current_tail += 2;
		printk(KERN_ALERT "current len is: %d\n",dev->current_len);
		wake_up_interruptible(&dev->w_wait);
		*f_ops +=2;
		ret = 2;
	}
out:
	up(&dev->sem);
out2:
	remove_wait_queue(&dev->w_wait, &wait);
	set_current_state(TASK_RUNNING);
return ret;
}
 
ssize_t simple_queue_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_ops)
{
	struct simple_queue_dev *dev = filp->private_data;
	int ret = 0;
	//printk(KERN_ALERT "---------------------%d%d\n",dev->data[dev->current_len],count);//echo的数据
	DECLARE_WAITQUEUE(wait, current);
	down(&dev->sem);
	add_wait_queue(&dev->w_wait, &wait);//定义等待队列，方便实现write的阻塞
	while(dev->current_len == MAX_DATA){
		if(filp->f_flags & O_NONBLOCK){ //判断文件是非阻塞还是阻塞的，非阻塞直接返回EAGAIN
			ret = -EAGAIN;
			goto out;
		}
		__set_current_state(TASK_INTERRUPTIBLE);
		up(&dev->sem);
		schedule();
		if(signal_pending(current)){//判断是否由信号唤醒
			ret = -ERESTARTSYS;
			goto out2;
		}
		down(&dev->sem);
	}
	if(copy_from_user(dev->data + dev->current_head, buf, count)){
		//copy失败
		ret = -EFAULT;
		goto out;
	}
	else {
		//copy成功返回0
		printk(KERN_ALERT "echo  data is: %c,count is: %d\n",dev->data[dev->current_head],count);//echo的数据
		dev->current_head += count;
		dev->current_len += count;
		wake_up_interruptible(&dev->r_wait);
		ret = count;
	}
out:
	up(&dev->sem);
out2:
	remove_wait_queue(&dev->w_wait, &wait);
	set_current_state(TASK_RUNNING);
return ret;
}
 

//simple_queue设备支持的file_operations 
static const struct	file_operations simple_queue_fops = {
	.owner	=	THIS_MODULE,
	.open	=	simple_queue_open,
	.write	=	simple_queue_write,
	.read	=	simple_queue_read,
};
 
//simple_queue的setup函数，负责dev的初始化
static	void simple_queue_setup_cdev(struct simple_queue_dev *dev, int index)
{
	int err, devno = MKDEV(simple_queue_major, index);
	cdev_init(&dev->cdev, &simple_queue_fops);//绑定cdev与file_ops
	dev->cdev.owner = THIS_MODULE;
	err = cdev_add(&dev->cdev, devno, 1);
	if(err)
		printk(KERN_NOTICE "ERROR %d adding simple_queue_dev %d", err, index);
}
 
//定义模块init函数，分配资源
int	simple_queue_init(void)
{
	int result;
	//注册设备号
	dev_t	devno = MKDEV(simple_queue_major,0);
	if(simple_queue_major)
		result = register_chrdev_region(devno, 1, "simple_queue");
	else {
		result = alloc_chrdev_region(&devno, 0, 1, "simple_queue");
		simple_queue_major = MAJOR(devno);
	}
	if(result < 0)
		return result;
	
	//分配内存并清0
	simple_queue_devp = kmalloc(sizeof(struct simple_queue_dev), GFP_KERNEL);
	if(!simple_queue_devp){
		result = -ENOMEM;
		goto fail_malloc;
	}
	memset(simple_queue_devp, 0 , sizeof(struct simple_queue_dev));
	
	//设备初始化
	simple_queue_setup_cdev(simple_queue_devp, 0);
    
	sema_init(&simple_queue_devp->sem,1);
    //初始化等待队列
	init_waitqueue_head(&simple_queue_devp->r_wait);
	init_waitqueue_head(&simple_queue_devp->w_wait);
 
	printk(KERN_ALERT "init simple_queue device\n");
	return 0;
fail_malloc:
	unregister_chrdev_region(devno, 1);
	return result;
}

//定义模块退出函数，释放资源
void simple_queue_cleanup(void)
{
	cdev_del(&simple_queue_devp->cdev);
	kfree(simple_queue_devp);
	unregister_chrdev_region(MKDEV(simple_queue_major, 0), 1);
	printk(KERN_ALERT "clean simple_queue device\n");	
}
 
module_init(simple_queue_init); //定义模块init函数
module_exit(simple_queue_cleanup);//定义模块退出函数
MODULE_LICENSE("GPL");
MODULE_AUTHOR("hs");