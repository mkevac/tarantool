/** @module Box */

/**
 * @exports Box
 * @constructor
 */
var Box

/**
 * Search for a tuple or tuples in the given space.
 * @param {Uint32} space_id - a space id
 * @param {Uint32} index_id - a space id
 * @param {Key} key - a key
 * @param {Uint32} [offset=0] - an offset
 * @param {Uint32} [limit=4294967295] - a limit
 * @returns {Array.<Tuple>}
 */
Box.prototype.select = function() {};

/**
 * Store a tuple into a space.
 * @param {Uint32} space_id - a space_id
 * @param {Tuple} tuple - a tuple
 * @param {boolean} [return_tuple=true] if set to false then returns a number of
 * affected tuples instead of tuples itself
 */
Box.prototype.store = function() {};

/**
 * Equivalent to store, but also checks that a tuple with the same key does not
 * exist.
 * @param {Uint32} space_id - a space_id
 * @param {Tuple} tuple - a tuple
 * @param {boolean} [return_tuple=true] if set to false then returns a number of
 * affected tuples instead of tuples itself
 */
Box.prototype.insert = function() {};

/**
 * Equivalent to store, but also checks that a tuple with the same key exist.
 * @param {Uint32} space_id - a space_id
 * @param {Tuple} tuple - a tuple
 * @param {boolean} [return_tuple=true] if set to false then returns a number of
 * affected tuples instead of tuples itself
 */
Box.prototype.replace = function() {};

/**
 * @param {Uint32} space_id - a space_id
 * @param {Key} key - a key
 * @param {boolean} [return_tuple=true] if set to false then returns a number of
 * affected tuples instead of tuples itself
 */
Box.prototype.delete = function() {};

/**
 * An object with information about server variables
 * @example
 * localhost> box.info
 * {
 *         "version": "1.5.3-138-g682a52a",
 *         "build": {
 *                 "flags": " -fno-omit-frame-pointer -fno-stack-protector -fexceptions -funwind-tables -msse2 -std=c11 -Wall -Wextra -Wno-sign-compare -Wno-strict-aliasing -Werror",
 *                 "target": "Linux-x86_64-Debug",
 *                 "compiler": "/usr/bin/clang /usr/bin/clang++",
 *                 "options": "cmake . -DCMAKE_INSTALL_PREFIX=/usr/local -DENABLE_STATIC=OFF -DENABLE_TRACE=ON -DENABLE_BACKTRACE=OFF -DENABLE_CLIENT=OFF"
 *         },
 *         "pid": 19015,
 *         "logger_pid": 0,
 *         "config": "/data/work/tarantool/js/test/var/tarantool.cfg"
 * }
 *
 */
Box.prototype.info = {}

/**
 * An object with informaaton about server configuration parameters
 * @example
 * {
 *       "io_collect_interval": 0,
 *       "pid_file": "box.pid",
 *       "slab_alloc_minimal": 64,
 *       "slab_alloc_arena": 2,
 *       "log_level": 5,
 *       "logger_nonblock": true,
 *       "memcached_expire_per_loop": 1024,
 *       // cut
 *       "memcached_port": 0,
 *       "memcached_expire": false
 * }
 */
Box.prototype.cfg = {}

/**
 * This package provides access to slab allocator statistics.
 */
Box.prototype.slab = {}

/**
 * This package provides access to request statistics.
 */
Box.prototype.stat = {}

/**
 * Array of active spaces indexed by space_id.
 * @see Space
 * @example
 * localhost> box.space
 * [
 *         {
 *                 "id": 0,
 *               "arity": 0,
 *               "enabled": true,
 *               "index": [
 *                       {
 *                               "id": 0,
 *                               "unique": true,
 *                               "type": "HASH",
 *                               "key_field": [
 *                                       {
 *                                               "type": "NUM",
 *                                               "field_id": 0
 *                                       }
 *                               ]
 *                       }
 *               ]
 *       }
 * ]
 * ...
 * localhost> box.space[0].length
 * 1
 * ...
 */
