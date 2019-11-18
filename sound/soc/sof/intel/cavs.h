/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Cezary Rojewski <cezary.rojewski@intel.com>
 */

#ifndef __SOF_INTEL_CAVS_H
#define __SOF_INTEL_CAVS_H

#include "../sof-priv.h"

enum cavs_msg_target {
	CAVS_FW_GEN_MSG = 0,
	CAVS_MOD_MSG = 1
};

enum cavs_msg_direction {
	CAVS_MSG_REQUEST = 0,
	CAVS_MSG_REPLY = 1
};

enum cavs_global_msg_type {
	CAVS_GLB_CREATE_PIPELINE = 17,
	CAVS_GLB_DELETE_PIPELINE = 18,
	CAVS_GLB_SET_PIPELINE_STATE = 19,
	CAVS_GLB_GET_PIPELINE_STATE = 20,
	CAVS_GLB_NOTIFICATION = 27,
};

enum cavs_pipeline_state {
    INVALID,
    UNINITIALIZED,
    RESET,
    PAUSED,
    RUNNING,
    EOS,
    ERROR_STOP,
    SAVED,
    RESTORED,
};

union cavs_global_msg {
	u64 val;
	struct {
		union {
			u32 primary;
			struct {
				u32 rsvd:24;
				enum cavs_global_msg_type type:5;
				enum cavs_msg_direction dir:1;
				enum cavs_msg_target target:1;
			};

			/* pipeline management */
			struct {
				u32 ppl_mem_size:11;
				u32 ppl_priority:5;
				u32 instance_id:8;
			} create_ppl;
			struct {
				u32 rsvd:16;
				u32 instance_id:8;
			} ppl; /* generic ppl request */
			struct {
				u32 state:16;
				u32 ppl_id:8;
			} set_ppl_state;
		};
		union {
			u32 val;
			/* pipeline management */
			struct {
				u32 lp:1;
			} create_ppl;
			struct {
				u32 multi_ppl:1;
				u32 sync_stop_start:1;
			} set_ppl_state;
		} ext;
	};
} __packed;

enum cavs_module_msg_type {
	CAVS_MOD_LARGE_CONFIG_GET = 3,
};

union cavs_module_msg {
	u64 val;
	struct {
		union {
			u32 primary;
			struct {
				u32 module_id:16;
				u32 instance_id:8;
				enum cavs_module_msg_type type:5;
				enum cavs_msg_direction dir:1;
				enum cavs_msg_target target:1;
			};
		};
		union {
			u32 val;
			struct {
				u32 data_off_size:20;
				u32 large_param_id:8;
				u32 final_block:1;
				u32 init_block:1;
			} large_config;
		} ext;
	};
} __packed;

enum cavs_basefw_runtime_param {
	CAVS_BASEFW_FIRMWARE_CONFIG = 7,
};

enum cavs_notify_msg_type {
	CAVS_NOTIFY_FW_READY = 8,
};

union cavs_reply_msg {
	u64 val;
	struct {
		union {
			u32 primary;
			struct {
				u32 status:24;
				enum cavs_global_msg_type type:5;
				enum cavs_msg_direction dir:1;
				enum cavs_msg_target target:1;
			};
		};
		union {
			u32 val;
			/* pipeline managenet */
			struct {
				enum cavs_pipeline_state state:5;
			} get_ppl_state;

			/* module management */
			struct {
				u32 data_off_size:20;
				u32 large_param_id:8;
				u32 final_block:1;
				u32 init_block:1;
			} large_config;
		} ext;
	};
} __packed;

union cavs_notify_msg {
	u64 val;
	struct {
		union {
			u32 primary;
			struct {
				u32 rsvd:16;
				enum cavs_notify_msg_type notify_type:8;
				enum cavs_global_msg_type type:5;
				enum cavs_msg_direction dir:1;
				enum cavs_msg_target target:1;
			};
		};
		union {
			u32 val;
		} ext;
	};
} __packed;

#define CAVS_MSG(hdr) { .val = hdr }

#define CAVS_GLOBAL_REQUEST(msg_type) \
{	.type = CAVS_GLB_##msg_type, .dir = CAVS_MSG_REQUEST, \
	.target = CAVS_FW_GEN_MSG }
#define CAVS_MODULE_REQUEST(msg_type) \
{	.type = CAVS_MOD_##msg_type, .dir = CAVS_MSG_REQUEST, \
	.target = CAVS_MOD_MSG }

#define cavs_is_msg_reply(hdr) \
({ \
	union cavs_reply_msg __msg; \
	__msg.val = hdr; \
	__msg.dir == CAVS_MSG_REPLY; \
})

int cavs_ipc_process_reply(struct snd_sof_dev *sdev, u64 header);
int cavs_ipc_process_notification(struct snd_sof_dev *sdev, u64 header);

/* Generic cAVS sof handlers */
int cavs_fw_ready(struct snd_sof_dev *sdev, u32 msg_id);
int cavs_post_fw_run(struct snd_sof_dev *sdev);

/*
 * IPC messages
 */

/* pipeline managenet */
int cavs_ipc_create_pipeline(struct snd_sof_dev *sdev,
		u16 req_size, u8 priority,
		u8 instance_id, bool lp);
int cavs_ipc_delete_pipeline(struct snd_sof_dev *sdev, u8 instance_id);
int cavs_ipc_set_pipeline_state(struct snd_sof_dev *sdev, u8 instance_id,
		enum cavs_pipeline_state state);
int cavs_ipc_get_pipeline_state(struct snd_sof_dev *sdev, u8 instance_id);

/* module management */
int cavs_ipc_large_config_get(struct snd_sof_dev *sdev,
		u16 module_id, u8 instance_id,
		u32 data_size, u8 param_id,
		u32 **payload, size_t *bytes);

int cavs_ipc_fw_config_get(struct snd_sof_dev *sdev);

#endif
