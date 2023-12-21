const std = @import("std");
const x11 = @cImport({
    @cInclude("X11/Xlib.h");
    @cInclude("X11/Xft/Xft.h");
});
const allocator = std.heap.page_allocator;

pub const Cursor = x11.Cursor;
pub const Display = x11.Display;
pub const XftFont = x11.XftFont;
pub const FcPattern = x11.FcPattern;
pub const Window = x11.Window;
pub const Drawable = x11.Drawable;
pub const GC = x11.GC;

pub const Cur = extern struct {
    cursor: Cursor,
};
pub const Fnt = extern struct {
    dpy: ?*Display,
    h: c_uint,
    xfont: ?*XftFont,
    pattern: ?*FcPattern,
    next: ?*Fnt = null,
    fn create(drw: ?*Drw, fontname: ?[*:0]const u8, font_pattern: ?*FcPattern) callconv(.C) ?*Fnt {
        const stderr_file = std.io.getStdErr();
        var bw = std.io.bufferedWriter(stderr_file.writer());
        const stderr = bw.writer();

        var xfont: ?*XftFont = null;
        var pattern: ?*FcPattern = null;
        if (fontname != null) {
            xfont = x11.XftFontOpenName(drw.?.*.dpy, drw.?.*.screen, fontname);
            if (xfont == null) {
                stderr.print("error, cannot load font from name: '{s}'\n", .{fontname.?}) catch {};
                bw.flush() catch {};
                return null;
            }
            pattern = x11.FcNameParse(@ptrCast(@constCast(fontname)));
            if (pattern == null) {
                stderr.print("error, cannot parse font name pattern: '{s}'\n", .{fontname.?}) catch {};
                bw.flush() catch {};
                x11.XftFontClose(drw.?.*.dpy, xfont);
                return null;
            }
        } else if (font_pattern) |pat| {
            xfont = x11.XftFontOpenPattern(drw.?.*.dpy, pat);
            if (xfont == null) {
                stderr.writeAll("error, cannot load font from pattern.\n") catch {};
                bw.flush() catch {};
                return null;
            }
        } else {
            @panic("no font specified");
        }

        var font = allocator.create(Fnt) catch @panic("alloc");
        font.* = .{
            .xfont = xfont,
            .pattern = pattern,
            .h = @bitCast(xfont.?.*.ascent + xfont.?.*.descent),
            .dpy = drw.?.*.dpy,
        };
        return font;
    }
    fn free(self: ?*Fnt) callconv(.C) void {
        if (self) |_| {
            if (self.?.*.pattern) |_| {
                x11.FcPatternDestroy(self.?.*.pattern);
            }
            x11.XftFontClose(self.?.*.dpy, self.?.*.xfont);
            allocator.destroy(self.?);
        }
    }
};
pub const ClrScmIdx = enum(c_int) {
    fg,
    bg,
    border,
};
pub const Clr = x11.XftColor;
extern fn drw_fontset_free(fonts: ?*Fnt) void;
pub const Drw = extern struct {
    w: c_uint,
    h: c_uint,
    dpy: ?*Display,
    screen: c_int,
    root: Window,
    drawable: Drawable,
    gc: GC,
    scheme: ?*Clr = undefined,
    fonts: ?*Fnt = undefined,
    pub fn create(dpy: ?*Display, screen: c_int, win: Window, w: c_uint, h: c_uint) callconv(.C) ?*Drw {
        const drw = allocator.create(Drw) catch @panic("drw_create");
        drw.* = .{
            .dpy = dpy,
            .screen = screen,
            .root = win,
            .w = w,
            .h = h,
            .drawable = x11.XCreatePixmap(dpy, win, @bitCast(w), @bitCast(h), @bitCast(x11.DefaultDepth(dpy, screen))),
            .gc = x11.XCreateGC(dpy, win, 0, null),
        };

        return drw;
    }
    pub fn resize(drw: ?*Drw, w: c_uint, h: c_uint) callconv(.C) void {
        if (drw) |win| {
            win.w = w;
            win.h = h;
            if (win.drawable != @intFromBool(false)) {
                _ = x11.XFreePixmap(win.dpy, win.drawable);
            }
            win.drawable = x11.XCreatePixmap(win.dpy, win.root, w, h, @bitCast(x11.DefaultDepth(win.dpy, win.screen)));
        } else {
            return;
        }
    }
    pub fn free(drw: ?*Drw) callconv(.C) void {
        _ = x11.XFreePixmap(drw.?.*.dpy, drw.?.*.drawable);
        _ = x11.XFreeGC(drw.?.*.dpy, drw.?.*.gc);
        drw_fontset_free(drw.?.*.fonts);
        allocator.destroy(drw.?);
    }
};
comptime {
    @export(Drw.create, .{ .name = "drw_create" });
    @export(Drw.free, .{ .name = "drw_free" });
    @export(Drw.resize, .{ .name = "drw_resize" });
    @export(Fnt.create, .{ .name = "xfont_create" });
    @export(Fnt.free, .{ .name = "xfont_free" });
}
