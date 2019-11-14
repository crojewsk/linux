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

static const struct snd_sof_debugfs_map cnl_debug_map[] = {
	{ "hda", HDA_DSP_HDA_BAR, 0, 0x4000, SOF_DEBUGFS_ACCESS_ALWAYS },
	{ "pp", HDA_DSP_PP_BAR, 0, 0x1000, SOF_DEBUGFS_ACCESS_ALWAYS },
	{ "dsp", HDA_DSP_BAR, 0, 0x10000, SOF_DEBUGFS_ACCESS_ALWAYS },
};

static void cnl_ipc_dump(struct snd_sof_dev *sdev)
{
	u32 hipcctl;
	u32 hipcida;
	u32 hipctdr;

	hda_ipc_irq_dump(sdev);

	/* read IPC status */
	hipcida = snd_sof_dsp_read(sdev, HDA_DSP_BAR, CNL_DSP_REG_HIPCIDA);
	hipcctl = snd_sof_dsp_read(sdev, HDA_DSP_BAR, CNL_DSP_REG_HIPCCTL);
	hipctdr = snd_sof_dsp_read(sdev, HDA_DSP_BAR, CNL_DSP_REG_HIPCTDR);

	/* dump the IPC regs */
	/* TODO: parse the raw msg */
	dev_err(sdev->dev,
		"error: host status 0x%8.8x dsp status 0x%8.8x mask 0x%8.8x\n",
		hipcida, hipctdr, hipcctl);
}

static int cnl_send_msg(struct snd_sof_dev *sdev, struct snd_sof_ipc_msg *msg)
{
	u32 offset = sdev->host_box.offset;
	u32 primary = (msg->tx.header & UINT_MAX);
	u32 extension = (msg->tx.header >> 32);

	dev_info(sdev->dev, "%s primary: 0x%x extension: 0x%x\n", __func__, primary, extension);

	if (msg->tx.size)
		sof_mailbox_write(sdev, offset, msg->tx.data, msg->tx.size);

	snd_sof_dsp_write(sdev, HDA_DSP_BAR, CNL_DSP_REG_HIPCIDD,
			  extension);
	snd_sof_dsp_write(sdev, HDA_DSP_BAR, CNL_DSP_REG_HIPCIDR,
			  primary | CNL_DSP_REG_HIPCIDR_BUSY);

	return 0;
}


static void cnl_ipc_host_done(struct snd_sof_dev *sdev)
{
	/*
	 * clear busy interrupt to tell dsp controller this
	 * interrupt has been accepted, not trigger it again
	 */
	snd_sof_dsp_update_bits_forced(sdev, HDA_DSP_BAR,
				       CNL_DSP_REG_HIPCTDR,
				       CNL_DSP_REG_HIPCTDR_BUSY,
				       CNL_DSP_REG_HIPCTDR_BUSY);
	/*
	 * set done bit to ack dsp the msg has been
	 * processed and send reply msg to dsp
	 */
	snd_sof_dsp_update_bits_forced(sdev, HDA_DSP_BAR,
				       CNL_DSP_REG_HIPCTDA,
				       CNL_DSP_REG_HIPCTDA_DONE,
				       CNL_DSP_REG_HIPCTDA_DONE);
}

static void cnl_ipc_dsp_done(struct snd_sof_dev *sdev)
{
	/*
	 * set DONE bit - tell DSP we have received the reply msg
	 * from DSP, and processed it, don't send more reply to host
	 */
	snd_sof_dsp_update_bits_forced(sdev, HDA_DSP_BAR,
				       CNL_DSP_REG_HIPCIDA,
				       CNL_DSP_REG_HIPCIDA_DONE,
				       CNL_DSP_REG_HIPCIDA_DONE);

	/* unmask Done interrupt */
	snd_sof_dsp_update_bits(sdev, HDA_DSP_BAR,
				CNL_DSP_REG_HIPCCTL,
				CNL_DSP_REG_HIPCCTL_DONE,
				CNL_DSP_REG_HIPCCTL_DONE);
}

