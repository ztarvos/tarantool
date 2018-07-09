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
#include "box.h"
#include "replication.h"
#include "promote.h"
#include "error.h"
#include "msgpuck.h"
#include "xrow.h"
#include "space.h"
#include "schema.h"
#include "schema_def.h"
#include "txn.h"
#include "tuple.h"
#include "iproto_constants.h"
#include "opt_def.h"
#include "info.h"

static const char *promote_msg_type_strs[] = {
	"begin",
	"status",
	"sync",
	"success",
	"error",
};

static inline bool
promote_msg_is_mine(const struct promote_msg *msg)
{
	return tt_uuid_is_equal(&msg->source_uuid, &INSTANCE_UUID);
}

enum promote_role {
	PROMOTE_ROLE_INITIATOR = 0,
	PROMOTE_ROLE_OLD_MASTER,
	PROMOTE_ROLE_WATCHER
};

static const char *promote_role_strs[] = {
	"initiator",
	"old master",
	"watcher",
};

enum promote_phase {
	PROMOTE_PHASE_NON_ACTIVE = 0,
	PROMOTE_PHASE_ERROR,
	PROMOTE_PHASE_SUCCESS,
	PROMOTE_PHASE_INITIAL,
	PROMOTE_PHASE_WAIT_SUCCESS,
	PROMOTE_PHASE_WAIT_SYNC,
};

static const char *promote_phase_strs[] = {
	"non-active",
	"error",
	"success",
	"initial",
	"wait success",
	"wait sync",
};

static struct promote_state {
	int round_id;
	struct tt_uuid round_uuid;
	struct tt_uuid old_master_uuid;
	struct tt_uuid initiator_uuid;
	bool is_master_found;
	struct diag diag;
	struct fiber_cond on_change;
	enum promote_role role;
	enum promote_phase phase;
	char comment[DIAG_ERRMSG_MAX + 1];
	int quorum;
	int synced;
	double timeout;
	struct fiber *timer;
	struct trigger on_timer_stop;
	int watcher_count;
	int step;
} promote_state;

static inline bool
promote_is_in_terminal_phase(void)
{
	return promote_state.phase == PROMOTE_PHASE_NON_ACTIVE ||
	       promote_state.phase == PROMOTE_PHASE_ERROR ||
	       promote_state.phase == PROMOTE_PHASE_SUCCESS;
}

static inline bool
promote_is_in_progress(void)
{
	return !promote_is_in_terminal_phase() || promote_state.timer != NULL;
}

static inline bool
promote_is_cluster_readonly(void)
{
	return promote_state.watcher_count + 1 == replicaset.applier.total;
}

static inline bool
promote_is_this_round_msg(const struct promote_msg *msg)
{
	return !promote_is_in_terminal_phase() &&
	       tt_uuid_is_equal(&msg->round_uuid, &promote_state.round_uuid);
}

#define promote_comment(...) do { \
	snprintf(promote_state.comment, sizeof(promote_state.comment), \
		 __VA_ARGS__); \
	say_info(promote_state.comment); \
} while(0)

const struct opt_def promote_msg_begin_format[] = {
	OPT_DEF("quorum", OPT_UINT32, struct promote_msg, begin.quorum),
	OPT_DEF("timeout", OPT_FLOAT, struct promote_msg, begin.timeout),
	OPT_END,
};

const struct opt_def promote_msg_status_format[] = {
	OPT_DEF("is_master", OPT_BOOL, struct promote_msg, status.is_master),
	OPT_END,
};

const struct opt_def promote_msg_error_format[] = {
	OPT_DEF("code", OPT_UINT32, struct promote_msg, error.code),
	OPT_DEF("message", OPT_STRPTR, struct promote_msg, error.message),
	OPT_END,
};

