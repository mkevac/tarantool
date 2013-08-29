/* Alter 'exports' object here */

(function() {
    var lua = require('lua')
    /* Fallback to Lua for these bindings */
    exports.cfg = lua.box.cfg
    exports.info = lua.box.info
    exports.slab = lua.box.slab
    exports.stat = lua.box.stat
})();

Object.defineProperty(exports.Space.prototype, "length", {
    get: function() { return this.index[0].length },
    enumerable: true
});

exports.Space.prototype.select = function(index, key, offset, limit) {
    return exports.select(this.id, index, key, offset, limit)
}

exports.Space.prototype.insert = function(tuple, return_tuple) {
    return exports.insert(this.id, tuple, return_tuple)
}

exports.Space.prototype.replace = function(tuple, return_tuple) {
    return exports.replace(this.id, tuple, return_tuple)
}

exports.Space.prototype.store = function(tuple, return_tuple) {
    return exports.store(this.id, tuple, return_tuple)
}

exports.Space.prototype.delete = function(key, return_tuple) {
    return exports.delete(this.id, key, return_tuple)
}

exports.Space.prototype.truncate = function() {
    var pk = this.index[0]
    while (pk.length > 0) {
        var it = new exports.Iterator(this.id, pk.id, exports.Iterator.ALL);
        while (tuple = it()) {
            var key = [];
            for (var i = 0; i < pk.key_field.length; i++) {
                key.push(tuple[pk.key_field[i].field_id])
            }
            this.delete(key, false)
        }
    }
}

exports.truncate = function(space) {
    return exports.space[space].truncate()
}

exports.Space.prototype.iterator = function(index, type, key) {
    return new exports.Iterator(this.id, index, type, key)
}

exports.Index.prototype.iterator = function(type, key) {
    return new exports.Iterator(this.space_id, this.id, type, key)
}

exports.iterator = function(space, index, type, key) {
    return new exports.Iterator(space, index, type, key)
}
