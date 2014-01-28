-- schema.lua (internal file)
--
local ffi = require('ffi')
ffi.cdef[[
    struct space *space_by_id(uint32_t id);
    void space_run_triggers(struct space *space, bool yesno);
]]

box.schema.space = {}
box.schema.space.create = function(name, options)
    local _space = box.space[box.schema.SPACE_ID]
    if options == nil then
        options = {}
    end
    local if_not_exists = options.if_not_exists

    local temporary = options.temporary and "temporary" or ""

    if box.space[name] then
        if options.if_not_exists then
            return box.space[name], "not created"
        else
            box.raise(box.error.ER_SPACE_EXISTS,
                     "Space '"..name.."' already exists")
        end
    end
    local id
    if options.id then
        id = options.id
    else
        id = _space.index[0]:max()[0]
        if id < box.schema.SYSTEM_ID_MAX then
            id = box.schema.SYSTEM_ID_MAX + 1
        else
            id = id + 1
        end
    end
    if options.arity == nil then
        options.arity = 0
    end
    _space:insert{id, options.arity, name, temporary}
    return box.space[id], "created"
end
box.schema.create_space = box.schema.space.create
box.schema.space.drop = function(space_id)
    local _space = box.space[box.schema.SPACE_ID]
    local _index = box.space[box.schema.INDEX_ID]
    local keys = { _index:select{space_id} }
    for i = #keys, 1, -1 do
        local v = keys[i]
        _index:delete{v[0], v[1]}
    end
    if _space:delete{space_id} == nil then
        box.raise(box.error.ER_NO_SUCH_SPACE,
                  "Space "..space_id.." does not exist")
    end
end
box.schema.space.rename = function(space_id, space_name)
    local _space = box.space[box.schema.SPACE_ID]
    _space:update(space_id, {{"=", 2, space_name}})
end

box.schema.index = {}

