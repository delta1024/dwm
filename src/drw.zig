const std = @import("std");
const x11 = @cImport({
    @cInclude("X11/Xlib.h");
    @cInclude("X11/Xft/Xft.h");
});
const allocator = std.heap.page_allocator;

pub const Cursor = x11.Cursor;
pub const Display = x11.Display;
pub const XftFont = x11.XftFont;
pub const XftColor = x11.XftColor;
pub const FcPattern = x11.FcPattern;
pub const Window = x11.Window;
pub const Drawable = x11.Drawable;
pub const GC = x11.GC;
const util = @cImport({
    @cInclude("util.h");
});

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
            util.die("no font specified.");
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
    pub fn getExts(self: ?*Fnt, text: ?[*:0]const u8, len: c_uint, w: ?*c_uint, h: ?*c_uint) callconv(.C) void {
        var ext: x11.XGlyphInfo = undefined;
        if (self == null or text == null) return;
        x11.XftTextExtentsUtf8(self.?.*.dpy, self.?.*.xfont, @as([*]const x11.XftChar8, @ptrCast(@alignCast(text))), @bitCast(len), &ext);
        if (w) |w_| {
            w_.* = @bitCast(@as(c_int, ext.xOff));
        }
        if (h) |h_| {
            h_.* = self.?.*.h;
        }
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
    scheme: ?[*]Clr = undefined,
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
    pub fn fontsetCreate(drw: ?*Drw, fonts: ?[*][*:0]const u8, font_count: usize) callconv(.C) ?*Fnt {
        if ((drw == null) or (fonts == null)) return null;
        var i: usize = 0;
        var ret: ?*Fnt = null;
        while (i < font_count) : (i += 1) {
            if (Fnt.create(drw, fonts.?[i], null)) |cur| {
                cur.next = ret;
                ret = cur;
            }
        }
        drw.?.*.fonts = ret;
        return ret;
    }
    pub fn fontsetFree(font: ?*Fnt) callconv(.C) void {
        if (font) |font_| {
            Drw.fontsetFree(font_.*.next);
            font_.free();
        }
    }
    pub fn curCreate(drw: ?*Drw, shape: c_int) callconv(.C) ?*Cur {
        if (drw == null) return null;
        var cur = allocator.create(Cur) catch return null;
        cur.*.cursor = x11.XCreateFontCursor(drw.?.*.dpy, @bitCast(shape));
        return cur;
    }
    pub fn curFree(drw: ?*Drw, cursor: ?*Cur) callconv(.C) void {
        if (cursor) |cur| {
            _ = x11.XFreeCursor(drw.?.*.dpy, cur.*.cursor);
            allocator.destroy(cur);
        }
    }
    pub fn clrCreate(drw: ?*Drw, dest: ?*Clr, clrname: ?[*:0]const u8) callconv(.C) void {
        if (drw == null or dest == null or clrname == null) return;

        if (x11.XftColorAllocName(
            drw.?.*.dpy,
            x11.DefaultVisual(
                drw.?.*.dpy,
                drw.?.*.screen,
            ),
            x11.DefaultColormap(
                drw.?.*.dpy,
                drw.?.*.screen,
            ),
            clrname,
            dest,
        ) == @intFromBool(false)) {
            util.die("error, cannot allocate color '%s'\n", clrname);
        }
    }
    pub fn scmCreate(_drw: ?*Drw, _clrnames: ?[*][*:0]const u8, clrcount: usize) callconv(.C) ?[*]Clr {
        var drw = _drw orelse return null;
        var clrnames = _clrnames orelse return null;

        if (clrcount < 2) return null;
        var ret: [*]Clr = @ptrCast(@alignCast((util.ecalloc(clrcount, @sizeOf(XftColor)) orelse return null)));
        for (clrnames[0..clrcount], ret) |c, *r| drw.clrCreate(r, c);
        return ret;
    }
    pub fn setFontSet(drw: ?*Drw, set: ?*Fnt) callconv(.C) void {
        if (drw) |d| d.fonts = set;
    }
    pub fn setScheme(drw: ?*Drw, scheme: ?[*]Clr) callconv(.C) void {
        if (drw) |d| d.scheme = scheme;
    }
    pub fn rect(
        drw: ?*Drw,
        x: c_int,
        y: c_int,
        w: c_uint,
        h: c_uint,
        filled: c_int,
        invert: c_int,
    ) callconv(.C) void {
        if (drw) |d| {
            if (d.scheme == null) return;
        } else return;
        _ = x11.XSetForeground(drw.?.*.dpy, drw.?.*.gc, if (invert != @intFromBool(false))
            drw.?.*.scheme.?[
                @intFromEnum(ClrScmIdx.bg)
            ].pixel
        else
            drw.?.*.scheme.?[
                @intFromEnum(ClrScmIdx.fg)
            ].pixel);
        _ = if (filled != @intFromBool(false))
            x11.XFillRectangle(drw.?.*.dpy, drw.?.*.drawable, drw.?.*.gc, x, y, w, h)
        else
            x11.XDrawRectangle(drw.?.*.dpy, drw.?.*.drawable, drw.?.*.gc, x, y, w - 1, h - 1);
    }

    pub fn map(_drw: ?*Drw, win: Window, x: c_int, y: c_int, w: c_uint, h: c_uint) callconv(.C) void {
        if (_drw == null) return;
        const drw = _drw.?;
        _ = x11.XCopyArea(drw.*.dpy, drw.*.drawable, win, drw.*.gc, x, y, w, h, x, y);
        _ = x11.XSync(drw.*.dpy, x11.False);
    }
    pub fn fontsetGetWidth(self: ?*Drw, text: ?[*:0]const u8) callconv(.C) c_uint {
        const drw = self orelse return 0;
        if (drw.*.fonts) |_| {} else return 0;
        const t = text orelse return 0;
        return @bitCast(drw_text(drw, 0, 0, 0, 0, 0, t, 0));
    }
    pub fn fontsetGetWidthClamp(self: ?*Drw, text: ?[*:0]const u8, n: c_uint) callconv(.C) c_uint {
        var tmp: c_uint = 0;
        if (self != null and self.?.*.fonts != null and n != @intFromBool(false)) {
            tmp = @bitCast(drw_text(self, 0, 0, 0, 0, 0, text, @bitCast(n)));
        }
        return @min(n, tmp);
    }
};
extern fn drw_text(drw: ?*Drw, x: c_int, y: c_int, w: c_uint, h: c_uint, lpad: c_uint, text: ?[*:0]const u8, invert: c_int) c_int;
comptime {
    @export(Drw.create, .{ .name = "drw_create" });
    @export(Drw.free, .{ .name = "drw_free" });
    @export(Drw.resize, .{ .name = "drw_resize" });
    @export(Fnt.create, .{ .name = "xfont_create" });
    @export(Fnt.free, .{ .name = "xfont_free" });
    @export(Drw.fontsetCreate, .{ .name = "drw_fontset_create" });
    @export(Drw.fontsetFree, .{ .name = "drw_fontset_free" });
    @export(Drw.curCreate, .{ .name = "drw_cur_create" });
    @export(Drw.curFree, .{ .name = "drw_cur_free" });
    @export(Drw.clrCreate, .{ .name = "drw_clr_create" });
    @export(Drw.scmCreate, .{ .name = "drw_scm_create" });
    @export(Drw.setScheme, .{ .name = "drw_setscheme" });
    @export(Drw.setFontSet, .{ .name = "drw_setfontset" });
    @export(Drw.rect, .{ .name = "drw_rect" });
    @export(Drw.map, .{ .name = "drw_map" });
    @export(Drw.fontsetGetWidth, .{ .name = "drw_fontset_getwidth" });
    @export(Drw.fontsetGetWidthClamp, .{ .name = "drw_fontset_getwidth_clamp" });
    @export(Fnt.getExts, .{ .name = "drw_font_getexts" });
}
