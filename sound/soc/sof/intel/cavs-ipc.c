// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
//
// This file is provided under a dual BSD/GPLv2 license.  When using or
// redistributing this file, you may do so under either license.
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Cezary Rojewski <cezary.rojewski@intel.com>
//

#include "cavs.h"

int cavs_ipc_large_config_get(struct snd_sof_dev *sdev,
		u16 module_id, u8 instance_id,
		u32 data_size, u8 param_id,
		u32 **payload, size_t *bytes)
{
	struct sof_ipc_message request, reply = {0};
	union cavs_module_msg msg = CAVS_MODULE_REQUEST(LARGE_CONFIG_GET);
	union cavs_reply_msg rsp;
	unsigned int *buf;
	int ret;

	reply.data = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!reply.data)
		return -ENOMEM;

	msg.module_id = module_id;
	msg.instance_id = instance_id;
	msg.ext.large_config.data_off_size = data_size;
	msg.ext.large_config.large_param_id = param_id;
	msg.ext.large_config.final_block = 1;
	msg.ext.large_config.init_block = 1;

	request.header = msg.val;
	request.data = *payload;
	request.size = *bytes;
	reply.size = PAGE_SIZE;

	ret = sof_ipc_tx_message(sdev->ipc, request, &reply);
	if (ret < 0)
		dev_err(sdev->dev, "ipc:  large config get fail, ret: %d\n", ret);

	rsp.val = reply.header;
	buf = krealloc(reply.data,
			rsp.ext.large_config.data_off_size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	*payload = buf;
	*bytes = rsp.ext.large_config.data_off_size;

	return ret;
}

int cavs_ipc_fw_config_get(struct snd_sof_dev *sdev)
{
	size_t bytes = 0;
	u8 *payload = NULL;
	int ret;

	ret = cavs_ipc_large_config_get(sdev, 0, 0, 0,
			CAVS_BASEFW_FIRMWARE_CONFIG,
			(u32**)&payload, &bytes);

	return 0;
}

