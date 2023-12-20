const std = @import("std");
const d = @cImport({
    @cInclude("dwm.h");
});

pub fn main() !void {
    var arena = std.heap.ArenaAllocator.init(std.heap.page_allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    const args_ = try std.process.argsAlloc(allocator);
    defer std.process.argsFree(allocator, args_);

    var args = try allocator.alloc([*c]u8, args_.len);
    defer allocator.free(args);
    for (args, args_) |*i, j| {
        i.* = @as([*c]u8, j.ptr);
    }
    defer args.deinit();

    d.check_ussage(@truncate(@as(isize, @bitCast(args.len))), args.ptr);
    var state = d.init_state();
    _ = d.checkotherwm(state);
    d.setup(state);
    d.scan(state);
    d.run(state);
    d.cleanup(state);
    d.free_state(state);
    std.process.exit(0);
}
