/** @module Box */

/**
 * Internal native constructor. Please use require.
 * @exports Box
 * @constructor
 * @example
 * var box = require('box') // return instanceof Box
 */
var Box = exports

/**
 * @exports Box/Space
 * @constructor
 */
var Space = Box.prototype.Space

/**
 * @exports Box/Index
 * @constructor
 */
var Index = Box.prototype.Index

/**
 * @exports Box/Iterator
 * @constructor
 * @param {Uint32} space_id - a space id
 * @param {Uint32} index_id - a index id
 * @param {Uint32} type - a iterator type
 * @param {Key} [key] - a key
 * @see Iterator
 * @returns Generator
 * @example
 * localhost> js it = new box.Iterator(0, 0, box.Iterator.GE, [1])
 * ---
 * {}
 * ...
 * localhost> js var tuple; res = []; while (tuple = it()) res.push(tuple); res.length
 * ---
 * 3
 */
var Iterator = Box.prototype.Iterator

var lua = require('lua')
/* Fallback to Lua for these bindings */
Box.prototype.cfg = lua.box.cfg
Box.prototype.info = lua.box.info
Box.prototype.slab = lua.box.slab
Box.prototype.stat = lua.box.stat

Object.defineProperty(Space.prototype, "length", {
     get: function() { return this.index[0].length },
     enumerable: true
});

/**
 * Search for a tuple or tuples in the given space.
 * @param {Uint32} index_id - a space id
 * @param {Key} key - a key
 * @param {Uint32} [offset=0] - an offset
 * @param {Uint32} [limit=4294967295] - a limit
 * @returns {Array.<Tuple>}
 */
Space.prototype.select = function(index, key, offset, limit) {
    return Box.select(this.id, index, key, offset, limit)
}

/**
 * Equivalent to store, but also checks that a tuple with the same key does not
 * exist.
 * @param {Tuple} tuple - a tuple
 * @param {boolean} [return_tuple=true] if set to false then returns a number of
 * affected tuples instead of tuples itself
 */
Space.prototype.insert = function(tuple, return_tuple) {
    return Box.insert(this.id, tuple, return_tuple)
}

/**
 * Equivalent to store, but also checks that a tuple with the same key exist.
 * @param {Tuple} tuple - a tuple
 * @param {boolean} [return_tuple=true] if set to false then returns a number of
 * affected tuples instead of tuples itself
 */
Space.prototype.replace = function(tuple, return_tuple) {
    return Box.replace(this.id, tuple, return_tuple)
}

/**
 * Store a tuple into a space.
 * @param {Tuple} tuple - a tuple
 * @param {boolean} [return_tuple=true] if set to false then returns a number of
 * affected tuples instead of tuples itself
 */
Space.prototype.store = function(tuple, return_tuple) {
    return Box.store(this.id, tuple, return_tuple)
}

/**
 * @param {Key} key - a key
 * @param {boolean} [return_tuple=true] if set to false then returns a number of
 * affected tuples instead of tuples itself
 */
Space.prototype.delete = function(key, return_tuple) {
    return Box.delete(this.id, key, return_tuple)
}

/**
 * Delete all tuples
 */
Space.prototype.truncate = function() {
    var pk = this.index[0]
    while (pk.length > 0) {
        var it = new Box.prototype.Iterator(this.id, pk.id, Box.Iterator.ALL);
        while (tuple = it()) {
            var key = [];
            for (var i = 0; i < pk.key_field.length; i++) {
                key.push(tuple[pk.key_field[i].field_id])
            }
            this.delete(key, false)
        }
    }
}

/**
 * Delete all tuples
 * @param {UInt32} space_id - a space id
 */
Box.prototype.truncate = function(space) {
    return Box.space[space].truncate()
}

/**
 * Creates an iterator.
 * @param {Uint32} index_id - a index id
 * @param {Uint32} type - a iterator type
 * @param {Key} [key] - a key
 * @see Iterator
 * @returns Generator
 */
Space.prototype.iterator = function(index, type, key) {
    return new Box.Iterator(this.id, index, type, key)
}

/**
 * Creates an iterator.
 * @param {Uint32} type - a iterator type
 * @param {Key} [key] - a key
 * @see Iterator
 * @returns Generator
 * @example
 * localhost> js it = box.space[0].index[0].iterator(box.Iterator.GE, [1])
 * ---
 * {}
 * ...
 * localhost> js var tuple; res = []; while (tuple = it()) res.push(tuple); res.length
 * ---
 * 3
 */
Index.prototype.iterator = function(type, key) {
    return new Box.prototype.Iterator(this.space_id, this.id, type, key)
}

/**
 * Creates an iterator.
 * @param {Uint32} space_id - a space id
 * @param {Uint32} index_id - a index id
 * @param {Uint32} type - a iterator type
 * @param {Key} [key] - a key
 * @see Iterator
 * @returns Generator
 */
Box.prototype.iterator = function(space, index, type, key) {
    return new Box.Iterator(space, index, type, key)
}

