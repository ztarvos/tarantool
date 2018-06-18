env = require('test_run')
vclock_diff = require('fast_replica').vclock_diff
test_run = env.new()


SERVERS = { 'autobootstrap1', 'autobootstrap2', 'autobootstrap3' }

--
-- Start servers
--
test_run:create_cluster(SERVERS)

--
-- Wait for full mesh
--
test_run:wait_fullmesh(SERVERS)

--
-- Check vclock
--
vclock1 = test_run:get_vclock('autobootstrap1')
vclock_diff(vclock1, test_run:get_vclock('autobootstrap2'))
vclock_diff(vclock1, test_run:get_vclock('autobootstrap3'))

--
-- Switch off second replica
--
test_run:cmd("switch autobootstrap2")
repl = box.cfg.replication
box.cfg{replication = ""}

--
-- Insert rows
--
test_run:cmd("switch autobootstrap1")
s = box.space.test
for i = 1, 5 do s:insert{i} box.snapshot() end
s:select()
fio = require('fio')
path = fio.pathjoin(fio.abspath("."), 'autobootstrap1/*.xlog')
-- Depend on first master is a leader or not it should be 5 or 6.
#fio.glob(path) >= 5

--
-- Switch off third replica
--
test_run:cmd("switch autobootstrap3")
repl = box.cfg.replication
box.cfg{replication = ""}

--
-- Insert more rows
--
test_run:cmd("switch autobootstrap1")
for i = 6, 10 do s:insert{i} box.snapshot() end
s:select()
fio = require('fio')
path = fio.pathjoin(fio.abspath("."), 'autobootstrap1/*.xlog')
-- Depend on if the first master is a leader or not it should be 10 or 11.
#fio.glob(path) >= 10
errinj = box.error.injection
errinj.set("ERRINJ_NO_DISK_SPACE", true)
function insert(a) s:insert(a) end
_, err = pcall(insert, {11})
err:match("ailed to write")

--
-- Switch off third replica
--
test_run:cmd("switch autobootstrap3")
box.cfg{replication = repl}

--
-- Wait untill the third replica will catch up with the first one.
--
test_run:cmd("switch autobootstrap1")
fiber = require('fiber')
while #fio.glob(path) ~= 2 do fiber.sleep(0.01) end
#fio.glob(path)

--
-- Check data integrity on the third replica.
--
test_run:cmd("switch autobootstrap3")
box.space.test:select{}

--
-- Stop servers
--
test_run:cmd("switch default")
test_run:drop_cluster(SERVERS)
