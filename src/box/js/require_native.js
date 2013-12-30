
/** @module Require */

/**
 * Tarantool exports only one global function to JavaScript - `require`.
 * All other modules can be loaded using `require`.
 * @constructor
 * @param {String} module - a module name
 * @returns {Object}
 * @example
 * JavaScript module example:
 * 
 * ~/tarantool_js/test/var $ cat test.js
 * exports.log = function(msg) {
 *     // some code
 * }
 * 
 * ~/tarantool_js/test/var $ tarantool
 * localhost> js test = require('test');
 * ---
 * [object Object]
 * ...
 * localhost> test.log('Hey You!')
 * ...
 * 
 * ~/tarantool_js/test/var $ tail -n 100 tarantool.log
 * 2013-02-03 21:03:22.754 [13130] 101/js-init-library I> Loading new JS module 'console' from './console.js'
 * 
 * localhost> box = require('box')
 */
var Require = function() {}