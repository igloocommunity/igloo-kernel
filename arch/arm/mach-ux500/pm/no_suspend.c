
#include <linux/kernel.h>
#include <linux/wakelock.h>

/* Temporary suspend blocker until suspend works */

static struct wake_lock wakelock;
static int __init take_wake_lock(void)
{
	wake_lock_init(&wakelock, WAKE_LOCK_SUSPEND, "no_suspend");
	wake_lock(&wakelock);
	return 0;
}
late_initcall(take_wake_lock);