static inline const char *
promote_msg_str(const struct promote_msg *msg)
{
	int offset = 0;
	char *buf = tt_static_buf();
	int len = TT_STATIC_BUF_LEN;

	offset += snprintf(buf, len, "{id: %d, round: '", msg->round_id);
	tt_uuid_to_string(&msg->round_uuid, buf + offset);
	offset += UUID_STR_LEN;
	offset += snprintf(buf + offset, len - offset, "', step: %d, source: '",
			   msg->step);
	tt_uuid_to_string(&msg->source_uuid, buf + offset);
	offset += UUID_STR_LEN;
	offset += snprintf(buf + offset, len - offset, "', ts: %f, type: '%s'",
			   msg->ts, promote_msg_type_strs[msg->type]);
	switch (msg->type) {
	case PROMOTE_MSG_BEGIN:
		offset += snprintf(buf + offset, len - offset, ", quorum: %d, "\
				   "timeout: %f}", msg->begin.quorum,
				   msg->begin.timeout);
		break;
	case PROMOTE_MSG_STATUS:
		offset += snprintf(buf + offset, len - offset, ", is_master: "\
				   "%d}", (int) msg->status.is_master);
		break;
	case PROMOTE_MSG_ERROR:
		offset += snprintf(buf + offset, len - offset, ", code: %d, "\
				   "message: '%s'}", msg->error.code,
				   msg->error.message);
		break;
	default:
		offset += snprintf(buf + offset, len - offset, "}");
		break;
	}
	return buf;
}

static const char *
promote_msg_encode(const struct promote_msg *msg, uint32_t *size_out)
{
	size_t size = 1024;
	char *data = region_alloc(&fiber()->gc, size);
	if (data == NULL) {
		diag_set(OutOfMemory, size, "region_alloc", "data");
		return NULL;
	}
	char *begin = data;
	/* Encode array header later. */
	data = mp_encode_array(data, 7);
	data = mp_encode_uint(data, msg->round_id);
	data = mp_encode_str(data, tt_uuid_str(&msg->round_uuid),
			     UUID_STR_LEN);
	data = mp_encode_uint(data, msg->step);
	data = mp_encode_str(data, tt_uuid_str(&msg->source_uuid),
			     UUID_STR_LEN);
	data = mp_encode_double(data, msg->ts);
	const char *type_str = promote_msg_type_strs[msg->type];
	data = mp_encode_str(data, type_str, strlen(type_str));
	switch(msg->type) {
	case PROMOTE_MSG_BEGIN:
		data = mp_encode_map(data, 2);
		data = mp_encode_str(data, "quorum", strlen("quorum"));
		data = mp_encode_uint(data, msg->begin.quorum);
		data = mp_encode_str(data, "timeout", strlen("timeout"));
		data = mp_encode_double(data, msg->begin.timeout);
		break;
	case PROMOTE_MSG_STATUS:
		data = mp_encode_map(data, 1);
		data = mp_encode_str(data, "is_master", strlen("is_master"));
		data = mp_encode_bool(data, msg->status.is_master);
		break;
	case PROMOTE_MSG_ERROR:
		data = mp_encode_map(data, 2);
		data = mp_encode_str(data, "code", strlen("code"));
		data = mp_encode_uint(data, msg->error.code);
		data = mp_encode_str(data, "message", strlen("message"));
		data = mp_encode_str(data, msg->error.message,
				     strlen(msg->error.message));
		break;
	default:
		data = mp_encode_nil(data);
		break;
	};
	*size_out = data - begin;
	assert(*size_out <= size);
	return begin;
}

