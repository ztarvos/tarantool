-- All tables in SQL are now WITHOUT ROW ID, so if user
-- tries to create table without a primary key, an appropriate error message
-- should be raised. This tests checks it.

box.cfg{}

box.sql.execute("CREATE TABLE t1(a INT PRIMARY KEY, b int UNIQUE)")
box.sql.execute("CREATE TABLE t2(a int UNIQUE, b int)")

box.sql.execute("CREATE TABLE t3(a int)")
box.sql.execute("CREATE TABLE t4(a int, b int)")
box.sql.execute("CREATE TABLE t5(a int, b int UNIQUE)")

box.sql.execute("DROP TABLE t1")
