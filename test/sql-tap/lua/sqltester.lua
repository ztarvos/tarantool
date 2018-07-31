local tap = require('tap')
local json = require('json')
local test = tap.test("errno")
local sql_tokenizer = require('sql_tokenizer')

-- pcall here, because we allow to run a test w/o test-run; use:
-- LUA_PATH='test/sql-tap/lua/?.lua;test/sql/lua/?.lua;;' \
--     ./test/sql-tap/xxx.test.lua
local ok, test_run = pcall(require, 'test_run')
test_run = ok and test_run.new() or nil

local function flatten(arr)
    local result = { }

    local function flatten(arr)
        for _, v in ipairs(arr) do
            if type(v) == "table" then
                flatten(v)
            else
                table.insert(result, v)
            end
        end
    end
    flatten(arr)
    return result
end

-- Goal of this routine is to update expected result
-- to be comparable with expected.
-- Right now it converts logical values to numbers.
-- Input must be a table.
local function fix_result(arr)
    if type(arr) ~= 'table' then return arr end
    for i, v in ipairs(arr) do
        if type(v) == 'table' then
            -- it is ok to pass array
            --fix_expect(v)
        else
            if type(v) == 'boolean' then
                if v then
                    arr[i] = 1
                else
                    arr[i] = 0
                end
            end
        end
    end
end

local function finish_test()
    test:check()
    os.exit()
end
test.finish_test = finish_test

-- Check if string is regex pattern.
-- Condition: /.../ or ~/.../
local function string_regex_p(str)
    if type(str) == 'string'
            and (string.sub(str, 1, 1) == '/'
            or string.sub(str, 1, 2) == '~/')
            and string.sub(str, -1) == '/' then
        return true;
    else
        return false;
    end
end

local function table_check_regex_p(t, regex)
    -- regex is definetely regex here, no additional checks
    local nmatch = string.sub(regex, 1, 1) == '~' and 1 or 0
    local regex_tr = string.sub(regex, 2 + nmatch, string.len(regex) - 1)
    for _, v in pairs(t) do
        if nmatch == 1 then
            if type(v) == 'table' and not table_check_regex_p(v, regex) then
                return 0
            end
            if type(v) == 'string' and string.find(v, regex_tr) then
                return 0
            end
        else
            if type(v) == 'table' and table_check_regex_p(v, regex) then
                return 1
            end
            if type(v) == 'string' and string.find(v, regex_tr) then
                return 1
            end
        end
    end

    return nmatch
end

local function is_deeply_regex(got, expected)
    if type(expected) == "number" or type(got) == "number" then
        if got ~= got and expected ~= expected then
            return true -- nan
        end
    end
    if type(expected) == "number" and type(got) == "number" then
        if got == expected then
            return true
        end
        local min_delta = 0.000000001 * math.abs(got)
        return (got - expected < min_delta) and (expected - got < min_delta)
    end

    if string_regex_p(expected) then
        return table_match_regex_p(got, expected)
    end

    if got == nil and expected == nil then return true end

    if type(got) ~= type(expected) then
        return false
    end

    if type(got) ~= 'table' then
        return got == expected
    end

    for i, v in pairs(expected) do
        if string_regex_p(v) then
            return table_check_regex_p(got, v) == 1
        else
            if not is_deeply_regex(got[i], v) then
                return false
            end
        end
    end

    if #got ~= #expected then
        return false
    end

    return true
end
test.is_deeply_regex = is_deeply_regex

local function do_test(self, label, func, expect)
    local ok, result = pcall(func)
    if ok then
        if result == nil then result = { } end
        -- Convert all trues and falses to 1s and 0s
        fix_result(result)

        -- If nothing is expected: just make sure there were no error.
        if expect == nil then
            if table.getn(result) ~= 0 and result[1] ~= 0 then
                test:fail(self, label)
            else
                test:ok(self, label)
            end
        else
            if is_deeply_regex(result, expect) then
                test:ok(self, label)
            else
                io.write(string.format('%s: Miscompare\n', label))
                io.write("Expected: ", json.encode(expect).."\n")
                io.write("Got     : ", json.encode(result).."\n")
                test:fail(label)
            end
        end
    else
        self:fail(string.format('%s: Execution failed: %s\n', label, result))
    end
end
test.do_test = do_test

local function execsql_one_by_one(sql)
    local queries = sql_tokenizer.split_sql(sql)
    local last_res = nil
    for _, query in pairs(queries) do
        last_res = box.sql.execute(query) or last_res
    end
    return last_res
end

local function execsql(self, sql)
    local result = execsql_one_by_one(sql)
    if type(result) ~= 'table' then return end

    result = flatten(result)
    for i, c in ipairs(result) do
        if c == nil then
            result[i] = ""
        end
    end
    return result
end
test.execsql = execsql

local function catchsql(self, sql, expect)
    r = {pcall(execsql, self, sql) }
    if r[1] == true then
        r[1] = 0
    else
        r[1] = 1
    end
    return r
end
test.catchsql = catchsql

local function do_catchsql_test(self, label, sql, expect)
    return do_test(self, label, function() return catchsql(self, sql) end, expect)
end
test.do_catchsql_test = do_catchsql_test

local function do_catchsql2_test(self, label, sql, expect)
    return do_test(self, label, function() return test.catchsql2(self, sql) end, expect)
end
test.do_catchsql2_test = do_catchsql2_test

local function do_execsql_test(self, label, sql, expect)
    return do_test(self, label, function() return test.execsql(self, sql) end, expect)
end
test.do_execsql_test = do_execsql_test

