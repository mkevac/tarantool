/* Alter 'Box' object here */

Box = exports
Space = Box.prototype.Space
Index = Box.prototype.Index
Iterator = Box.prototype.Iterator

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

Space.prototype.select = function(index, key, offset, limit) {
    return Box.select(this.id, index, key, offset, limit)
}

Space.prototype.insert = function(tuple, return_tuple) {
    return Box.insert(this.id, tuple, return_tuple)
}

Space.prototype.replace = function(tuple, return_tuple) {
    return Box.replace(this.id, tuple, return_tuple)
}

Space.prototype.store = function(tuple, return_tuple) {
    return Box.store(this.id, tuple, return_tuple)
}

Space.prototype.delete = function(key, return_tuple) {
    return Box.delete(this.id, key, return_tuple)
}

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

Box.prototype.truncate = function(space) {
    return Box.space[space].truncate()
}

Space.prototype.iterator = function(index, type, key) {
    return new Box.Iterator(this.id, index, type, key)
}

exports.prototype.Index.prototype.iterator = function(type, key) {
    return new Box.Iterator(this.space_id, this.id, type, key)
}

exports.prototype.iterator = function(space, index, type, key) {
    return new Box.Iterator(space, index, type, key)
}