irqreturn_t cavs_cnl_irq_thread(int irq, void *context)
{
	struct snd_sof_dev *sdev = context;
	u32 hipcida, hipcidr;
	u32 hipctdr, hipctdd;
	u64 header;
	bool ipc_irq = false;

	hipcida = snd_sof_dsp_read(sdev, HDA_DSP_BAR, CNL_DSP_REG_HIPCIDA);
	hipctdr = snd_sof_dsp_read(sdev, HDA_DSP_BAR, CNL_DSP_REG_HIPCTDR);
	hipctdd = snd_sof_dsp_read(sdev, HDA_DSP_BAR, CNL_DSP_REG_HIPCTDD);
	hipcidr = snd_sof_dsp_read(sdev, HDA_DSP_BAR, CNL_DSP_REG_HIPCIDR);

	/* is this a reply message from the DSP */
	if (hipcida & CNL_DSP_REG_HIPCIDA_DONE) {
		dev_warn(sdev->dev,
			 "ipc: firmware response, msg:0x%x, msg_ext:0x%x\n",
			 hipcida, hipcidr);

		/* mask Done interrupt */
		snd_sof_dsp_update_bits(sdev, HDA_DSP_BAR,
					CNL_DSP_REG_HIPCCTL,
					CNL_DSP_REG_HIPCCTL_DONE, 0);

		cnl_ipc_dsp_done(sdev);
		ipc_irq = true;
	}

	/* is this a new message from DSP */
	if (hipctdr & CNL_DSP_REG_HIPCTDR_BUSY) {
		header = ((u64)hipctdd << 32) | hipctdr;

		dev_warn(sdev->dev,
			 "ipc: firmware initiated, msg:0x%x, msg_ext:0x%x\n",
			 hipctdr, hipctdd);

		if (cavs_is_msg_reply(header)) {
			cavs_ipc_process_reply(sdev, header);
			snd_sof_ipc_reply(sdev, 0);
		} else {
			dev_info(sdev->dev, "IPC irq: Notification from firmware\n");
			cavs_ipc_process_notification(sdev, header);
		}

		cnl_ipc_host_done(sdev);
		ipc_irq = true;
	}

	if (!ipc_irq) {
		/*
		 * This interrupt is not shared so no need to return IRQ_NONE.
		 */
		dev_dbg_ratelimited(sdev->dev,
				    "nothing to do in IPC IRQ thread\n");
	}

	/* re-enable IPC interrupt */
	snd_sof_dsp_update_bits(sdev, HDA_DSP_BAR, HDA_DSP_REG_ADSPIC,
				HDA_DSP_ADSPIC_IPC, HDA_DSP_ADSPIC_IPC);

	return IRQ_HANDLED;
}

const struct snd_sof_dsp_ops sof_cavs_cnl_ops = {
	.probe		= hda_dsp_probe,
	.remove		= hda_dsp_remove,

	.write		= sof_io_write,
	.read		= sof_io_read,
	.write64	= sof_io_write64,
	.read64		= sof_io_read64,

	.block_read	= sof_block_read,
	.block_write	= sof_block_write,

	.irq_handler	= hda_dsp_ipc_irq_handler,
	.irq_thread	= cavs_cnl_irq_thread,

	.send_msg	= cnl_send_msg,
	.fw_ready	= cavs_fw_ready,
	.get_mailbox_offset = hda_dsp_ipc_get_mailbox_offset,
	.get_window_offset = hda_dsp_ipc_get_window_offset,

	.ipc_msg_data	= hda_ipc_msg_data,
	.ipc_pcm_params	= hda_ipc_pcm_params,

	.debug_map	= cnl_debug_map,
	.debug_map_count	= ARRAY_SIZE(cnl_debug_map),
	.dbg_dump	= hda_dsp_dump,
	.ipc_dump	= cnl_ipc_dump,

	.pcm_open	= hda_dsp_pcm_open,
	.pcm_close	= hda_dsp_pcm_close,
	.pcm_hw_params	= hda_dsp_pcm_hw_params,
	.pcm_hw_free	= hda_dsp_stream_hw_free,
	.pcm_trigger	= hda_dsp_pcm_trigger,
	.pcm_pointer	= hda_dsp_pcm_pointer,

	.load_firmware = snd_sof_load_firmware_raw,
	.pre_fw_run = hda_dsp_pre_fw_run,
	.run = hda_dsp_cl_boot_firmware,
	.post_fw_run = hda_dsp_post_fw_run,

	.core_power_up = hda_dsp_enable_core,
	.core_power_down = hda_dsp_core_reset_power_down,

	.trace_init = hda_dsp_trace_init,
	.trace_release = hda_dsp_trace_release,
	.trace_trigger = hda_dsp_trace_trigger,

	.drv		= skl_dai,
	.num_drv	= SOF_SKL_NUM_DAIS,

	.suspend		= hda_dsp_suspend,
	.resume			= hda_dsp_resume,
	.runtime_suspend	= hda_dsp_runtime_suspend,
	.runtime_resume		= hda_dsp_runtime_resume,
	.runtime_idle		= hda_dsp_runtime_idle,
	.set_hw_params_upon_resume = hda_dsp_set_hw_params_upon_resume,
	.set_power_state	= hda_dsp_set_power_state,

	.hw_info =	SNDRV_PCM_INFO_MMAP |
			SNDRV_PCM_INFO_MMAP_VALID |
			SNDRV_PCM_INFO_INTERLEAVED |
			SNDRV_PCM_INFO_PAUSE |
			SNDRV_PCM_INFO_NO_PERIOD_WAKEUP,
};
EXPORT_SYMBOL(sof_cavs_cnl_ops);

const struct sof_intel_dsp_desc cavs_cnl_chip_info = {
	/* Cannonlake */
	.cores_num = 4,
	.init_core_mask = 1,
	.cores_mask = HDA_DSP_CORE_MASK(0) |
			HDA_DSP_CORE_MASK(1) |
			HDA_DSP_CORE_MASK(2) |
			HDA_DSP_CORE_MASK(3),
	.ipc_req = CNL_DSP_REG_HIPCIDR,
	.ipc_req_mask = CNL_DSP_REG_HIPCIDR_BUSY,
	.ipc_ack = CNL_DSP_REG_HIPCIDA,
	.ipc_ack_mask = CNL_DSP_REG_HIPCIDA_DONE,
	.ipc_ctl = CNL_DSP_REG_HIPCCTL,
	.rom_init_timeout	= 300,
	.ssp_count = 0,
	.ssp_base_offset = 0,
};
EXPORT_SYMBOL(cavs_cnl_chip_info);
