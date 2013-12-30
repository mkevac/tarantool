/** @module Lua */

/**
 * @constructor
 * @example
localhost> js lua = require('lua')
---

...
localhost> js lua.box.space[0].index[0].type
---
"HASH"
...
localhost> js lua.box.select(0, 0, 1).toString()
---
"1: {2, 3}"
...
 */
var Lua