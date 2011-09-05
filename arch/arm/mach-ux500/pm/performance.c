/*
 * Copyright (C) ST-Ericsson SA 2011
 *
 * License Terms: GNU General Public License v2
 * Author: Johan Rudholm <johan.rudholm@stericsson.com>
 */

#include <linux/kernel.h>
#include <mach/prcmu-qos.h>
#include <linux/genhd.h>
#include <linux/major.h>
#include <linux/cdev.h>

/*
 * TODO:
 * o Develop a more power-aware algorithm
 * o Make the parameters visible through debugfs
 * o Get the value of CONFIG_MMC_BLOCK_MINORS in runtime instead, since
 *   it may be altered by drivers/mmc/card/block.c
 */

/* Sample reads and writes every n ms */
#define PERF_MMC_PROBE_DELAY 1000
/* Read threshold, sectors/second */
#define PERF_MMC_LIMIT_READ 10240
/* Write threshold, sectors/second */
#define PERF_MMC_LIMIT_WRITE 8192
/* Nr of MMC devices */
#define PERF_MMC_HOSTS 8

/*
 * Rescan for new MMC devices every
 * PERF_MMC_PROBE_DELAY * PERF_MMC_RESCAN_CYCLES ms
 */
#define PERF_MMC_RESCAN_CYCLES 10

static struct delayed_work work_mmc;

/*
 * Loop through every CONFIG_MMC_BLOCK_MINORS'th minor device for
 * MMC_BLOCK_MAJOR, get the struct gendisk for each device. Returns
 * nr of found disks. Populate mmc_disks.
 */
static int scan_mmc_devices(struct gendisk *mmc_disks[])
{
	dev_t devnr;
	int i, j = 0, part;
	struct gendisk *mmc_devices[256 / CONFIG_MMC_BLOCK_MINORS];

	memset(&mmc_devices, 0, sizeof(mmc_devices));

	for (i = 0; i * CONFIG_MMC_BLOCK_MINORS < 256; i++) {
		devnr = MKDEV(MMC_BLOCK_MAJOR, i * CONFIG_MMC_BLOCK_MINORS);
		mmc_devices[i] = get_gendisk(devnr, &part);

		/* Invalid capacity of device, do not add to list */
		if (!mmc_devices[i] || !get_capacity(mmc_devices[i]))
			continue;

		mmc_disks[j] = mmc_devices[i];
		j++;

		if (j == PERF_MMC_HOSTS)
			break;
	}

	return j;
}

/*
 * Sample sectors read and written to any MMC devices, update PRCMU
 * qos requirement
 */
static void mmc_load(struct work_struct *work)
{
	static unsigned long long old_sectors_read[PERF_MMC_HOSTS];
	static unsigned long long old_sectors_written[PERF_MMC_HOSTS];
	static struct gendisk *mmc_disks[PERF_MMC_HOSTS];
	static int cycle, nrdisk;
	static bool old_mode;
	unsigned long long sectors;
	bool new_mode = false;
	int i;

	if (!cycle) {
		memset(&mmc_disks, 0, sizeof(mmc_disks));
		nrdisk = scan_mmc_devices(mmc_disks);
		cycle = PERF_MMC_RESCAN_CYCLES;
	}
	cycle--;

	for (i = 0; i < nrdisk; i++) {
		sectors = part_stat_read(&(mmc_disks[i]->part0),
						sectors[READ]);

		if (old_sectors_read[i] &&
			sectors > old_sectors_read[i] &&
			(sectors - old_sectors_read[i]) >
			PERF_MMC_LIMIT_READ)
			new_mode = true;

		old_sectors_read[i] = sectors;
		sectors = part_stat_read(&(mmc_disks[i]->part0),
						sectors[WRITE]);

		if (old_sectors_written[i] &&
			sectors > old_sectors_written[i] &&
			(sectors - old_sectors_written[i]) >
			PERF_MMC_LIMIT_WRITE)
			new_mode = true;

		old_sectors_written[i] = sectors;
	}

	if (!old_mode && new_mode)
		prcmu_qos_update_requirement(PRCMU_QOS_ARM_OPP,
						"mmc", 125);

	if (old_mode && !new_mode)
		prcmu_qos_update_requirement(PRCMU_QOS_ARM_OPP,
						"mmc", 25);

	old_mode = new_mode;

	schedule_delayed_work(&work_mmc,
				 msecs_to_jiffies(PERF_MMC_PROBE_DELAY));

}


static int __init performance_register(void)
{
	int ret;

	prcmu_qos_add_requirement(PRCMU_QOS_ARM_OPP, "mmc", 25);

	INIT_DELAYED_WORK_DEFERRABLE(&work_mmc, mmc_load);
	ret = schedule_delayed_work(&work_mmc,
				 msecs_to_jiffies(PERF_MMC_PROBE_DELAY));

	return ret;
}
late_initcall(performance_register);
