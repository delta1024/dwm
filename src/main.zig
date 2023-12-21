const std = @import("std");
const dwm = @import("dwm");

pub fn main() !void {
    var arena = std.heap.ArenaAllocator.init(std.heap.page_allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    const args_ = try std.process.argsAlloc(allocator);
    defer std.process.argsFree(allocator, args_);

    var args = try allocator.alloc([*:0]const u8, args_.len);
    defer allocator.free(args);
    for (args, args_) |*i, j| {
        i.* = @as([*:0]const u8, j.ptr);
    }
    defer args.deinit();

    dwm.checkUssage(@truncate(@as(isize, @bitCast(args.len))), args.ptr);
    var state = dwm.initState();
    dwm.checkOtherWm(state);
    dwm.setup(state);
    dwm.scan(state);
    dwm.run(state);
    dwm.cleanup(state);
    dwm.freeState(state);
    std.process.exit(0);
}
