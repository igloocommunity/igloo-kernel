/*
 * Copyright (C) ST-Ericsson SA 2010
 * Author: Marcin Mielczarczyk <marcin.mielczarczyk@tieto.com>
 *	   for ST-Ericsson
 * License Terms: GNU General Public License v2
 */

#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/ste_timed_vibra.h>
#include <linux/delay.h>
#include "timed_output.h"

/**
 * struct vibra_info - Vibrator information structure
 * @tdev:		Pointer to timed output device structure
 * @vibra_workqueue:    Pointer to vibrator workqueue structure
 * @vibra_work:		Vibrator work
 * @vibra_lock:		Vibrator lock
 * @vibra_timer:	Vibrator high resolution timer
 * @vibra_state:	Actual vibrator state
 * @state_force:	Indicates if oppositive state is requested
 * @timeout:		Indicates how long time the vibrator will be enabled
 * @time_passed:	Total time passed in states
 * @pdata:		Local pointer to platform data with vibrator parameters
 *
 * Structure vibra_info holds vibrator information
 **/
struct vibra_info {
	struct timed_output_dev			tdev;
	struct workqueue_struct			*vibra_workqueue;
	struct work_struct			vibra_work;
	spinlock_t				vibra_lock;
	struct hrtimer				vibra_timer;
	enum ste_timed_vibra_states		vibra_state;
	bool					state_force;
	unsigned int				timeout;
	unsigned int				time_passed;
	struct ste_timed_vibra_platform_data	*pdata;
};

/*
 * For Linear type vibrators, resonance frequency waveform is
 * generated using software loop and for same negative and
 * positive cycle time period need to be calibrated. For 150Hz
 * frequency nearest delay value is found out to be equal to "3".
 */
#define LINEAR_VIBRA_150HZ_DELAY	3

/**
 * vibra_control_work() - Vibrator work, turns on/off vibrator
 * @work:	Pointer to work structure
 *
 * This function is called from workqueue, turns on/off vibrator
 **/
static void vibra_control_work(struct work_struct *work)
{
	struct vibra_info *vinfo =
			container_of(work, struct vibra_info, vibra_work);
	unsigned val = 0;
	unsigned char speed_pos = 0, speed_neg = 0;
	unsigned long flags;
	unsigned long jiffies_till;

	spin_lock_irqsave(&vinfo->vibra_lock, flags);
	/* Should be already expired */
	hrtimer_cancel(&vinfo->vibra_timer);

	switch (vinfo->vibra_state) {
	case STE_VIBRA_BOOST:
		/* Turn on both vibrators with boost speed */
		speed_pos = vinfo->pdata->boost_level;
		val = vinfo->pdata->boost_time;
		break;
	case STE_VIBRA_ON:
		/* Turn on both vibrators with speed */
		speed_pos = vinfo->pdata->on_level;
		val = vinfo->timeout - vinfo->pdata->boost_time;
		break;
	case STE_VIBRA_OFF:
		/* Turn on both vibrators with reversed speed */
		speed_neg = vinfo->pdata->off_level;
		val = vinfo->pdata->off_time;
		break;
	case STE_VIBRA_IDLE:
		vinfo->time_passed = 0;
		break;
	default:
		break;
	}
	spin_unlock_irqrestore(&vinfo->vibra_lock, flags);

	if (vinfo->pdata->is_linear_vibra) {
		jiffies_till = jiffies + msecs_to_jiffies(vinfo->timeout);
		while (time_before(jiffies, jiffies_till)) {
			vinfo->pdata->timed_vibra_control(
					speed_pos, speed_neg,
					speed_pos, speed_neg);
			mdelay(LINEAR_VIBRA_150HZ_DELAY);
			vinfo->pdata->timed_vibra_control(
					speed_neg, speed_pos,
					speed_neg, speed_pos);
			mdelay(LINEAR_VIBRA_150HZ_DELAY);
		}

		/* Disable Vibrator */
		speed_pos = speed_neg = 0;
		vinfo->pdata->timed_vibra_control(
				speed_pos, speed_neg,
				speed_pos, speed_neg);
		vinfo->vibra_state = STE_VIBRA_IDLE;
	} else {
		/* Send new settings */
		vinfo->pdata->timed_vibra_control(
				speed_pos, speed_neg,
				speed_pos, speed_neg);

		/* Start timer if it's not in IDLE state */
		if (vinfo->vibra_state != STE_VIBRA_IDLE) {
			hrtimer_start(&vinfo->vibra_timer,
				ktime_set(val / 1000, (val % 1000) * 1000000),
				HRTIMER_MODE_REL);
		}
	}
}

