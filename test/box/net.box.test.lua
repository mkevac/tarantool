space = box.schema.create_space('tweedledum')
space:create_index('primary', { type = 'hash'})
remote = box.net.box.new('localhost', box.cfg.primary_port, '0.5')
type(remote)
remote:ping()
remote:ping()
box.net.box.ping(remote)
space:insert{123, 'test1', 'test2'}
space:select{123}
tuple = remote:select(space.n, 123)

function test(...) return box.tuple.new({ 123, 456 }) end
f, a = box.call_loadproc('test')
type(f)
type(a)

remote:call('test')
function test(...) return box.tuple.new({ ... }) end
remote:call('test', 123, 345, { 678, 910 })
function test(...) return box.tuple.new({ ... }), box.tuple.new({ ... }) end
remote:call('test', 123, 345, { 678, 910 })
test = { a = 'a', b = function(self, ...) return box.tuple.new(123) end }
remote:call('test:b')
test.b = function(self, ...) return box.tuple.new({self.a, ...}) end
f, a = box.call_loadproc('test:b')
type(f)
type(a)
a.a
f, a = box.call_loadproc('test.b')
type(f)
type(a)

remote:call('test:b')
remote:call('test:b', 'b', 'c')
remote:call('test:b', 'b', 'c', 'd', 'e')
remote:call('test:b', 'b', { 'c', { d = 'e' } })


test = { a = { c = 1, b = function(self, ...) return { self.c, ... } end } }
f, a = box.call_loadproc('test.a:b')
type(f)
type(a)
a.c
f, a = box.call_loadproc('test.a.b')
type(f)
type(a)

remote:call('test.a:b', 123)



box.space.tweedledum:select(123)
box.space.tweedledum:select({123})
remote:call('box.space.tweedledum:select', 123)
remote:call('box.space.tweedledum:select', {123})

slf, foo = box.call_loadproc('box.net.self:select')
type(slf)
type(foo)

tuple
type(tuple)
#tuple

space:update(123, {{'=', 1, 'test1-updated'}})
remote:update(space.n, 123, {{'=', 2, 'test2-updated'}})

space:insert{123, 'test1', 'test2'}
remote:insert(space.n, {123, 'test1', 'test2'})

remote:insert(space.n, {345, 'test1', 'test2'})
remote:select(space.n, {345})
remote:call('box.space.tweedledum:select', 345)
space:select{345}

remote:replace(space.n, {345, 'test1-replaced', 'test2-replaced'})
space:select{345}
-- remote:select_limit(space.n, 0, 0, 1000, 345)

space:select_range(0, 1000)
remote:select_range(space.n, 0, 1000)
space:select{345}
remote:select(space.n, {345})
remote:timeout(0.5):select(space.n, {345})

remote:call('box.fiber.sleep', .01)
remote:timeout(0.01):call('box.fiber.sleep', 10)

--# setopt delimiter ';'
pstart = box.time();
parallel = {};
function parallel_foo(id)
    box.fiber.sleep(math.random() * .05)
    return id
end;
parallel_foo('abc');
for i = 1, 20 do
    box.fiber.resume(
        box.fiber.create(
            function()
                box.fiber.detach()
                local s = string.format('%07d', i)
                local so = remote:call('parallel_foo', s)
                table.insert(parallel, s == so[0])
            end
        )
    )
end;
for i = 1, 20 do
    if #parallel == 20 then
        break
    end
    box.fiber.sleep(0.1)
end;
--# setopt delimiter ''
unpack(parallel)
#parallel
box.time() - pstart < 0.5

remote:close()
remote:close()
remote:ping()

space:drop()
