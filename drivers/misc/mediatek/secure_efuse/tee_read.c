/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/tee_drv.h>
#include <trustzone/kree/mem.h>
#include <trustzone/kree/system.h>
#include <trustzone/tz_cross/ta_efuse.h>

#define TZ_TA_EFUSE_UUID   "5deec602-b2fe-4bb6-8c7b-be99c6ccabb0"
#define TEEC_PAYLOAD_REF_COUNT 4

#ifdef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
int tee_fuse_read(u32 fuse, u8 *data, size_t len)
{
	union MTEEC_PARAM param[4];
	u32 paramTypes;
	int tz_ret = TZ_RESULT_SUCCESS;
	KREE_SESSION_HANDLE efuse_session;

	tz_ret = KREE_CreateSession(TZ_TA_EFUSE_UUID, &efuse_session);
	if (tz_ret) {
		pr_err("[efuse] failed to create session: %d\n", tz_ret);
		return tz_ret;
	}

	param[0].value.a = fuse;
	param[1].mem.buffer = data;
	param[1].mem.size = len;
	paramTypes = TZ_ParamTypes2(TZPT_VALUE_INPUT, TZPT_MEM_OUTPUT);
	tz_ret = KREE_TeeServiceCall(efuse_session, 0, paramTypes, param);
	if (tz_ret)
		pr_err("[efuse] failed to do service call: %x\n", tz_ret);

	tz_ret = KREE_CloseSession(efuse_session);
	if (tz_ret)
		pr_err("[efuse] failed to close seesion: %x\n", tz_ret);

	return tz_ret;
}
#else

static int efuse_optee_dev_match(struct tee_ioctl_version_data *t
		, const void *v)
{
	if (t->impl_id == TEE_IMPL_ID_OPTEE)
		return 1;
	return 0;
}

enum efuse_cmd {
	TZCMD_EFUSE_READ = 0,
	TZCMD_EFUSE_WRITE
};

int tee_fuse_read(u32 fuse, u8 *data, size_t len)
{
	int rc, ret = 0;
	struct tee_context *tee_ctx;
	struct tee_ioctl_open_session_arg osarg;
	uint8_t efuse_uuid[] = {0xa2, 0x56, 0x7d, 0x51,
				0x01, 0x44, 0x45, 0x43,
				0xb4, 0x0a, 0xca, 0xba,
				0x40, 0x27, 0x97, 0x03 };

	struct tee_ioctl_invoke_arg arg;
	struct tee_param params[TEEC_PAYLOAD_REF_COUNT];
	struct tee_shm *shm = NULL;
	u32 shm_size = 256;

	/* 0: open context */
	tee_ctx = tee_client_open_context(NULL, efuse_optee_dev_match,
							NULL, NULL);
	if (IS_ERR(tee_ctx)) {
		pr_err("open_context failed err %ld", PTR_ERR(tee_ctx));
		ret = -1;
		goto out;
	}
	pr_debug("open_context succeed! tee_ctx = %lx\n",
			(unsigned long)tee_ctx);

	shm = tee_shm_alloc(tee_ctx, shm_size,
				   TEE_SHM_MAPPED | TEE_SHM_DMA_BUF);

	if (IS_ERR(shm)) {
		pr_err("tee_shm_alloc failed\n");
		ret = -1;
		goto close_context;
	}

	/* 1: open session */
	memset(&osarg, 0, sizeof(osarg));
	osarg.num_params = TEEC_PAYLOAD_REF_COUNT;
	osarg.clnt_login = TEE_IOCTL_LOGIN_PUBLIC;
	memcpy(osarg.uuid, efuse_uuid, sizeof(efuse_uuid));
	memset(params, 0, sizeof(params));
	rc = tee_client_open_session(tee_ctx, &osarg, params);

	if (rc || osarg.ret) {
		pr_err("open_session failed err %d, ret=%d", rc, osarg.ret);
		ret = -1;
		goto free_shm;
	}
	pr_debug("open_session succeed! session = %lx\n",
			(unsigned long)osarg.session);

	/* 2: invoke cmd */
	memset(&arg, 0, sizeof(arg));
	arg.num_params = TEEC_PAYLOAD_REF_COUNT;
	arg.session = osarg.session;
	arg.func = TZCMD_EFUSE_READ; /* cmd id */

	memset(params, 0, sizeof(params));
	params[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
	params[0].u.value.a = fuse;
	params[1].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_OUTPUT;
	params[1].u.memref.shm = shm;
	params[1].u.memref.size = shm_size;
	params[1].u.memref.shm_offs = 0;
	params[2].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
	params[2].u.value.a = len;
	rc = tee_client_invoke_func(tee_ctx, &arg, params);
	if (rc) {
		pr_err("%s(): rc = %d\n", __func__, rc);
		ret = -1;
		goto close_session;
	}
	if (arg.ret != 0) {
		pr_err("%s(): ret %d, orig %d", __func__,
				arg.ret, arg.ret_origin);
		ret = -1;
		goto close_session;
	}

	pr_debug("%s(): show param[1] value= 0x%x\n", __func__,
		*(unsigned int *)params[1].u.memref.shm->kaddr);

	memcpy(data, params[1].u.memref.shm->kaddr, shm_size);
close_session:
	/* 3: close session */
	rc = tee_client_close_session(tee_ctx, osarg.session);
	if (rc != 0)
		pr_err("close_session failed err %d", rc);
free_shm:
	/* 4: free shm */
	tee_shm_free(shm);
close_context:
	/* 5: close context */
	tee_client_close_context(tee_ctx);
out:
	return ret;
}
#endif