/**
 * vibra_enable() - Enables vibrator
 * @tdev:      Pointer to timed output device structure
 * @timeout:	Time indicating how long vibrator will be enabled
 *
 * This function enables vibrator
 **/
static void vibra_enable(struct timed_output_dev *tdev, int timeout)
{
	struct vibra_info *vinfo = dev_get_drvdata(tdev->dev);
	unsigned long flags;

	spin_lock_irqsave(&vinfo->vibra_lock, flags);
	switch (vinfo->vibra_state) {
	case STE_VIBRA_IDLE:
		if (timeout)
			vinfo->vibra_state = STE_VIBRA_BOOST;
		else    /* Already disabled */
			break;

		vinfo->state_force = false;
		/* Trim timeout */
		timeout = timeout < vinfo->pdata->boost_time ?
				vinfo->pdata->boost_time : timeout;
		/* Remember timeout value */
		vinfo->timeout = timeout;
		queue_work(vinfo->vibra_workqueue, &vinfo->vibra_work);
		break;
	case STE_VIBRA_BOOST:
		/* Force only when user requested OFF while BOOST */
		if (!timeout)
			vinfo->state_force = true;
		break;
	case STE_VIBRA_ON:
		/* If user requested OFF */
		if (!timeout && !vinfo->pdata->is_linear_vibra) {
			hrtimer_cancel(&vinfo->vibra_timer);
			vinfo->vibra_state = STE_VIBRA_OFF;
			queue_work(vinfo->vibra_workqueue, &vinfo->vibra_work);
		}
		break;
	case STE_VIBRA_OFF:
		/* Force only when user requested ON while OFF */
		if (timeout)
			vinfo->state_force = true;
		break;
	default:
		break;
	}
	spin_unlock_irqrestore(&vinfo->vibra_lock, flags);
}

/**
 * vibra_timer_expired() - Handles vibrator machine state
 * @hrtimer:      Pointer to high resolution timer structure
 *
 * This function handles vibrator machine state
 *
 * Returns:
 *	Returns value which indicates wether hrtimer should be restarted
 **/
static enum hrtimer_restart vibra_timer_expired(struct hrtimer *hrtimer)
{
	struct vibra_info *vinfo =
			container_of(hrtimer, struct vibra_info, vibra_timer);
	unsigned long flags;

	spin_lock_irqsave(&vinfo->vibra_lock, flags);
	switch (vinfo->vibra_state) {
	case STE_VIBRA_BOOST:
		/* If BOOST finished and force, go to OFF */
		if (vinfo->state_force)
			vinfo->vibra_state = STE_VIBRA_OFF;
		else
			vinfo->vibra_state = STE_VIBRA_ON;
		vinfo->time_passed = vinfo->pdata->boost_time;
		break;
	case STE_VIBRA_ON:
		vinfo->vibra_state = STE_VIBRA_OFF;
		vinfo->time_passed = vinfo->timeout;
		break;
	case STE_VIBRA_OFF:
		/* If OFF finished and force, go to ON */
		if (vinfo->state_force)
			vinfo->vibra_state = STE_VIBRA_ON;
		else
			vinfo->vibra_state = STE_VIBRA_IDLE;
		vinfo->time_passed += vinfo->pdata->off_time;
		break;
	case STE_VIBRA_IDLE:
		break;
	default:
		break;
	}
	vinfo->state_force = false;
	spin_unlock_irqrestore(&vinfo->vibra_lock, flags);

	queue_work(vinfo->vibra_workqueue, &vinfo->vibra_work);

