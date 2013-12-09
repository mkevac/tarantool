space_1 = box.schema.create_space('tweedledum1')
space_1:create_index('primary', 'hash', {parts = {0, 'str'}, unique = true })
space_1:create_index('minmax', 'tree', {parts = {1, 'str', 2, 'str'}, unique = true })

space_1:insert('brave', 'new', 'world')
space_1.index['minmax']:min()
space_1.index['minmax']:max()
space_1.index['minmax']:select('new', 'world')

-- A test case for Bug #904208
-- "assert failed, when key cardinality is greater than index cardinality"
--  https://bugs.launchpad.net/tarantool/+bug/904208

space_1.index['minmax']:select('new', 'world', 'order')
space_1:delete('brave')

-- A test case for Bug #902091
-- "Positioned iteration over a multipart index doesn't work"
-- https://bugs.launchpad.net/tarantool/+bug/902091

space_1:insert('item 1', 'alabama', 'song')
space_1.index['minmax']:select('alabama')
space_1:insert('item 2', 'california', 'dreaming ')
space_1:insert('item 3', 'california', 'uber alles')
space_1:insert('item 4', 'georgia', 'on my mind')
iter_1, tuple_1 = space_1.index['minmax']:next('california')
tuple_1
_, tuple_1 = space_1.index['minmax']:next(iter_1)
tuple_1
space_1:delete('item 1')
space_1:delete('item 2')
space_1:delete('item 3')
space_1:delete('item 4')
space_1:truncate()

--
-- Test that we print index number in error ER_INDEX_VIOLATION
--
space_1:insert('1', 'hello', 'world')
space_1:insert('2', 'hello', 'world')
space_1:drop()
