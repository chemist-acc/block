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
 
#define RAMBLOCK_SIZE (1024*1024)
static struct gendisk *ram_block;
static struct request_queue *ram_block_queue;
static unsigned char *ramblock_buf; 
static int major;
static DEFINE_SPINLOCK(ramblock_lock);
 
static void do_ramblock_request(struct request_queue * q){
	static int r_cnt = 0;
	static int w_cnt = 0;
	struct request *req;
	void *buffer;
	
	//printk("do_ramblock_request %d\n", ++cnt);
	/*linux2.6之后的内核，elv_next_reques改为blk_fetch_request */
	while ((req = blk_fetch_request(q)) != NULL) {
		/* 数据传输三要素: 源,目的,长度 */
		/* 源/目的: */
		unsigned long offset = blk_rq_pos(req)<<9;
 
		/* 目的/源: */
		// req->buffer
 
		/* 长度: */		
		unsigned long len = blk_rq_cur_bytes(req);
		buffer=bio_data(req->bio);
		if (rq_data_dir(req) == READ)
		{
			printk("do_ramblock_request read %d\n", ++r_cnt);
			memcpy(buffer, ramblock_buf+offset, len);
		}
		else
		{
			printk("do_ramblock_request write %d\n", ++w_cnt);
			memcpy(ramblock_buf+offset,buffer, len);
		}		
		
		//end_request(req, 1);
		__blk_end_request_all(req, BLK_STS_OK);
	}
}
 
static int ramblock_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	/* 容量=heads*cylinders*sectors*512 */
	geo->heads     = 2;
	geo->cylinders = 32;
	geo->sectors   = RAMBLOCK_SIZE/2/32/512;
	return 0;
}
 
static struct block_device_operations ram_block_fops={
	.owner	= THIS_MODULE,
	.getgeo	= ramblock_getgeo,
};
static int ram_block_int(void){
	ram_block=alloc_disk(5);
	ram_block_queue=blk_init_queue(do_ramblock_request, &ramblock_lock);
	ram_block->queue=ram_block_queue;
	major=register_blkdev(0,"ram_block");
	ram_block->major=major;
	ram_block->first_minor=0;
	ram_block->fops=&ram_block_fops;
	sprintf(ram_block->disk_name,"ram_block");
	set_capacity(ram_block, RAMBLOCK_SIZE / 512);
	ramblock_buf = kzalloc(RAMBLOCK_SIZE, GFP_KERNEL);
	add_disk(ram_block);
	return	0;
}
 
static void ram_block_exit(void){
	unregister_blkdev(major, "ram_block");
	del_gendisk(ram_block);
	put_disk(ram_block);
	blk_cleanup_queue(ram_block_queue);
	kfree(ramblock_buf);
}
 
module_init(ram_block_int);
module_exit(ram_block_exit);
MODULE_LICENSE("GPL");

