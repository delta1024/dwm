const util = struct {
    extern fn die(fmt: [*:0]const u8, ...) void;
    extern fn ecalloc(nmenb: usize, size: usize) ?*anyopaque;
};

pub const die = util.die;
pub const eCAlloc = util.ecalloc;
