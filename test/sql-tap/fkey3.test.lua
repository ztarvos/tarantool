#!/usr/bin/env tarantool
test = require("sqltester")
test:plan(25)

-- This file implements regression tests for foreign keys.

test:do_execsql_test(
    "fkey3-1.1",
    [[
        CREATE TABLE t1(x INTEGER PRIMARY KEY);
        INSERT INTO t1 VALUES(100);
        INSERT INTO t1 VALUES(101);
        CREATE TABLE t2(y INTEGER PRIMARY KEY REFERENCES t1 (x));
        INSERT INTO t2 VALUES(100);
        INSERT INTO t2 VALUES(101);
        SELECT 1, x FROM t1;
    ]], {
        -- <fkey3-1.1>
        1, 100, 1, 101
        -- </fkey3-1.1>
    })

test:do_execsql_test(
    "fkey3-1.2",
    [[
        SELECT 2, y FROM t2;
    ]], {
        -- <fkey3-1.2>
        2, 100, 2, 101
        -- </fkey3-1.2>
    })

test:do_catchsql_test(
    "fkey3-1.3.1",
    [[
        DROP TABLE t1;
    ]], {
        -- <fkey3-1.3.1>
        1, "Can't drop space 'T1': other objects depend on it"
        -- </fkey3-1.3.1>
    })

test:do_catchsql_test(
    "fkey3-1.3.2",
    [[
        DROP TABLE t1;
    ]], {
        -- <fkey3-1.3.2>
        1, "Can't drop space 'T1': other objects depend on it"
        -- </fkey3-1.3.2>
    })

test:do_execsql_test(
    "fkey3-1.4",
    [[
        SELECT * FROM t1;
    ]], {
        -- <fkey3-1.4>
        100, 101
        -- </fkey3-1.4>
    })

test:do_execsql_test(
    "fkey3-1.5",
    [[
        DROP TABLE t2;
    ]], {
        -- <fkey3-1.5>
        -- </fkey3-1.5>
    })

test:do_execsql_test(
    "fkey3-1.6",
    [[
        DROP TABLE t1;
    ]], {
        -- <fkey3-1.6>
        -- </fkey3-1.6>
    })

test:do_execsql_test(
    "fkey3-2.1",
    [[
        CREATE TABLE t1(x INTEGER PRIMARY KEY);
        INSERT INTO t1 VALUES(100);
        INSERT INTO t1 VALUES(101);
        CREATE TABLE t2(y INTEGER PRIMARY KEY REFERENCES t1 (x) ON UPDATE SET NULL);
        INSERT INTO t2 VALUES(100);
        INSERT INTO t2 VALUES(101);
        SELECT 1, x FROM t1;
    ]], {
        -- <fkey3-2.1>
        1, 100, 1, 101
        -- </fkey3-2.1>
    })

test:do_execsql_test(
    "fkey3-2.2",
    [[
        SELECT 2, y FROM t2;
    ]], {
        -- <fkey3-2.2>
        2, 100, 2, 101
        -- </fkey3-2.2>
    })

test:do_execsql_test(
    "fkey3-3.1",
    [[
        CREATE TABLE t3(a PRIMARY KEY, b, c, d,
            UNIQUE(a, b),
            FOREIGN KEY(c, d) REFERENCES t3(a, b));
        INSERT INTO t3 VALUES(1, 2, 1, 2);
    ]], {
        -- <fkey3-3.1>
        -- </fkey3-3.1>
    })

test:do_catchsql_test(
    "fkey3-3.2",
    [[
        INSERT INTO t3 VALUES(2, 2, 5, 2);
    ]], {
        -- <fkey3-3.2>
        1, "FOREIGN KEY constraint failed"
        -- </fkey3-3.2>
    })

test:do_catchsql_test(
    "fkey3-3.3",
    [[
        INSERT INTO t3 VALUES(2, 3, 5, 2);
    ]], {
        -- <fkey3-3.3>
        1, "FOREIGN KEY constraint failed"
        -- </fkey3-3.3>
    })

test:do_execsql_test(
    "fkey3-3.4",
    [[
        CREATE TABLE t4(a PRIMARY KEY, b REFERENCES t4(a));
    ]], {
        -- <fkey3-3.4>
        -- </fkey3-3.4>
    })

test:do_catchsql_test(
    "fkey3-3.5",
    [[
        INSERT INTO t4 VALUES(2, 1);
    ]], {
        -- <fkey3-3.5>
        1, "FOREIGN KEY constraint failed"
        -- </fkey3-3.5>
    })