Box.prototype.space = []

/**
 * @exports Box/Tuple
 * @constructor
 * @example
 *localhost> js new box.Tuple(1, 2, 'hello')
 *---
 *{
 *        "0": {
 *                "0": 1,
 *                "1": 0,
 *                "2": 0,
 *                "3": 0
 *        },
 *        "1": {
 *                "0": 2,
 *                "1": 0,
 *                "2": 0,
 *                "3": 0
 *        },
 *        "2": {
 *                "0": 104,
 *                "1": 101,
 *                "2": 108,
 *                "3": 108,
 *                "4": 111
 *        },
 *        "arity": 3
 * }
 * ...
 * localhost> js t = new box.Tuple([1, 2, 'hello'])
 * ---
 * {
 *        "0": {
 *                "0": 1,
 *                "1": 0,
 *                "2": 0,
 *                "3": 0
 *        },
 *        "1": {
 *                "0": 2,
 *                "1": 0,
 *                "2": 0,
 *                "3": 0
 *        },
 *        "2": {
 *                "0": 104,
 *                "1": 101,
 *                "2": 108,
 *                "3": 108,
 *                "4": 111
 *        },
 *        "arity": 3
 *}
 */
var Tuple = function() {}

/** Arity */
Tuple.prototype.arity = 0;

/**
 * @exports Box/Space
 * @constructor
 */
var Space

/**
 * Return a number of types in the primary index
 * @returns {Uint32}
 */
Space.prototype.length = 0

/**
 * @exports Box/Index
 * @constructor
 */
var Index

/**
 * Id
 * @type {Uint32}
 * @example
 * localhost> js box.space[0].index[0]
 * {
 *       "id": 0,
 *       "unique": true,
 *       "type": "HASH",
 *       "key_field": [
 *               {
 *                       "type": "NUM",
 *                       "field_id": 0
 *               }
 *       ]
 * }
 */
Index.prototype.id = 0;

/**
 * Return a number of types in the index
 * @returns {Uint32}
 */
Index.prototype.length = 0

/**
 * Id
 * @type {Boolean}
 */
Index.prototype.unique = 0;

/**
 * Type
 * @type {String}
 */
Index.prototype.type = 0;

/**
 * Key fields
 * @type {Array}
 */
Index.prototype.key_field = {};

/**
 * The smallest value in the index. Available only for indexes of type 'TREE'. 
 * @returns {Tuple}
 */
Index.prototype.min = function()

/**
 * The biggest value in the index. Available only for indexes of type 'TREE'. 
 * @returns {Tuple}
 */
Index.prototype.max = function()

/**
 * @param {Uint32} random
 * @returns {Tuple}
 */
Index.prototype.random = function() {}

/**
 * Iterate over an index, count the number of tuples which equal the provided
 * search criteria. The argument can either point to a tuple, a key, or one or
 * more key parts. Returns the number of matched tuples. 
 * @param {Key} key
 * @returns {Uint32}
 */
Index.prototype.count = function() {}

/**
 * @exports Box/Iterator
 * @constructor
 */
var Iterator

/** ALL iterator type */
Iterator.ALL = 0

/** EQ iterator type */
Iterator.EQ = 0

/** REQ iterator type */
Iterator.REQ = 0

/** LT iterator type */
Iterator.LT = 0

/** LE iterator type */
Iterator.LE = 0

/** GE iterator type */
Iterator.GE = 0

/** GE iterator type */
Iterator.GT = 0

/** BITS_ANY_SET iterator type */
Iterator.BITS_ANY_SET = 0

/** BITS_ANY_SET iterator type */
Iterator.BITS_ANY_SET = 0

/** BITS_ALL_NOT_SET iterator type */
Iterator.BITS_ALL_NOT_SET = 0

/** Iterator class */
Box.prototype.Iterator = Iterator
