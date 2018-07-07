#!/usr/bin/env tarantool

package.path = "lua/?.lua;"..package.path

local ffi = require('ffi')
local tap = require('tap')
local common = require('serializer_test')

local function is_map(s)
    return string.sub(s, 1, 1) == "{"
end

local function is_array(s)
    return string.sub(s, 1, 1) == "["
end

local function test_misc(test, s)
    test:plan(2)
    test:iscdata(s.NULL, 'void *', '.NULL is cdata')
    test:ok(s.NULL == nil, '.NULL == nil')
end

tap.test("json", function(test)
    local serializer = require('json')
    test:plan(21)

-- gh-2888: Check the possibility of using options in encode()/decode().

    local sub = {a = 1, { b = {c = 1, d = {e = 1}}}}
    serializer.cfg({encode_max_depth = 1})
    test:ok(serializer.encode(sub) == '{"1":null,"a":1}',
        'depth of encoding is 1 with .cfg')
    serializer.cfg({encode_max_depth = 2})
    test:ok(serializer.encode(sub) == '{"1":{"b":null},"a":1}',
        'depth of encoding is 2 with .cfg')
    serializer.cfg({encode_max_depth = 2})
    test:ok(serializer.encode(sub, {encode_max_depth = 1}) == '{"1":null,"a":1}',
        'depth of encoding is 1 with .encode')

    local nan = 1/0
    test:ok(serializer.encode({a = nan}) == '{"a":inf}',
        'default "encode_invalid_numbers"')
    serializer.cfg({encode_invalid_numbers = false})
    test:ok(pcall(serializer.encode, {a = nan}) == false,
        'expected error with NaN ecoding with .cfg')
    serializer.cfg({encode_invalid_numbers = true})
    test:ok(pcall(serializer.encode, {a = nan},
        {encode_invalid_numbers = false}) == false,
        'expected error with NaN ecoding with .encode')

    local number = 0.12345
    test:ok(serializer.encode({a = number}) == '{"a":0.12345}',
        'precision more than 5')
    serializer.cfg({encode_number_precision = 3})
    test:ok(serializer.encode({a = number}) == '{"a":0.123}',
        'precision is 3')
    serializer.cfg({encode_number_precision = 14})
    test:ok(serializer.encode({a = number},
        {encode_number_precision = 3}) == '{"a":0.123}', 'precision is 3')

    serializer.cfg({decode_invalid_numbers = false})
    test:ok(pcall(serializer.decode, '{"a":inf}') == false,
        'expected error with NaN decoding with .cfg')
    serializer.cfg({decode_invalid_numbers = true})
    test:ok(pcall(serializer.decode, '{"a":inf}',
        {decode_invalid_numbers = false}) == false,
        'expected error with NaN decoding with .decode')

    test:ok(pcall(serializer.decode, '{"1":{"b":{"c":1,"d":null}},"a":1}',
        {decode_max_depth = 2}) == false,
        'error: too many nested data structures')

--
    test:test("unsigned", common.test_unsigned, serializer)
    test:test("signed", common.test_signed, serializer)
    test:test("double", common.test_double, serializer)
    test:test("boolean", common.test_boolean, serializer)
    test:test("string", common.test_string, serializer)
    test:test("nil", common.test_nil, serializer)
    test:test("table", common.test_table, serializer, is_array, is_map)
    test:test("ucdata", common.test_ucdata, serializer)
    test:test("misc", test_misc, serializer)
end)
