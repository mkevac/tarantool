/** @module Fiber */

/**
 * @constructor
 * @param {Function} function - a function to executre
 * @param {...Object} args - arguments
 * @example
 * fiber = require('fiber')
 * 
 * function body1(msg, count) {
 *    console.log("In Fiber: " + this.id);
 * 
 *         var k = 0;
 *         for (var i = 0; i < count; i++) {
 *             console.log("Hello from fiber: " + msg);
 *             fiber.sleep(1.0)
 *         }
 * 
 *         return 48;
 *     }
 * 
 *     f1 = new fiber(body1, "Hello", 10)
 *     f2 = new fiber(body1, "Hey", 5)
 *     console.log("f1.id = " + f1.id)
 */
var Fiber = function() {}


/**
 * @param {Number} [timeout = TIMEOUT_INFINITY]
 */
Fiber.sleep = function() {}

/**
 * Infinity timeout
 */
Fiber.TIMEOUT_INFINITY = 0

/**
 * Id
 */
Fiber.prototype.id = 0

/**
 * State
 */
Fiber.prototype.state = 0

/**
 * Name
 */
Fiber.prototype.name = 0