local function do_execsql2_test(self, label, sql, expect)
    return do_test(self, label, function() return test.execsql2(self, sql) end, expect)
end
test.do_execsql2_test = do_execsql2_test

local function flattern_with_column_names(result)
    local ret = {}
    local columns = result[0]
    for i = 1, #result, 1 do
        for j = 1, #columns, 1 do
            table.insert(ret, columns[j])
            table.insert(ret, result[i][j])
        end
    end
    return ret
end

function test.do_catchsql_set_test(self, testcases, prefix)
    -- testcases structure:
    -- {
    --      {
    --          TEST_CASE_NAME,
    --          SQL_STATEMENTS,
    --          RESULT (AS IN CATCHSQL TEST)
    --      }
    -- }
    if prefix == nil then prefix = "" end
    for _, testcase in ipairs(testcases) do
        test:do_catchsql_test(
                              prefix..testcase[1],
                              testcase[2],
                              testcase[3])
    end
end

local function execsql2(self, sql)
    local result = execsql_one_by_one(sql)
    if type(result) ~= 'table' then return end
    -- shift rows down, revealing column names
    result = flattern_with_column_names(result)
    return result
end
test.execsql2 = execsql2

local function sortsql(self, sql)
    local result = execsql(self, sql)
    table.sort(result, function(a,b) return a[2] < b[2] end)
    return result
end
test.sortsql = sortsql

local function catchsql2(self, sql)
    r = {pcall(execsql2, self, sql) }
    -- 0 means ok
    -- 1 means not ok
    r[1] = r[1] == true and 0 or 1
    return r
end
test.catchsql2 = catchsql2

-- Show the VDBE program for an SQL statement but omit the Trace
-- opcode at the beginning.  This procedure can be used to prove
-- that different SQL statements generate exactly the same VDBE code.
local function explain_no_trace(self, sql)
    tr = execsql(self, "EXPLAIN "..sql)
    for i=1,8 do
        table.remove(tr,1)
    end
    return tr
end
test.explain_no_trace = explain_no_trace
json = require("json")
function test.find_spaces_by_sql_statement(self, statement)
    local spaces = box.space._space:select{}
    local res = {}
    for _, space in ipairs(spaces) do
        local name_num = 3-- [3] is space name
        local options_num = 6-- [6] is space options
        if not space[name_num]:startswith("_") and space[options_num]["sql"] then
            if string.find(space[options_num]["sql"], statement) then
                table.insert(res, space[name_num])
            end
        end
    end
    return res
end

function test.drop_all_tables(self)
    local tables = test:find_spaces_by_sql_statement("CREATE TABLE")
    for _, table in ipairs(tables) do
        test:execsql(string.format([[DROP TABLE "%s";]], table))
    end
    return tables
end

function test.drop_all_views(self)
    local views = test:find_spaces_by_sql_statement("CREATE VIEW")
    for _, view in ipairs(views) do
        test:execsql(string.format([[DROP VIEW "%s";]], view))
    end
    return views
end

function test.do_select_tests(self, label, tests)
    for _, test_case in ipairs(tests) do
        local tn = test_case[1]
        local sql = test_case[2]
        local result = test_case[3]
        test:do_test(
            label..'.'..tn,
            function()
                return test:execsql(sql)
            end,
            result)
    end
end

function test.sf_execsql(self, sql)
    local old = box.sql.debug().sql_search_count
    local r = test:execsql(sql)
    local new = box.sql.debug().sql_search_count - old

    return {new, r}
end

function test.do_sf_execsql_test(self, label, sql, result)
    return test:do_test(label,
                        function()
                            return test:sf_execsql(sql)
                        end,
                        result)
end

local function db(self, cmd, ...)
    if cmd == 'eval' then
        return execsql(self, ...)
    end
end
test.db = db

-- returns first occurance of seed in input or -1
local function lsearch(self, input, seed)
    local index = 1
    local success = false
    local function search(arr)
        if type(arr) == 'table' then
            for _, v in ipairs(arr) do
                search(v)
                if success == true then
                    return
                end
            end
        else
            if type(arr) == 'string' and arr:find(seed) ~= nil then
                success = true
            else
                index = index + 1
            end
        end
    end
    search(input)
    return success == true and index or -1
end
test.lsearch = lsearch

function test.lindex(arr, pos)
    return arr[pos+1]
end

--function capable()
--    return true
--end

function test.randstr(Length)
    -- Length (number)
    local Chars = {}
    for Loop = 0, 255 do
        Chars[Loop+1] = string.char(Loop)
    end
    local String = table.concat(Chars)
    local Result = {}
    local Lookup = Chars
    local Range = #Lookup

    for Loop = 1,Length do
        Result[Loop] = Lookup[math.random(1, Range)]
    end

    return table.concat(Result)
end

test.do_eqp_test = function (self, label, sql, result)
    test:do_test(
        label,
        function()
            return execsql_one_by_one("EXPLAIN QUERY PLAN "..sql)
        end,
        result)
end

setmetatable(_G, nil)

-- perform clean up only under test-run
if test_run then
    os.execute("rm -rf $(ls -d */)")
    os.execute("rm -f *.snap *.xlog* *.vylog* *.run*")
end

-- start the database
box.cfg{
    memtx_max_tuple_size=4996109;
    vinyl_max_tuple_size=4996109;
    log="tarantool.log";
}

local engine = test_run and test_run:get_cfg('engine') or 'memtx'
box.sql.execute('pragma sql_default_engine=\''..engine..'\'')

function test.engine(self)
    return engine
end

return test
