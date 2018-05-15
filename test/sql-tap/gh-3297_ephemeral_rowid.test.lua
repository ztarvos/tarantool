#!/usr/bin/env tarantool
test = require("sqltester")
test:plan(1)

-- Check that OP_NextIdEphemeral generates unique ids.

test:execsql [[
    CREATE TABLE t1(a primary key);
    CREATE TABLE t2(a primary key, b);
    insert into t1 values(12);
    insert into t2 values(1, 5);
    insert into t2 values(2, 2);
    insert into t2 values(3, 2);
    insert into t2 values(4, 2);
]]

test:do_execsql_test(
    "gh-3297-1",
    [[
        select * from ( select a from t1 limit 1), (select b from t2 limit 10);
    ]],
    {
        12, 2,
        12, 2,
        12, 2,
        12, 5
    }
)

test:finish_test()
