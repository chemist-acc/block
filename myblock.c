#include <linux/module.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/genhd.h>
#include <linux/hdreg.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <linux/blkdev.h>
#include <linux/blkpg.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <asm/uaccess.h>
#include <asm/dma.h>
#include <asm-generic/errno-base.h>

#define MAX_DEV_NAME_LEN (127)
#define MAX_DEVICE_COUNT (31)
#define BYTE_PER_SECTOR (512)

//设备信息
#define HEAD_CNT (64)
#define SECTOR_CNT (64)


typedef struct dev_info_s{
	struct hd_geometry geo ;					// 设备信息，size
	char dev_name[MAX_DEV_NAME_LEN];			// 设备名字，显示的blockname
	char * buffer ;								// 内存起始地址
	DEFINE_SPINLOCK(ramblock_lock);				// 操作自旋锁
	struct gendisk * ram_block ;				//
	struct request_queue * ram_block_queue ;	// 消息队列
	int major ;									// 
	int device_size ;							// 设备大小，sector
	short in_use:1 ;
}dev_info_t;

dev_info_t g_devs[MAX_DEVICE_COUNT] ;
int g_dev_cnt ;
 
static void do_ramblock_request(struct request_queue * q){
	struct request *req;
	void *buffer;
	
	/*linux2.6之后的内核，elv_next_reques改为blk_fetch_request */
	while ((req = blk_fetch_request(q)) != NULL) {
		unsigned long offset = blk_rq_pos(req)<<9;
 
		/* 长度: */		
		unsigned long len = blk_rq_cur_bytes(req);
		buffer=bio_data(req->bio);
		if (rq_data_dir(req) == READ)
		{
			memcpy(buffer, ramblock_buf+offset, len);
		}
		else
		{
			memcpy(ramblock_buf+offset,buffer, len);
		}		
		__blk_end_request_all(req, BLK_STS_OK);
	}
}
 
static int ramblock_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	/* 容量=heads*cylinders*sectors*512 */
	geo->heads     = HEAD_CNT ;
	geo->cylinders = CYLINDER_CNT ;
	geo->sectors   = g_devs[g_dev_cnt].device_size / HEAD_CNT / SECTOR_CNT ;
	return 0;
}
 
static struct block_device_operations ram_block_fops={
	.owner	= THIS_MODULE,
	.getgeo	= ramblock_getgeo,
};

//创建一个block_size（MB）大小的RAM based disk
static int create_ram_block(int block_size){
	dev_info_t * device = &devs[g_dev_cnt] ;
	ramblock_buf = vmalloc(block_size * 1024 * 1024);
	if(NULL == ramblock_buf){
		return -ENOMEM;
	}
	device->device_size = block_device * 1024 * 2 ;
	device->ram_block = alloc_disk(5);
	device->ram_block_queue = blk_init_queue(do_ramblock_request, &device->ramblock_lock);
	device->ram_block->queue=ram_block_queue;
	device->major=register_blkdev(0,device->dev_name);
	device->ram_block->major=device->major;
	device->ram_block->first_minor=0;
	device->ram_block->fops=&ram_block_fops;
	sprintf(ram_block->disk_name,device->dev_name);
	set_capacity(device->ram_block, device->device_size);
	add_disk(device->ram_block);
	
}
static int ram_block_int(void){
	g_dev_cnt = 0 ;	
	return	0 ;
}
 
static void ram_block_exit(void){
	unregister_blkdev(major, "ram_block");
	del_gendisk(ram_block);
	put_disk(ram_block);
	blk_cleanup_queue(ram_block_queue);
	vfree(ramblock_buf);
	//kfree(ramblock_buf);
}
 
module_init(ram_block_int);
module_exit(ram_block_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("ChenGuangming");
MODULE_DESCRIPTION("The Ram Block Device Drive");
