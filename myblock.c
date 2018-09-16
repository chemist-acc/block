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


#define RAMBLOCK_SIZE (300 * 1024 * 1024)
#define HEAD_CNT (64)
#define SECTOR_CNT (64)
#define BYTE_PER_SECTOR (512)
#define CYLINDER_CNT (RAMBLOCK_SIZE / BYTE_PER_SECTOR / HEAD_CNT / SECTOR_CNT)

static struct gendisk *ram_block;
static struct request_queue *ram_block_queue;
static unsigned char *ramblock_buf; 
static int major;
static DEFINE_SPINLOCK(ramblock_lock);
 
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
	geo->sectors   = SECTOR_CNT ;
	return 0;
}
 
static struct block_device_operations ram_block_fops={
	.owner	= THIS_MODULE,
	.getgeo	= ramblock_getgeo,
};
static int ram_block_int(void){
	//ramblock_buf = kzalloc(RAMBLOCK_SIZE, GFP_KERNEL);
	ramblock_buf = vmalloc(RAMBLOCK_SIZE);
	if(NULL == ramblock_buf){
		return -ENOMEM;
	}

	ram_block=alloc_disk(5);
	ram_block_queue=blk_init_queue(do_ramblock_request, &ramblock_lock);
	ram_block->queue=ram_block_queue;
	major=register_blkdev(0,"ram_block");
	ram_block->major=major;
	ram_block->first_minor=0;
	ram_block->fops=&ram_block_fops;
	sprintf(ram_block->disk_name,"ram_block");
	set_capacity(ram_block, RAMBLOCK_SIZE / 512);
	add_disk(ram_block);
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
