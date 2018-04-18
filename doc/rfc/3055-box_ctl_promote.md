# Replicaset master promotion

* **Status**: In progress
* **Start date**: 02-03-2018
* **Authors**: Vladislav Shpilevoy @Gerold103 \<v.shpilevoy@tarantool.org\>,
Konstantin Osipov @kostja \<kostja@tarantool.org\>
* **Issues**: [#3055](https://github.com/tarantool/tarantool/issues/3055),
[#2625](https://github.com/tarantool/tarantool/issues/2625)

## Summary

Replicaset master promotion is a procedure of atomic making one slave be new
master, and an old master be slave in a fullmesh master-slave replicaset. Master
is a replica in read-write mode. Slave is a replica in read-only mode.

Master promotion has API:
```Lua
--
-- Called on a slave promotes its role to master, demoting an old
-- one to slave. Called on a master returns an error.
-- @param opts Options.
--        * timeout - the time in which a promotion must be
--          finished;
--        * quorum - before an old master demotion its data must
--          be synced with no less than quorum slave count,
--          including the being promoted one.
--
-- @retval true Promotion is started.
-- @retval nil, error Can not start promotion.
--
box.ctl.promote(opts)

--
-- Status of the latest finished or the currently working
-- promotion round.
-- @retval nil Promote() was not called since the instance has
--         been started, or it was started on another instance,
--         that could not sent promotion info to the current
--         instance.
-- @retval status A table with the format:
--    {
--         round_uuid = <Promotion round UUID, generated on
--                       initiator side>,
--         promote_uuid = <UUID of the promotion initiator>,
--         demote_uuid = <UUID of the old master>,
--         state = <Human readable status of the algorithm - it
--                  can be finished ok, finished with an error,
--                  not finished being on one of algorithm steps>,
--         step_number = <Promotion round step identifier>,
--         error = <If the promotion is finished with an error,
--                  then here the error object is stored>,
--         is_finished = <True, if the promotion round is
--                        finished>,
--         start_ts = <Time of the promotion start on initiator
--                     clock>,
--         update_ts = <Time of the last update of this promotion
--                      round by last sender clock>,
--         end_ts = <Time of the promotion finish on initiator
--                   clock, if it is finished>,
--         timeout = <Timeout of the promotion round>,
--         quorum = <Requested quorum>,
--    }
--
box.ctl.promotion_status()

--
-- Remove info about all promotions from the entire cluster. It
-- can be useful, when it is necessary to use a role specified in
-- box.cfg{} even if it contradicts with a promotion result.
--
box.ctl.promotion_reset()
```

## Background and motivation

The promote procedure strongly simplifies life of developers since they must not
do all of the promotion steps manually, that in a common case is not a trivial
task, as you can see in the algorithm description in the next section.

The common algorithm, disregarding failures and their processing consists of the
following steps: 
1. On an old master stop accepting DDL/DML - only DQL;
2. Wait until all master data is received by needed slave count, including the
new master candidate;
3. Make the old master be slave;
4. Make the slave be new master;
5. Notify all other slaves, that master is changed.

All of the steps are persisted in WAL, that guarantees, that even after a
promotion participant is restarted, after waking up it will not forgot about
promotion. Persistency eliminates any possibility of making the cluster have two
masters after the promotion.

## Detailed design

Each cluster member has a special system space to distribute promotion steps
over the cluster - `_promotion`:
```Lua
format = {}
-- UUID of the promotion round, generated on an initiator.
format[1] = {'round_uuid', 'string'}
-- UUID of the sender instance.
format[2] = {'source_uuid', 'string'}
-- Increasing step identifier. It grows from 1 to the last one
-- during promotion progress.
format[3] = {'step_number', 'unsigned'}
-- Timestamp, set by a sender using its own clock.
format[4] = {'ts', 'unsigned'}
--
-- Type is what the sender want to get or send. Value depends on
-- type.
--
format[5] = {'type', 'string'}
format[6] = {'value', 'any', is_nullable = true}
--
--            Here the type-value pairs are described.
--
-- 'begin'   - the message sent by a promotion initiator to start
--             a round. Value contains promotion metadata: round
--             UUID, initiator UUID, start timestamp etc.
--
-- 'status'  - the message sent by all promotion participants. The
--             single goal of this message is to cope with a case
--             when the cluster has no masters. The initiator via
--             statuses detects read-only cluster.
--
-- 'sync'    - the message sent by an old master to sync with the
--             slaves. Value is nil.
--
-- 'success' - the message sent by a slave on 'sync'. This message
--             is used by an old master to detect that the data
--             is synced.
--
-- 'error'   - an error, that can be sent by any cluster member.
--             For example, it can be failed sync, or an existing
--             promotion is found. Value is the error description.
--
s = box.schema.create_space('_promotion', {format = format})
```
To participate in a promotion a cluster member just writes into `_promotion`
space and waits until the record is replicated. This space is cleared by a
garbage collector from finished promotions - with error or success status. Only
latest promotion is not deleted to be able to restore role after recovery.

Below the protocol is described. On the image the state machine is showed:
![alt text](https://raw.githubusercontent.com/tarantool/tarantool/gerold103/gh-3055-box-ctl-promote-rfc/doc/rfc/3055-box_ctl_promote_img1.svg?sanitize=true)

In the simplest case the being promoted instance is master already - immediately
finish the promotion with the error and with no persisting that. Now assume
promote() is called on a slave. At first, the initiator broadcasts `begin`
request with the promotion status: `promote_uuid, step_number, start_ts,
timeout, round_uuid, ...`.

Each cluster member, received the `begin`, checks if it already knows about
another active promotions. If has, then responds `error` to the newer promotion
request. Else broadcasts `status` message.

If the cluster has no a master, the promotion initiator detects it by timeout of
waiting for `success` from a non-existing old master. Or by collecting `status`
messages from each replicaset member. In such a case it broadcasts `error` if
the promotion is not forced. Consider the case when a master exists.

An old master got `begin` request enters read-only mode and broadcasts `sync`
request. If the master recevies `sync` from another node, there are multiple
masters - the promotion is aborted via `error` broadcast and the master is back
in read-write mode.

A slave got `sync` finishes its participation in the round responding `success`.
The old master collects quorum `success`es including the promotion initiator's.
On timeout broadcast `error`. Once the old master has collected responses it
writes its own `success`. The initiator, got `success` from the master, enters
read-write mode and becomes a new master.

### Recovery

Recovery procedure consists of several independent cases, if a `_promotion`
space is not empty:
* Recovery of non-participant slave replica. Just do nothing.
* Recovery of non-participant master replica. Ignore 'master' role - another
master exists already.
* Recovery of the old master.

	Assume the found promotion state is `begin` - broadcast `error` and
	become a master.

	Assume the state is `error` - then the promotion is failed, and the
	current replica is still a master.

	Assume `success`es are found, but no one is from this master. So it has
	not finished the sync. Broadcast `error` and become a master.

	Assume the `success` sent from self is found. It means, that demotion is
	complete. Ignore master role and become a slave.

* Recovery of the promotion initiator.

	Assume the found promotion state is `begin` - broadcast an `error` and
	become a slave.

	Assume the state is `error` - then become a slave.

	Assume the status is `success` got from the old master - then become a
	master regardless of configuration.

	Assume the status is `success` but not from the old master - broadcast
	`error` and become a slave.
