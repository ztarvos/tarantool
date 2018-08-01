#ifndef TARANTOOL_BOX_FKEY_H_INCLUDED
#define TARANTOOL_BOX_FKEY_H_INCLUDED
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
#include <stdbool.h>
#include <stdint.h>

#include "space.h"

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

struct sqlite3;

enum fkey_action {
	FKEY_NO_ACTION = 0,
	FKEY_ACTION_SET_NULL,
	FKEY_ACTION_SET_DEFAULT,
	FKEY_ACTION_CASCADE,
	FKEY_ACTION_RESTRICT,
	fkey_action_MAX
};

enum fkey_match {
	FKEY_MATCH_SIMPLE = 0,
	FKEY_MATCH_PARTIAL,
	FKEY_MATCH_FULL,
	fkey_match_MAX
};

extern const char *fkey_action_strs[];

extern const char *fkey_match_strs[];

/** Structure describing field dependencies for foreign keys. */
struct field_link {
	uint32_t parent_field;
	uint32_t child_field;
};

/** Definition of foreign key constraint. */
struct fkey_def {
	/** Id of space containing the REFERENCES clause (child). */
	uint32_t child_id;
	/** Id of space that the key points to (parent). */
	uint32_t parent_id;
	/** Number of fields in this key. */
	uint32_t field_count;
	/** True if constraint checking is deferred till COMMIT. */
	bool is_deferred;
	/** Match condition for foreign key. SIMPLE by default. */
	enum fkey_match match;
	/** ON DELETE action. NO ACTION by default. */
	enum fkey_action on_delete;
	/** ON UPDATE action. NO ACTION by default. */
	enum fkey_action on_update;
	/** Mapping of fields in child to fields in parent. */
	struct field_link *links;
	/** Name of the constraint. */
	char name[0];
};

/** Structure representing foreign key relationship. */
struct fkey {
	struct fkey_def *def;
	/** Index id of referenced index in parent space. */
	uint32_t index_id;
	/** Triggers for actions. */
	struct sql_trigger *on_delete_trigger;
	struct sql_trigger *on_update_trigger;
	/** Links for parent and child lists. */
	struct rlist parent_link;
	struct rlist child_link;
};

/**
 * FIXME: as SQLite legacy temporary we use such mask throught
 * SQL code. It should be replaced later with regular
 * mask from column_mask.h
 */
#define FKEY_MASK(x) (((x)>31) ? 0xffffffff : ((uint64_t)1<<(x)))

/**
 * Alongside with struct fkey_def itself, we reserve memory for
 * string containing its name and for array of links.
 * Memory layout:
 * +-------------------------+ <- Allocated memory starts here
 * |     struct fkey_def     |
 * |-------------------------|
 * |        name + \0        |
 * |-------------------------|
 * |          links          |
 * +-------------------------+
 */
static inline size_t
fkey_def_sizeof(uint32_t links_count, uint32_t name_len)
{
	return sizeof(struct fkey) + links_count * sizeof(struct field_link) +
	       name_len + 1;
}

static inline bool
fkey_is_self_referenced(const struct fkey_def *fkey)
{
	return fkey->child_id == fkey->parent_id;
}

/** Release memory for foreign key and its triggers, if any. */
void
fkey_delete(struct fkey *fkey);

#if defined(__cplusplus)
} /* extern "C" */
#endif /* __cplusplus */

#endif /* TARANTOOL_BOX_FKEY_H_INCLUDED */