	return HRTIMER_NORESTART;
}

/**
 * vibra_get_time() - Returns remaining time to disabling vibration
 * @tdev:      Pointer to timed output device structure
 *
 * This function returns time remaining to disabling vibration
 *
 * Returns:
 *	Returns remaining time to disabling vibration
 **/
static int vibra_get_time(struct timed_output_dev *tdev)
{
	struct vibra_info *vinfo = dev_get_drvdata(tdev->dev);
	u32 ms;

	if (hrtimer_active(&vinfo->vibra_timer)) {
		ktime_t remain = hrtimer_get_remaining(&vinfo->vibra_timer);
		ms = (u32) ktime_to_ms(remain);
		return ms + vinfo->time_passed;
	} else
		return 0;
}

static int __devinit ste_timed_vibra_probe(struct platform_device *pdev)
{
	int ret;
	struct vibra_info *vinfo;

	if (!pdev->dev.platform_data) {
		dev_err(&pdev->dev, "No platform data supplied\n");
		return -ENODEV;
	}

	vinfo = kmalloc(sizeof *vinfo, GFP_KERNEL);
	if (!vinfo) {
		dev_err(&pdev->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	vinfo->tdev.name = "vibrator";
	vinfo->tdev.enable = vibra_enable;
	vinfo->tdev.get_time = vibra_get_time;
	vinfo->time_passed = 0;
	vinfo->vibra_state = STE_VIBRA_IDLE;
	vinfo->state_force = false;
	vinfo->pdata = pdev->dev.platform_data;

	if (vinfo->pdata->is_linear_vibra)
		dev_info(&pdev->dev, "Linear Type Vibrators\n");
	else
		dev_info(&pdev->dev, "Rotary Type Vibrators\n");

	ret = timed_output_dev_register(&vinfo->tdev);
	if (ret) {
		dev_err(&pdev->dev, "failed to register timed output device\n");
		goto exit_free_vinfo;
	}


	vinfo->tdev.dev->parent = pdev->dev.parent;
	dev_set_drvdata(vinfo->tdev.dev, vinfo);

	/* Create workqueue just for timed output vibrator */
	vinfo->vibra_workqueue =
		create_singlethread_workqueue("ste-timed-output-vibra");
	if (!vinfo->vibra_workqueue) {
		dev_err(&pdev->dev, "failed to allocate workqueue\n");
		ret = -ENOMEM;
		goto exit_timed_output_unregister;
	}

	INIT_WORK(&vinfo->vibra_work, vibra_control_work);
	spin_lock_init(&vinfo->vibra_lock);
	hrtimer_init(&vinfo->vibra_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	vinfo->vibra_timer.function = vibra_timer_expired;
	platform_set_drvdata(pdev, vinfo);
	return 0;

exit_timed_output_unregister:
	timed_output_dev_unregister(&vinfo->tdev);
exit_free_vinfo:
	kfree(vinfo);
	return ret;
}

static int __devexit ste_timed_vibra_remove(struct platform_device *pdev)
{
	struct vibra_info *vinfo = platform_get_drvdata(pdev);

	timed_output_dev_unregister(&vinfo->tdev);
	destroy_workqueue(vinfo->vibra_workqueue);
	kfree(vinfo);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver ste_timed_vibra_driver = {
	.driver = {
		.name = "ste_timed_output_vibra",
		.owner = THIS_MODULE,
	},
	.probe  = ste_timed_vibra_probe,
	.remove = __devexit_p(ste_timed_vibra_remove)
};

static int __init ste_timed_vibra_init(void)
{
	return platform_driver_register(&ste_timed_vibra_driver);
}
module_init(ste_timed_vibra_init);

static void __exit ste_timed_vibra_exit(void)
{
	platform_driver_unregister(&ste_timed_vibra_driver);
}
module_exit(ste_timed_vibra_exit);

MODULE_AUTHOR("Marcin Mielczarczyk <marcin.mielczarczyk@tieto.com>");
MODULE_DESCRIPTION("STE Timed Output Vibrator");
MODULE_LICENSE("GPL v2");
