space_2 = box.schema.create_space('tweedledum2')
space_2:create_index('primary', 'hash', {parts = {0, 'str'}, unique = true })
space_2:create_index('minmax', 'tree', {parts = {1, 'str', 2, 'str'}, unique = true })

space_2:insert('brave', 'new', 'world')
space_2.index['minmax']:min()
space_2.index['minmax']:max()
space_2.index['minmax']:select('new', 'world')

-- A test case for Bug #904208
-- "assert failed, when key cardinality is greater than index cardinality"
--  https://bugs.launchpad.net/tarantool/+bug/904208

space_2.index['minmax']:select('new', 'world', 'order')
space_2:delete('brave')

-- A test case for Bug #902091
-- "Positioned iteration over a multipart index doesn't work"
-- https://bugs.launchpad.net/tarantool/+bug/902091

space_2:insert('item 1', 'alabama', 'song')
space_2.index['minmax']:select('alabama')
space_2:insert('item 2', 'california', 'dreaming ')
space_2:insert('item 3', 'california', 'uber alles')
space_2:insert('item 4', 'georgia', 'on my mind')
iter_2, tuple_2 = space_2.index['minmax']:next('california')
tuple_2
_, tuple_2 = space_2.index['minmax']:next(iter_1)
tuple_2
space_2:delete('item 1')
space_2:delete('item 2')
space_2:delete('item 3')
space_2:delete('item 4')
space_2:truncate()

--
-- Test that we print index number in error ER_INDEX_VIOLATION
--
space_2:insert('1', 'hello', 'world')
space_2:insert('2', 'hello', 'world')
space_2:drop()
