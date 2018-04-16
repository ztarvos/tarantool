#!/usr/bin/env tarantool
test = require("sqltester")
test:plan(8)

test:execsql([[
	CREATE TABLE t1(a INT PRIMARY KEY, b INT UNIQUE, c INT, d STRING);
	INSERT INTO t1 VALUES (1, 1, 1, 'abcd'), (2, 2, 2, 'abcde');
	INSERT INTO t1 VALUES (3, 3, 3, 'abcdef'), (4, 4, 4, 'abcdefg');
	CREATE INDEX t1ix1 ON t1(a);
	CREATE INDEX t1ix2 ON t1(a, b);
	CREATE INDEX t1ix3 ON t1(a, b, c);
	CREATE INDEX t1ix4 ON t1(b);
	CREATE INDEX t1ix5 ON t1(b, c);
	CREATE INDEX t1ix6 ON t1(d);
]])

test:do_catchsql_test(
	"index-info-1.1",
	"PRAGMA index_xinfo (t1.t1ix1);",
	{
	1,"no such pragma: INDEX_XINFO"
	})

test:do_catchsql_test(
	"index-info-1.2",
	"PRAGMA index_info = t1.t1ix1;",
	{
	1,"near \".\": syntax error"
	})

test:do_execsql_test(
	"index-info-1.3",
	"PRAGMA index_info (t1.t1ix1);",
	{
	0, 0, 'A', 0, 'BINARY', 1, 'integer',
	})

test:do_execsql_test(
	"index-info-1.4",
	"PRAGMA index_info (t1.t1ix2);",
	{
	0, 0, 'A', 0, 'BINARY', 1, 'integer', 1, 1, 'B', 0, 'BINARY', 1, 'integer',
	})


test:do_execsql_test(
	"index-info-1.5",
	"PRAGMA index_info (t1.t1ix3);",
	{
	0, 0, 'A', 0, 'BINARY', 1, 'integer', 1, 1, 'B', 0, 'BINARY', 1, 'integer', 2,
	2, 'C', 0, 'BINARY', 1, 'integer',
	})

test:do_execsql_test(
	"index-info-1.6",
	"PRAGMA index_info (t1.t1ix4);",
	{
	0, 1, 'B', 0, 'BINARY', 1, 'integer',
	})

test:do_execsql_test(
	"index-info-1.7",
	"PRAGMA index_info (t1.t1ix5);",
	{
	0, 1, 'B', 0, 'BINARY', 1, 'integer', 1, 2, 'C', 0, 'BINARY', 1, 'integer',
	})

test:do_execsql_test(
	"index-info-1.8",
	"PRAGMA index_info (t1.t1ix6);",
	{
	0, 3, 'D', 0, 'BINARY', 1, 'scalar',
	})

test:finish_test()