int
promote_msg_decode(const char *data, struct promote_msg *msg)
{
	uint32_t size = mp_decode_array(&data);
	assert(size == 7 || size == 6);
	uint32_t len;
	struct region *region = &fiber()->gc;
	msg->round_id = (int) mp_decode_uint(&data);
	const char *str = mp_decode_str(&data, &len);
	if (tt_uuid_from_strl(str, len, &msg->round_uuid) != 0) {
		diag_set(ClientError, ER_WRONG_PROMOTION_RECORD,
			 BOX_PROMOTION_FIELD_ROUND_UUID, "invalid UUID");
		return -1;
	}
	msg->step = (int) mp_decode_uint(&data);
	str = mp_decode_str(&data, &len);
	if (tt_uuid_from_strl(str, len, &msg->source_uuid) != 0) {
		diag_set(ClientError, ER_WRONG_PROMOTION_RECORD,
			 BOX_PROMOTION_FIELD_SOURCE_UUID, "invalid UUID");
		return -1;
	}
	if (mp_read_double(&data, &msg->ts) != 0 || msg->ts < 0) {
		diag_set(ClientError, ER_WRONG_PROMOTION_RECORD,
			 BOX_PROMOTION_FIELD_TS, "wrong ts");
		return -1;
	}
	str = mp_decode_str(&data, &len);
	msg->type = STRN2ENUM(promote_msg_type, str, len);
	if (msg->type == promote_msg_type_MAX) {
		diag_set(ClientError, ER_WRONG_PROMOTION_RECORD,
			 BOX_PROMOTION_FIELD_TYPE, "wrong type");
		return -1;
	}

	switch(msg->type) {
	case PROMOTE_MSG_BEGIN:
		if (opts_decode(msg, promote_msg_begin_format, &data,
				ER_WRONG_PROMOTION_RECORD,
				BOX_PROMOTION_FIELD_VALUE, region, 2) != 0)
			return -1;
		break;
	case PROMOTE_MSG_STATUS:
		if (opts_decode(msg, promote_msg_status_format, &data,
				ER_WRONG_PROMOTION_RECORD,
				BOX_PROMOTION_FIELD_VALUE, region, 1) != 0)
			return -1;
		break;
	case PROMOTE_MSG_ERROR:
		if (opts_decode(msg, promote_msg_error_format, &data,
				ER_WRONG_PROMOTION_RECORD,
				BOX_PROMOTION_FIELD_VALUE, region, 2) != 0)
			return -1;
		break;
	default:
		if (mp_typeof(*data) != MP_NIL) {
			diag_set(ClientError, ER_WRONG_PROMOTION_RECORD,
				 BOX_PROMOTION_FIELD_VALUE,
				 tt_sprintf("'%s' has to have value nil",
					    promote_msg_type_strs[msg->type]));
			return -1;
		}
		mp_decode_nil(&data);
		break;
	};
	return 0;
}

static int
promote_send_f(va_list ap)
{
	const struct promote_msg *msg = va_arg(ap, const struct promote_msg *);
	size_t used = region_used(&fiber()->gc);
	struct request request;
	memset(&request, 0, sizeof(request));
	request.type = msg->type != PROMOTE_MSG_ERROR ?
		       IPROTO_INSERT : IPROTO_REPLACE;
	request.space_id = BOX_PROMOTION_ID;
	uint32_t size;
	request.tuple = promote_msg_encode(msg, &size);
	if (request.tuple == NULL)
		return -1;
	request.tuple_end = request.tuple + size;
	int rc = box_process_sys_dml(&request);
	region_truncate(&fiber()->gc, used);
	return rc;
}

static inline int
promote_send(const struct promote_msg *msg)
{
	/*
	 * Do nothing on recovery. If a message was sent on the
	 * previous launch, it would arrive among next rows.
	 */
	if (! box_is_configured())
		return 0;
	struct fiber *sender = fiber_new("promote sender", promote_send_f);
	if (sender == NULL)
		return -1;
	say_info("send promotion message: %s", promote_msg_str(msg));
	fiber_set_joinable(sender, true);
	fiber_start(sender, msg);
	int rc = fiber_join(sender);
	if (rc != 0) {
		say_info("promotion message has not sent: %s",
			 box_error_message(box_error_last()));
	}
	return rc;
}

static inline void
promote_msg_create(struct promote_msg *msg, enum promote_msg_type type)
{
	msg->round_id = promote_state.round_id;
	msg->round_uuid = promote_state.round_uuid;
	msg->source_uuid = INSTANCE_UUID;
	msg->ts = fiber_time();
	msg->type = type;
	msg->step = promote_state.step + 1;
}

