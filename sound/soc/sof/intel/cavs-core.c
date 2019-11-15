// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
//
// This file is provided under a dual BSD/GPLv2 license.  When using or
// redistributing this file, you may do so under either license.
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Cezary Rojewski <cezary.rojewski@intel.com>
//

#include "../ops.h"
#include "hda.h"
#include "cavs.h"

int cavs_ipc_process_reply(struct snd_sof_dev *sdev, u64 header)
{
	struct snd_sof_ipc_msg *msg = sdev->msg;
	union cavs_reply_msg rsp = CAVS_MSG(header);
	u32 offset = sdev->dsp_box.offset;

	msg->rx.header = header;

	dev_info(sdev->dev, "size of inc data: %lu\n", msg->rx.size);
	if (!rsp.status)
		sof_mailbox_read(sdev, offset, msg->rx.data, msg->rx.size);

	return 0;
}

int cavs_ipc_process_notification(struct snd_sof_dev *sdev, u64 header)
{
	union cavs_notify_msg msg = CAVS_MSG(header);
	int ret;

	switch (msg.notify_type) {
	case CAVS_NOTIFY_FW_READY:
		dev_info(sdev->dev, "FW READY %x\n", msg.primary);
		sdev->boot_complete = true;
		wake_up(&sdev->boot_wait);

		ret = sof_ops(sdev)->fw_ready(sdev, 0);
		if (ret < 0)
			return ret;
		break;

	default:
		dev_warn(sdev->dev, "ipc: Unhandled error msg=%x\n",
			msg.primary);
		break;
	}

	return 0;
}

int cavs_fw_ready(struct snd_sof_dev *sdev, u32 msg_id)
{
	int uplink, downlink;

	uplink = snd_sof_dsp_get_mailbox_offset(sdev);
	if (uplink < 0) {
		dev_err(sdev->dev, "error: no uplink offset\n");
		return uplink;
	}

	downlink = snd_sof_dsp_get_window_offset(sdev, 1);
	if (downlink < 0) {
		dev_err(sdev->dev, "error: no downlink offset\n");
		return downlink;
	}

	snd_sof_dsp_mailbox_init(sdev, uplink, PAGE_SIZE, downlink, PAGE_SIZE);

	return 0;
}
