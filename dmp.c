#include <linux/device-mapper.h>
#include <linux/kobject.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/bio.h>
#include <linux/sysfs.h>

#define DM_MSG_PREFIX "dmp"

/* Synchronization via read/write spinlock */

DEFINE_RWLOCK(stat_rwlock);

/*
 *	Statistics part
 */

static struct kobject* stat_kobject;

struct statistics {
	unsigned long long int write_query_count;
	unsigned long long int read_query_count;

	unsigned long long int write_sectors_count;
	unsigned long long int read_sectors_count;
};

static struct statistics dmp_stat = {
	.write_query_count = 0,
	.read_query_count  = 0,

	.write_sectors_count = 0,
	.read_sectors_count  = 0,
};

static ssize_t dmp_stat_show(struct kobject *kobj,
                         struct kobj_attribute *attr,
                      	 char *buf)
{
	unsigned long long int avg_read_block_size;
	unsigned long long int avg_write_block_size;
	unsigned long long int total_queries;
	unsigned long long int avg_block_size;

	/* constant SECTOR_SIZE is defined in blkdev.h and equal 512 : SECTOR_SIZE = (1 << 9) */	

	read_lock(&stat_rwlock);

	if (dmp_stat.read_query_count > 0) {
		avg_read_block_size = 
		dmp_stat.read_sectors_count * SECTOR_SIZE / dmp_stat.read_query_count;
	} else {
		avg_read_block_size = 0;
	}

	if (dmp_stat.write_query_count > 0) {
		avg_write_block_size = 
		dmp_stat.write_sectors_count * SECTOR_SIZE / dmp_stat.write_query_count;
	} else {
		avg_write_block_size = 0;
	}
	
	total_queries = 
	dmp_stat.read_query_count + dmp_stat.write_query_count;

	if (total_queries > 0) {
		avg_block_size = 
		(avg_read_block_size + avg_write_block_size) / 2;
	} else {
		avg_block_size = 0;
	}

	read_unlock(&stat_rwlock);

	const char* text;
  	text = "\
		read:\n\
			\treqs: %llu \n\
			\tavg: %llu \n\
		write:\n\
			\treqs: %llu \n\
			\tavg: %llu \n\
		total:\n\
			\treqs: %llu \n\
			\tavg: %llu \n\
		"; 

	return sprintf(buf, text, dmp_stat.read_query_count,
					avg_read_block_size,

					dmp_stat.write_query_count,
					avg_write_block_size,

					total_queries,
					avg_block_size);
}

static ssize_t dmp_stat_store(struct kobject *kobj, 
				struct kobj_attribute *attr,
				const char *buf, 
				size_t count) 
{
	return count;
}

static struct kobj_attribute stat_attribute =
	__ATTR(volumes, 0440, dmp_stat_show, dmp_stat_store);

/*
 *	Target part
 */

struct my_dm_target {
	struct dm_dev *dev;
	sector_t start;
};

static int dmp_ctr(struct dm_target *ti,
                   unsigned int argc,
                   char **argv)
{
	struct my_dm_target *mdt;

	unsigned long long start;

	if (argc != 2) {
		printk(KERN_CRIT "\n Invalid no.of arguments.\n");

		ti->error = "Invalid argument count";

		return -EINVAL;
	}

	mdt = kmalloc(sizeof(struct my_dm_target), GFP_KERNEL);

	if(mdt==NULL) {
		printk(KERN_CRIT "\n Mdt is null\n");

		ti->error = "dm-basic_target: Cannot allocate linear context";

		return -ENOMEM;
	}       

	if(sscanf(argv[1], "%llu", &start)!=1) {
		ti->error = "dm-basic_target: Invalid device sector";

		goto bad;
	}

	mdt->start=(sector_t)start;

	if (dm_get_device(ti, argv[0], dm_table_get_mode(ti->table), &mdt->dev)) {
		ti->error = "dm-basic_target: Device lookup failed";

		goto bad;
	}

	ti->private = mdt;
                 
	return 0;

bad:
	kfree(mdt);
	printk(KERN_CRIT "\n>>out function basic_target_ctr with errorrrrrrrrrr \n");           
	return -EINVAL;
}    


static void dmp_dtr(struct dm_target *ti)
{
	struct my_dm_target *mdt = (struct my_dm_target *) ti->private;
	printk(KERN_CRIT "\n<<in function basic_target_dtr \n");        
	dm_put_device(ti, mdt->dev);
 	kfree(mdt);
 	printk(KERN_CRIT "\n>>out function basic_target_dtr \n");               
}

static int dmp_map(struct dm_target *ti, struct bio *bio)
{
	struct my_dm_target *mdt = (struct my_dm_target *) ti->private;

	bio_set_dev(bio, mdt->dev->bdev);

	//writing data to dmp_stats

	write_lock(&stat_rwlock);

	switch (bio_op(bio)) {

		case REQ_OP_READ:

			dmp_stat.read_query_count += 1;
			dmp_stat.read_sectors_count += bio_sectors(bio);

		break;

		case REQ_OP_WRITE:

			dmp_stat.write_query_count += 1;
			dmp_stat.write_sectors_count += bio_sectors(bio);

		break;

		default:
			return DM_MAPIO_KILL;
	}

	write_unlock(&stat_rwlock);
        
	submit_bio(bio);

	return DM_MAPIO_SUBMITTED;
}

static struct target_type dmp_target = {    
	.name = "dmp",
	.version = {1,0,0},
	.module = THIS_MODULE,
	.ctr = dmp_ctr,
	.dtr = dmp_dtr,
	.map = dmp_map,
};

/*
 *	Init / Exit part
 */

static int __init dmp_init(void)
{
	/* Here we should initialize resources and register target */
	int result;
	result = dm_register_target(&dmp_target);

	if(result < 0) {
		printk(KERN_CRIT "\n Error in registering target \n");
	}

	stat_kobject = kobject_create_and_add("stat", &THIS_MODULE->mkobj.kobj);

	if (!stat_kobject) {
		return -ENOMEM;
	}

	int error;
	error = sysfs_create_file(stat_kobject, &stat_attribute.attr);

	if (error) {
		printk(KERN_CRIT "\n Error in creating sysfs_file \n");
	}

	return 0;
}

static void __exit dmp_exit(void)
{
	/* Here we should free resources */
	kobject_put(stat_kobject);
 	dm_unregister_target(&dmp_target);

 	/* You can comment next line if you want the file to remain */
 	sysfs_remove_file(stat_kobject, &stat_attribute.attr);
}

module_init(dmp_init)
module_exit(dmp_exit)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Morozov Ilia - morozov97@mail.ru");
MODULE_DESCRIPTION("Simple device mapper proxy");