static inline int
promote_send_begin(int quorum, double timeout)
{
	struct promote_msg msg;
	promote_msg_create(&msg, PROMOTE_MSG_BEGIN);
	tt_uuid_create(&msg.round_uuid);
	msg.begin.quorum = quorum;
	msg.begin.timeout = timeout;
	msg.round_id++;
	msg.step = 1;
	return promote_send(&msg);
}

static inline int
promote_send_status(void)
{
	struct promote_msg msg;
	promote_msg_create(&msg, PROMOTE_MSG_STATUS);
	msg.status.is_master = ! box_is_ro();
	return promote_send(&msg);
}

static inline int
promote_send_sync(void)
{
	struct promote_msg msg;
	promote_msg_create(&msg, PROMOTE_MSG_SYNC);
	return promote_send(&msg);
}

static inline int
promote_send_success(void)
{
	struct promote_msg msg;
	promote_msg_create(&msg, PROMOTE_MSG_SUCCESS);
	return promote_send(&msg);
}

static inline int
promote_send_error(void)
{
	struct promote_msg msg;
	promote_msg_create(&msg, PROMOTE_MSG_ERROR);
	struct error *e = box_error_last();
	msg.error.code = box_error_code(e);
	msg.error.message = box_error_message(e);
	return promote_send(&msg);
}

static inline int
promote_send_out_of_bound_error(int round_id, const struct tt_uuid *round_uuid,
				int step)
{
	struct promote_msg msg;
	promote_msg_create(&msg, PROMOTE_MSG_ERROR);
	msg.round_id = round_id;
	msg.round_uuid = *round_uuid;
	struct error *e = box_error_last();
	msg.error.code = box_error_code(e);
	msg.error.message = box_error_message(e);
	msg.step = step;
	return promote_send(&msg);
}

static void
promote_on_timer_stop(struct trigger *t, void *fiber)
{
	(void) t;
	say_info("promotion timer is stopped");
	if (fiber == promote_state.timer)
		promote_state.timer = NULL;
}


static int
promote_timer_f(va_list ap)
{
	(void) ap;
	assert(promote_state.timeout >= 0);
	fiber_set_cancellable(true);
	double timeout = promote_state.timeout;
	double start = fiber_clock();
	while (fiber_cond_wait_timeout(&promote_state.on_change,
				       timeout) == 0) {
		if (promote_is_in_terminal_phase() || fiber_is_cancelled())
			return 0;
		timeout -= fiber_clock() - start;
		start = fiber_clock();
	}
	if (promote_is_in_terminal_phase() || fiber_is_cancelled())
		return 0;
	diag_set(ClientError, ER_TIMEOUT);
	promote_state.step++;
	promote_send_error();
	return 0;
}

static int
promote_start_timer(void)
{
	assert(promote_state.timer == NULL);
	promote_state.timer = fiber_new("promote timer", promote_timer_f);
	if (promote_state.timer == NULL)
		return -1;
	say_info("start promotion timer for %f seconds", promote_state.timeout);
	trigger_create(&promote_state.on_timer_stop, promote_on_timer_stop,
		       NULL, NULL);
	trigger_add(&promote_state.timer->on_stop,
		    &promote_state.on_timer_stop);
	fiber_start(promote_state.timer);
	return 0;
}