test:do_execsql_test(
    "fkey3-3.6",
    [[
        CREATE TABLE t6(a INTEGER PRIMARY KEY, b, c, d,
            FOREIGN KEY(c, d) REFERENCES t6(a, b));
        CREATE UNIQUE INDEX t6i ON t6(b, a);
        INSERT INTO t6 VALUES(1, 'a', 1, 'a');
        INSERT INTO t6 VALUES(2, 'a', 2, 'a');
        INSERT INTO t6 VALUES(3, 'a', 1, 'a');
        INSERT INTO t6 VALUES(5, 'a', 2, 'a');
    ]], {
        -- <fkey3-3.6>
        -- </fkey3-3.6>
    })

test:do_catchsql_test(
    "fkey3-3.7",
    [[
        INSERT INTO t6 VALUES(4, 'a', 65, 'a');
    ]], {
        -- <fkey3-3.7>
        1, "FOREIGN KEY constraint failed"
        -- </fkey3-3.7>
    })

test:do_execsql_test(
    "fkey3-3.8",
    [[
        INSERT INTO t6 VALUES(100, 'one', 100, 'one');
        DELETE FROM t6 WHERE a = 100;
        SELECT * FROM t6 WHERE a = 100;
    ]], {
        -- <fkey3-3.8>
        -- </fkey3-3.8>
    })

test:do_execsql_test(
    "fkey3-3.9",
    [[
        INSERT INTO t6 VALUES(100, 'one', 100, 'one');
        UPDATE t6 SET c = 1, d = 'a' WHERE a = 100;
        DELETE FROM t6 WHERE a = 100;
        SELECT * FROM t6 WHERE a = 100;
    ]], {
        -- <fkey3-3.9>
        -- </fkey3-3.9>
    })

test:do_execsql_test(
    "fkey3-3.10",
    [[
        CREATE TABLE t7(a, b, c, d INTEGER PRIMARY KEY,
            FOREIGN KEY(c, d) REFERENCES t7(a, b));
        CREATE UNIQUE INDEX t7i ON t7(a, b);
        INSERT INTO t7 VALUES('x', 1, 'x', 1);
        INSERT INTO t7 VALUES('x', 2, 'x', 2);
    ]], {
        -- <fkey3-3.10>
        -- </fkey3-3.10>
    })

test:do_catchsql_test(
    "fkey3-3.11",
    [[
        INSERT INTO t7 VALUES('x', 450, 'x', 4);
    ]], {
        -- <fkey3-3.11>
        1, "FOREIGN KEY constraint failed"
        -- </fkey3-3.11>
    })

test:do_catchsql_test(
    "fkey3-3.12",
    [[
        INSERT INTO t7 VALUES('x', 450, 'x', 451);
    ]], {
        -- <fkey3-3.12>
        1, "FOREIGN KEY constraint failed"
        -- </fkey3-3.12>
    })

test:do_execsql_test(
    "fkey3-6.1",
    [[
        CREATE TABLE t8(a PRIMARY KEY, b, c, d, e, FOREIGN KEY(c, d) REFERENCES t8(a, b));
        CREATE UNIQUE INDEX t8i1 ON t8(a, b);
        CREATE UNIQUE INDEX t8i2 ON t8(c);
        INSERT INTO t8 VALUES(1, 1, 1, 1, 1);
    ]], {
        -- <fkey3-6.1>
        -- </fkey3-6.1>
    })

test:do_catchsql_test(
    "fkey3-6.2",
    [[
        UPDATE t8 SET d = 2;
    ]], {
        -- <fkey3-6.2>
        1, "FOREIGN KEY constraint failed"
        -- </fkey3-6.2>
    })

test:do_execsql_test(
    "fkey3-6.3",
    [[
        UPDATE t8 SET d = 1;
        UPDATE t8 SET e = 2;
    ]], {
        -- <fkey3-6.3>
        -- </fkey3-6.3>
    })

test:do_catchsql_test(
    "fkey3-6.4",
    [[
        CREATE TABLE TestTable (
            id INTEGER PRIMARY KEY,
            name TEXT,
            source_id INTEGER NOT NULL,
            parent_id INTEGER,
            FOREIGN KEY(source_id, parent_id) REFERENCES TestTable(source_id, id));
        CREATE UNIQUE INDEX testindex on TestTable(source_id, id);
        INSERT INTO TestTable VALUES (1, 'parent', 1, null);
        INSERT INTO TestTable VALUES (2, 'child', 1, 1);
        UPDATE TestTable SET parent_id=1000 WHERE id=2;
    ]], {
        -- <fkey3-6.4>
        1, "FOREIGN KEY constraint failed"
        -- </fkey3-6.4>
    })

test:finish_test()