box.schema.index.create = function(space_id, name, options)
    local _index = box.space[box.schema.INDEX_ID]
    if options == nil then
        options = {}
    end
    if options.type == nil then
        options.type = "tree"
    end
    if options.parts == nil then
        options.parts = { 0, "num" }
    end
    if options.unique == nil then
        options.unique = true
    end
    local index_type = options.type
    local unique = options.unique and 1 or 0
    local part_count = bit.rshift(#options.parts, 1)
    local parts = options.parts
    local iid = 0
    -- max
    local tuple = _index.index[0]:select_reverse_range(1, space_id)
    if tuple then
        local id = tuple[0]
        if id == space_id then
            iid = tuple[1] + 1
        end
    end
    if options.id then
        iid = options.id
    end
    _index:insert{space_id, iid, name, index_type, unique, part_count, unpack(parts)}
end
box.schema.index.drop = function(space_id, index_id)
    local _index = box.space[box.schema.INDEX_ID]
    _index:delete{space_id, index_id}
end
box.schema.index.rename = function(space_id, index_id, name)
    local _index = box.space[box.schema.INDEX_ID]
    _index:update({space_id, index_id}, {{"=", 2, name}})
end
box.schema.index.alter = function(space_id, index_id, options)
    if space_id == nil or index_id == nil then
        box.raise(box.error.ER_PROC_LUA, "Usage: index:alter{opts}")
    end
    if box.space[space_id] == nil then
        box.raise(box.error.ER_NO_SUCH_SPACE,
                  "Space "..space_id.." does not exist")
    end
    if box.space[space_id].index[index_id] == nil then
        box.raise(box.error.ER_NO_SUCH_INDEX,
                  "Index "..index_id.." not found in space"..space_id)
    end
    if options == nil then
        return
    end
    if type(space_id) == "string" then
        space_id = box.space[space_id].n
    end
    if type(index_id) == "string" then
        index_id = box.space[space_id].index[index_id].id
    end
    local _index = box.space[box.schema.INDEX_ID]
    if options.unique ~= nil then
        options.unique = options.unique and 1 or 0
    end
    if options.id ~= nil then
        if options.parts ~= nil then
            box.raise(box.error.ER_PROC_LUA,
                      "Don't know how to update both id and parts")
        end
        ops = {}
        local function add_op(value, field_no)
            if value then
                table.insert(ops, {'=', field_no, value})
            end
        end
        add_op(options.id, 1)
        add_op(options.name, 2)
        add_op(options.type, 3)
        add_op(options.unique, 4)
        _index:update({space_id, index_id}, ops)
        return
    end
    local tuple = _index:select{space_id, index_id}
    if options.name == nil then
        options.name = tuple[2]
    end
    if options.type == nil then
        options.type = tuple[3]
    end
    if options.unique == nil then
        options.unique = tuple[4]
    end
    if options.parts == nil then
        options.parts = {tuple:slice(6)} -- not part count
    end
    _index:replace{space_id, index_id, options.name, options.type,
                   options.unique, #options.parts/2, unpack(options.parts)}
end

local function keify(key)
    if type(key) == "table" then
        return key
    end
    return {key}
end

function box.schema.space.bless(space)
    local index_mt = {}
    -- __len and __index
    index_mt.len = function(index) return #index.idx end
    index_mt.__newindex = function(table, index)
        return error('Attempt to modify a read-only table') end
    index_mt.__index = index_mt
    -- min and max
    index_mt.min = function(index) return index.idx:min() end
    index_mt.max = function(index) return index.idx:max() end
    index_mt.random = function(index, rnd) return index.idx:random(rnd) end
    -- iteration
    index_mt.iterator = function(index, ...)
        return index.idx:iterator(...)
    end
    --
    -- pairs
    index_mt.pairs = function(index)
        return index.idx.next, index.idx, nil
    end
    -- index subtree size
    index_mt.count = function(index, ...)
        return index.idx:count(...)
    end

    --
    index_mt.eselect = function(index, key, opts)
        if opts == nil then
            opts = {}
        end

        local iterator = opts.iterator

        if type(key) ~= 'table' then
            key = { key }
        end

        if iterator == nil then
            iterator = box.index.EQ
        end
        if type(iterator) == 'string' then
            if box.index[ iterator ] == nil then
                error(string.format("Wrong iterator: %s", tostring(iterator)))
            end
            iterator = box.index[ iterator ]
        end

        local result = {}
        local offset = 0
        local skip = 0
        local count = 0
        if opts.offset ~= nil then
            offset = tonumber(opts.offset)
        end
        local limit = opts.limit
        local grep = opts.grep
        local map = opts.map

        if limit == 0 then
            return result
        end

        for tuple in index:iterator(iterator, unpack(key)) do
            if grep == nil or grep(tuple) then
                if skip < offset then
                    skip = skip + 1
                else
                    if map == nil then
                        table.insert(result, tuple)
                    else
                        table.insert(result, map(tuple))
                    end
                    count = count + 1

                    if limit == nil then
                        if count > 1 then
                            error("More than one tuple found without 'limit'")
                        end
                    elseif count >= limit then
                        break
                    end
                end
            end
        end
        if limit == nil then
            return unpack(result)
        end
        return result
    end

    --
    index_mt.select_range = function(index, limit, ...)
        local range = {}
        for v in index:iterator(box.index.GE, ...) do
            if #range >= limit then
                break
            end
            table.insert(range, v)
        end
        return unpack(range)
    end
    index_mt.select_reverse_range = function(index, limit, ...)
        local range = {}
        for v in index:iterator(box.index.LE, ...) do
            if #range >= limit then
                break
            end
            table.insert(range, v)
        end
        return unpack(range)
    end
    index_mt.select_limit = function(index, offset, limit, key)
        return box.process(box.net.box.SELECT,
                msgpack.encode({
                    [box.net.box.SPACE_ID] = index.n,
                    [box.net.box.INDEX_ID] = index.id,
                    [box.net.box.OFFSET] = offset,
                    [box.net.box.LIMIT] = limit,
                    [box.net.box.KEY] = keify(key)}))
    end
    index_mt.select = function(index, key)
        return index:select_limit(0, 4294967295, key)
    end
    index_mt.drop = function(index)
        return box.schema.index.drop(index.n, index.id)
    end
    index_mt.rename = function(index, name)
        return box.schema.index.rename(index.n, index.id, name)
    end
    index_mt.alter= function(index, options)
        return box.schema.index.alter(index.n, index.id, options)
    end
    --
    local space_mt = {}
    space_mt.len = function(space) return space.index[0]:len() end
    space_mt.__newindex = index_mt.__newindex

    space_mt.eselect = function(space, key, opts)
        return space.index[0]:eselect(key, opts)
    end

    space_mt.select = function(space, key)
        return box.process(box.net.box.SELECT,
                msgpack.encode({
                    [box.net.box.SPACE_ID] = space.n,
                    [box.net.box.INDEX_ID] = 0,
                    [box.net.box.OFFSET] = 0,
                    [box.net.box.LIMIT] = 4294967295,
                    [box.net.box.KEY] = keify(key)}))
     end
    space_mt.select_range = function(space, ino, limit, ...)
        return space.index[ino]:select_range(limit, ...)
    end
    space_mt.select_reverse_range = function(space, ino, limit, ...)
        return space.index[ino]:select_reverse_range(limit, ...)
    end
    space_mt.insert = function(space, tuple)
        return box.process(box.net.box.INSERT,
                msgpack.encode({
                    [box.net.box.SPACE_ID] = space.n,
                    [box.net.box.TUPLE] = tuple
                    }))
    end
    space_mt.replace = function(space, tuple)
        return box.process(box.net.box.REPLACE,
                msgpack.encode({
                    [box.net.box.SPACE_ID] = space.n,
                    [box.net.box.TUPLE] = tuple
                    }))
    end
    space_mt.update = function(space, key, ops)
        return box.process(box.net.box.UPDATE,
            msgpack.encode({
                [box.net.box.SPACE_ID] = space.n,
                [box.net.box.KEY] = keify(key),
                [box.net.box.TUPLE] = ops 
            }))
    end
--
-- delete can be done only by the primary key, whose
-- index is always 0. It doesn't accept compound keys
--
    space_mt.delete = function(space, key)
        return box.process(box.net.box.DELETE,
                msgpack.encode({
                    [box.net.box.SPACE_ID] = space.n,
                    [box.net.box.KEY] = keify(key)
                    }))
    end
-- Assumes that spaceno has a TREE (NUM) primary key
-- inserts a tuple after getting the next value of the
-- primary key and returns it back to the user
    space_mt.auto_increment = function(space, tuple)
        local max_tuple = space.index[0].idx:max()
        local max = 0
        if max_tuple ~= nil then
            max = max_tuple[0]
        end
        table.insert(tuple, 1, max + 1)
        return space:insert(tuple)
    end

    space_mt.truncate = function(space)
        local pk = space.index[0]
        if pk == nil then
            box.raise(box.error.ER_NO_SUCH_INDEX,
                      "No index #0 is defined in space "..space.n);
        end
        while #pk.idx > 0 do
            for t in pk:iterator() do
                local key = {}
                -- ipairs does not work because pk.key_field is zero-indexed
                for _k2, key_field in pairs(pk.key_field) do
                    table.insert(key, t[key_field.fieldno])
                end
                space:delete(key)
            end
        end
    end
    space_mt.pairs = function(space) return space.index[0]:pairs() end
    space_mt.drop = function(space)
        return box.schema.space.drop(space.n)
    end
    space_mt.rename = function(space, name)
        return box.schema.space.rename(space.n, name)
    end
    space_mt.create_index = function(space, name, options)
        return box.schema.index.create(space.n, name, options)
    end
    space_mt.run_triggers = function(space, yesno)
        local space = ffi.C.space_by_id(space.n)
        if space == nil then
            box.raise(box.error.ER_NO_SUCH_SPACE, "Space not found")
        end
        ffi.C.space_run_triggers(space, yesno)
    end
    space_mt.__index = space_mt

    setmetatable(space, space_mt)
    if type(space.index) == 'table' and space.enabled then
        for j, index in pairs(space.index) do
            if type(j) == 'number' then
                rawset(index, 'idx', box.index.bind(space.n, j))
                setmetatable(index, index_mt)
            end
        end
    end
end