int
box_ctl_promote(double timeout, int quorum)
{
	if (quorum < 0)
		quorum = replicaset.applier.total;
	/* Stage 1: broadcast BEGIN. */
	if (! box_is_ro()) {
		diag_set(ClientError, ER_PROMOTE, "non-initialized",
			 "the initiator is already master");
		return -1;
	}
	if (promote_is_in_progress()) {
		diag_set(ClientError, ER_PROMOTE_EXISTS);
		return -1;
	}
	if (quorum <= replicaset.applier.total / 2) {
		diag_set(ClientError, ER_PROMOTE, "non-initialized",
			 tt_sprintf("too small quorum, expected > %d, "\
				    "but got %d", replicaset.applier.total / 2,
				    quorum));
		return -1;
	}
	if (promote_send_begin(quorum, timeout) != 0)
		return -1;

	/* Stage 2: wait for SUCCESS from the old master. */
	while (promote_state.phase != PROMOTE_PHASE_SUCCESS) {
		fiber_cond_wait(&promote_state.on_change);
		if (promote_state.phase == PROMOTE_PHASE_ERROR) {
			assert(! diag_is_empty(&promote_state.diag));
			diag_move(&promote_state.diag, diag_get());
			return -1;
		}
	}
	return 0;
}

static inline int
promote_clean_round(uint32_t id, uint32_t *next_id)
{
	if (promote_is_in_progress()) {
		diag_set(ClientError, ER_PROMOTE_EXISTS);
		return -1;
	}
	*next_id = id;
	char buf[16];
	char *key = mp_encode_array(buf, 1);
	mp_encode_uint(key, id);
	struct index *pk = space_index(space_by_id(BOX_PROMOTION_ID), 0);
	if (index_count(pk, ITER_ALL, NULL, 0) == 0)
		return 0;
	struct request request;
	memset(&request, 0, sizeof(request));
	request.type = IPROTO_DELETE;
	request.space_id = BOX_PROMOTION_ID;
	struct iterator *it = index_create_iterator(pk, ITER_GE, key, 1);
	if (it == NULL)
		return -1;
	if (box_txn_begin() != 0) {
		iterator_delete(it);
		return -1;
	}
	struct tuple *t;
	int rc;
	while ((rc = iterator_next(it, &t)) == 0 && t != NULL) {
		uint32_t key_size;
		char *key2 = tuple_extract_key(t, pk->def->key_def, &key_size);
		if (key2 == NULL)
			goto rollback;
		if (key_compare(key2, key, pk->def->key_def) != 0) {
			tuple_field_u32(t, BOX_PROMOTION_FIELD_ID, next_id);
			break;
		}
		request.key = key2;
		request.key_end = key2 + key_size;
		if (box_process_sys_dml(&request) != 0)
			goto rollback;
	}
	if (rc != 0 || box_txn_commit() != 0)
		goto rollback;
	iterator_delete(it);
	return 0;
rollback:
	box_txn_rollback();
	iterator_delete(it);
	return -1;
}

int
box_ctl_promote_reset(void)
{
	uint32_t id, next_id = 0;
	do {
		id = next_id;
		if (promote_clean_round(id, &next_id) != 0)
			return -1;
	} while (id != next_id);
	promote_state.phase = PROMOTE_PHASE_NON_ACTIVE;
	return 0;
}

void
box_ctl_promote_info(struct info_handler *info)
{
	struct promote_state *s = &promote_state;
	info_begin(info);
	if (s->phase == PROMOTE_PHASE_NON_ACTIVE ||
	    s->phase == PROMOTE_PHASE_INITIAL) {
		info_end(info);
		return;
	}
	info_append_int(info, "round_id", s->round_id);
	info_append_str(info, "round_uuid",
			tt_uuid_str(&s->round_uuid));
	info_append_str(info, "initiator_uuid",
			tt_uuid_str(&s->initiator_uuid));
	info_append_str(info, "role", promote_role_strs[s->role]);
	info_append_str(info, "phase", promote_phase_strs[s->phase]);
	info_append_str(info, "comment", s->comment);
	if (s->is_master_found) {
		info_append_str(info, "old_master_uuid",
				tt_uuid_str(&s->old_master_uuid));
	}
	info_append_int(info, "quorum", s->quorum);
	info_append_double(info, "timeout", s->timeout);
	info_end(info);
}

