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

int cavs_ipc_create_pipeline(struct snd_sof_dev *sdev,
		u16 req_size, u8 priority,
		u8 instance_id, bool lp)
{
	struct sof_ipc_message request = {0};
	union cavs_global_msg msg = CAVS_GLOBAL_REQUEST(CREATE_PIPELINE);
	int ret;

	msg.create_ppl.ppl_mem_size = req_size;
	msg.create_ppl.ppl_priority = priority;
	msg.create_ppl.instance_id = instance_id;
	msg.ext.create_ppl.lp = lp;
	request.header = msg.val;

	ret = sof_ipc_tx_message(sdev->ipc, request, NULL);
	if (ret < 0)
		dev_err(sdev->dev, "ipc: create pipeline fail, ret: %d\n", ret);
	return ret;
}

int cavs_ipc_delete_pipeline(struct snd_sof_dev *sdev, u8 instance_id)
{
	struct sof_ipc_message request = {0};
	union cavs_global_msg msg = CAVS_GLOBAL_REQUEST(DELETE_PIPELINE);
	int ret;

	msg.ppl.instance_id = instance_id;
	request.header = msg.val;

	ret = sof_ipc_tx_message(sdev->ipc, request, NULL);
	if (ret < 0)
		dev_err(sdev->dev, "ipc: delete pipeline fail, ret: %d\n", ret);
	return ret;
}

int cavs_ipc_set_pipeline_state(struct snd_sof_dev *sdev, u8 instance_id,
		enum cavs_pipeline_state state)
{
	struct sof_ipc_message request = {0};
	union cavs_global_msg msg = CAVS_GLOBAL_REQUEST(SET_PIPELINE_STATE);
	int ret;

	msg.set_ppl_state.ppl_id = instance_id;
	msg.set_ppl_state.state = state;
	request.header = msg.val;

	ret = sof_ipc_tx_message(sdev->ipc, request, NULL);
	if (ret < 0)
		dev_err(sdev->dev,
			"ipc: set pipeline state fail, ret: %d\n", ret);
	return ret;
}

int cavs_ipc_get_pipeline_state(struct snd_sof_dev *sdev, u8 instance_id)
{
	struct sof_ipc_message request = {0}, reply = {0};
	union cavs_global_msg msg = CAVS_GLOBAL_REQUEST(GET_PIPELINE_STATE);
	union cavs_reply_msg rsp;
	int ret;

	msg.ppl.instance_id = instance_id;
	request.header = msg.val;

	ret = sof_ipc_tx_message(sdev->ipc, request, &reply);
	if (ret < 0) {
		dev_err(sdev->dev,
			"ipc: get pipeline state fail, ret: %d\n", ret);
		return ret;
	}

	rsp.val = reply.header;
	return rsp.ext.get_ppl_state.state;
}

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

