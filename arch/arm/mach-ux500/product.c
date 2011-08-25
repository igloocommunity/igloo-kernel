/*
 * Copyright (C) ST-Ericsson SA 2011
 *
 * Author: Jens Wiklander <jens.wiklander@stericsson.com>
 * Author: Jonas Aaberg <jonas.aberg@stericsson.com>
 * Author: Mian Yousaf Kaukab <mian.yousaf.kaukab@stericsson.com>
 *
 * License terms: GNU General Public License (GPL) version 2
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/tee.h>
#include <linux/module.h>
#include <mach/hardware.h>

#define STATIC_TEE_TA_START_LOW	0xBC765EDE
#define STATIC_TEE_TA_START_MID	0x6724
#define STATIC_TEE_TA_START_HIGH	0x11DF
#define STATIC_TEE_TA_START_CLOCKSEQ  \
	{0x8E, 0x12, 0xEC, 0xDB, 0xDF, 0xD7, 0x20, 0x85}

static struct tee_product_config product_config;

bool ux500_jtag_enabled(void)
{
	return (product_config.rt_flags & TEE_RT_FLAGS_JTAG_ENABLED) ==
		TEE_RT_FLAGS_JTAG_ENABLED;
}

static int __init product_detect(void)
{
	int err;
	int origin_err;
	struct tee_operation operation = { { { 0 } } };
	struct tee_context context;
	struct tee_session session;

	/* Selects trustzone application needed for the job. */
	struct tee_uuid static_uuid = {
		STATIC_TEE_TA_START_LOW,
		STATIC_TEE_TA_START_MID,
		STATIC_TEE_TA_START_HIGH,
		STATIC_TEE_TA_START_CLOCKSEQ,
	};

	if (cpu_is_u5500())
		return -ENODEV;

	err = teec_initialize_context(NULL, &context);
	if (err) {
		pr_err("ux500-product: unable to initialize tee context,"
			" err = %d\n", err);
		err = -EINVAL;
		goto error0;
	}

	err = teec_open_session(&context, &session, &static_uuid,
				TEEC_LOGIN_PUBLIC, NULL, NULL, &origin_err);
	if (err) {
		pr_err("ux500-product: unable to open tee session,"
			" tee error = %d, origin error = %d\n",
			err, origin_err);
		err = -EINVAL;
		goto error1;
	}

	operation.shm[0].buffer = &product_config;
	operation.shm[0].size = sizeof(product_config);
	operation.shm[0].flags = TEEC_MEM_OUTPUT;
	operation.flags = TEEC_MEMREF_0_USED;

	err = teec_invoke_command(&session,
				  TEE_STA_GET_PRODUCT_CONFIG,
				  &operation, &origin_err);
	if (err) {
		pr_err("ux500-product: fetching product settings failed, err=%d",
		       err);
		err = -EINVAL;
		goto error1;
	}

	pr_info("ux500-product: JTAG is %s\n",
		ux500_jtag_enabled()? "enabled" : "disabled");
error1:
	(void) teec_finalize_context(&context);
error0:
	return err;
}
device_initcall(product_detect);

