#ifndef INCLUDES_TARANTOOL_BOX_PROMOTE_H
#define INCLUDES_TARANTOOL_BOX_PROMOTE_H
/*
 * Copyright 2010-2018, Tarantool AUTHORS, please see AUTHORS file.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * <COPYRIGHT HOLDER> OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include "tt_uuid.h"
#include "diag.h"
#include "fiber_cond.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct info_handler;

enum promote_msg_type {
	PROMOTE_MSG_BEGIN = 0,
	PROMOTE_MSG_STATUS,
	PROMOTE_MSG_SYNC,
	PROMOTE_MSG_SUCCESS,
	PROMOTE_MSG_ERROR,
	promote_msg_type_MAX,
};

struct promote_msg {
	int round_id;
	struct tt_uuid round_uuid;
	struct tt_uuid source_uuid;
	double ts;
	enum promote_msg_type type;
	int step;
	union {
		struct {
			int quorum;
			double timeout;
		} begin;
		struct {
			bool is_master;
		} status;
		struct {
			int code;
			const char *message;
		} error;
	};
};

int
promote_msg_decode(const char *data, struct promote_msg *msg);

void
promote_process(const struct promote_msg *msg);

int
box_ctl_promote(double timeout, int quorum);

void
box_ctl_promote_info(struct info_handler *info);

int
box_ctl_promote_reset(void);

int
box_ctl_promote_init(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* INCLUDES_TARANTOOL_BOX_PROMOTE_H */