void
promote_process(const struct promote_msg *msg)
{
	if (box_is_configured()) {
		say_info("promotion message has %s: %s",
			 promote_msg_is_mine(msg) ? "commited" : "received",
			 promote_msg_str(msg));
	} else {
		say_info("promotion message has recovered: %s",
			 promote_msg_str(msg));
	}
	if (promote_is_in_terminal_phase() &&
	    msg->round_id <= promote_state.round_id &&
	    msg->type == PROMOTE_MSG_BEGIN) {
		diag_set(ClientError, ER_PROMOTE, tt_uuid_str(&msg->round_uuid),
			 tt_sprintf("outdated round id: expected > %d, but "\
				    "got %d", promote_state.round_id,
				    msg->round_id));
		promote_send_out_of_bound_error(msg->round_id, &msg->round_uuid,
						msg->step + 1);
		return;
	}
	if (!promote_is_this_round_msg(msg) && msg->type != PROMOTE_MSG_BEGIN) {
		if (msg->type == PROMOTE_MSG_ERROR ||
		    msg->round_id <= promote_state.round_id)
			return;
		diag_set(ClientError, ER_PROMOTE, tt_uuid_str(&msg->round_uuid),
			 tt_sprintf("unexpected message '%s'",
				    promote_msg_type_strs[msg->type]));
		promote_send_out_of_bound_error(msg->round_id, &msg->round_uuid,
						msg->step + 1);
		return;
	}
	promote_state.step = msg->step;
	switch (msg->type) {
	case PROMOTE_MSG_BEGIN:
		if (promote_state.timer != NULL) {
			assert(! box_is_configured());
			fiber_cancel(promote_state.timer);
			while (promote_state.timer != NULL)
				fiber_sleep(0);
		}
		promote_state.round_id = msg->round_id;
		promote_state.round_uuid = msg->round_uuid;
		promote_state.old_master_uuid = uuid_nil;
		promote_state.initiator_uuid = msg->source_uuid;
		promote_state.is_master_found = false;
		diag_clear(&promote_state.diag);
		promote_state.quorum = msg->begin.quorum;
		promote_state.timeout = msg->begin.timeout;
		/* 1 - synced with itself. */
		promote_state.synced = 1;
		promote_state.watcher_count = 0;
		promote_state.phase = PROMOTE_PHASE_INITIAL;
		if (promote_start_timer() != 0) {
			promote_send_error();
			break;
		}
		if (promote_msg_is_mine(msg)) {
			promote_state.role = PROMOTE_ROLE_INITIATOR;
			promote_state.phase = PROMOTE_PHASE_WAIT_SUCCESS;
			promote_comment("promotion is started, my promotion "\
					"role is %s, waiting for demotion of "\
					"the old master",
					promote_role_strs[promote_state.role]);
		} else if (! box_is_ro()) {
			promote_state.role = PROMOTE_ROLE_OLD_MASTER;
			promote_state.phase = PROMOTE_PHASE_WAIT_SYNC;
			promote_comment("promotion is started, my promotion "\
					"role is %s, waiting for at least %d "\
					"syncs",
					promote_role_strs[promote_state.role],
					promote_state.quorum - 1);
			promote_send_status();
		} else {
			promote_state.role = PROMOTE_ROLE_WATCHER;
			promote_state.phase = PROMOTE_PHASE_WAIT_SYNC;
			promote_comment("promotion is started, my promotion "\
					"role is %s, waiting for sync with "\
					"the old master",
					promote_role_strs[promote_state.role]);
			promote_send_status();
		}
		break;

	case PROMOTE_MSG_STATUS:
		/* Recovery the instance read_only flag. */
		if (! box_is_configured() && promote_msg_is_mine(msg)) {
			box_set_ro(msg->status.is_master);
			box_expose_ro();
		}
		if (msg->status.is_master) {
			if (! promote_state.is_master_found) {
				promote_state.old_master_uuid =
					msg->source_uuid;
				promote_state.is_master_found = true;
				if (promote_state.role !=
				    PROMOTE_ROLE_OLD_MASTER)
					break;
				if (promote_msg_is_mine(msg)) {
					promote_send_sync();
					break;
				}
			}
			const char *r, *m1, *m2;
			r = tt_uuid_str(&msg->round_uuid);
			m1 = tt_uuid_str(&msg->source_uuid);
			m2 = tt_uuid_str(&promote_state.old_master_uuid);
			if (strcmp(m1, m2) > 0)
				SWAP(m1, m2);
			diag_set(ClientError, ER_PROMOTE, r,
				 tt_sprintf("two masters exist: '%s' and '%s'",
					    m1, m2));
			promote_send_error();
			break;
		}
		++promote_state.watcher_count;
		if (promote_state.role != PROMOTE_ROLE_INITIATOR ||
		    !promote_is_cluster_readonly())
			break;
		promote_comment("the cluster is completely readonly, promote "\
				"is run with no syncing with an old master");
		/*
		 * The cluster is readonly and all slaves are
		 * available. Then the promotion is allowed. But
		 * the initiator plays for an old master.
		 */
		promote_send_sync();
		break;

	case PROMOTE_MSG_SYNC:
		switch (promote_state.role) {
		case PROMOTE_ROLE_OLD_MASTER:
			assert(promote_msg_is_mine(msg));
			promote_comment("old master entered readonly mode to "\
					"sync with slaves");
			box_set_ro(true);
			box_expose_ro();
			break;
		case PROMOTE_ROLE_WATCHER:
			promote_send_success();
			break;
		case PROMOTE_ROLE_INITIATOR:
			if (! promote_is_cluster_readonly())
				promote_send_success();
			else
				assert(promote_msg_is_mine(msg));
			break;
		default:
			unreachable();
		}
		break;

	case PROMOTE_MSG_SUCCESS:
		switch (promote_state.role) {
		case PROMOTE_ROLE_OLD_MASTER:
			if (promote_msg_is_mine(msg)) {
				promote_state.phase = PROMOTE_PHASE_SUCCESS;
				promote_comment("the old master is demoted "\
						"completely");
			} else if (++promote_state.synced ==
				   promote_state.quorum) {
				promote_send_success();
			}
			break;
		case PROMOTE_ROLE_INITIATOR:
			if (tt_uuid_is_equal(&msg->source_uuid,
					     &promote_state.old_master_uuid) ||
			    (promote_is_cluster_readonly() &&
			     ++promote_state.synced == promote_state.quorum)) {
				promote_comment("the new master is promoted");
				promote_state.phase = PROMOTE_PHASE_SUCCESS;
				box_set_ro(false);
				box_expose_ro();
			}
			break;
		case PROMOTE_ROLE_WATCHER:
			if (promote_msg_is_mine(msg)) {
				promote_state.phase = PROMOTE_PHASE_SUCCESS;
				promote_comment("the watcher has voted and "\
						"left the round");
			}
			break;
		default:
			unreachable();
		}
		break;

	case PROMOTE_MSG_ERROR:
		if (promote_state.role == PROMOTE_ROLE_OLD_MASTER &&
		    promote_state.phase == PROMOTE_PHASE_WAIT_SYNC) {
			promote_comment("the old master is back in read-write "\
					"mode due to the error: %s",
					 msg->error.message);
			box_set_ro(false);
			box_expose_ro();
		} else {
			promote_comment("the round failed due to the error: %s",
					msg->error.message);
		}
		promote_state.phase = PROMOTE_PHASE_ERROR;
		promote_state.round_id =
			MAX(promote_state.round_id, msg->round_id) + 1;
		box_error_raise(msg->error.code, "%s", msg->error.message);
		diag_move(diag_get(), &promote_state.diag);
		break;
	default:
		break;
	}
	fiber_cond_broadcast(&promote_state.on_change);
}

int
box_ctl_promote_init(void)
{
	memset(&promote_state, 0, sizeof(promote_state));
	fiber_cond_create(&promote_state.on_change);
	return 0;
}
