const x11 = @cImport({
    @cInclude("X11/Xlib.h");
    @cInclude("X11/Xft/Xft.h");
});

pub const Display = x11.Display;
pub const Cursor = x11.Cursor;
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
    next: ?*Fnt,
};

pub const Col = enum(c_int) {
    fg,
    bg,
    border,
};
pub const Clr = x11.XftColor;

pub const Drw = extern struct {
    w: c_uint,
    h: c_uint,
    dpy: ?*Display,
    screen: c_int,
    root: Window,
    drawable: Drawable,
    gc: GC,
    scheme: ?*Clr,
    fonts: ?*Fnt,
};

const drw = struct {
    extern fn drw_create(dpy: ?*Display, screen: c_int, win: Window, w: c_uint, h: c_uint) ?*Drw;
    extern fn drw_resize(drw: ?*Drw, w: c_uint, h: c_uint) void;
    extern fn drw_free(drw: ?*Drw) void;
    extern fn drw_fontset_create(drw: ?*Drw, fonts: [*][*:0]const u8, fontcount: usize) ?*Fnt;
    extern fn drw_fontset_free(set: ?*Fnt) void;
    extern fn drw_fontset_getwidth(drw: ?*Drw, text: [*:0]const u8) c_uint;
    extern fn drw_fontset_getwidth_clamp(drw: ?*Drw, text: [*:0]const u8, n: c_uint) c_uint;
    extern fn drw_font_getexts(font: ?*Fnt, text: [*:0]const u8, len: c_uint, w: ?*c_uint, h: ?*c_uint) void;
    extern fn drw_clr_create(drw: ?*Drw, dest: ?*Clr, clrname: [*:0]const u8) void;
    extern fn drw_scm_create(drw: ?*Drw, clrnames: [*][*:0]const u8, clrcount: usize) ?*Clr;
    extern fn drw_cur_create(drw: *Drw, shape: c_int) ?*Cur;
    extern fn drw_cur_free(drw: ?*Drw, cursor: ?*Cur) void;
    extern fn drw_setfontset(drw: ?*Drw, set: ?*Fnt) void;
    extern fn drw_setscheme(drw: ?*Drw, scm: ?*Clr) void;
    extern fn drw_rect(drw: ?*Drw, x: c_int, y: c_int, w: c_uint, h: c_uint, filled: c_int, invert: c_int) void;
    extern fn drw_text(drw: ?*Drw, x: c_int, y: c_int, w: c_uint, h: c_uint, lpad: c_uint, text: [*:0]const u8, invert: c_int) c_int;
    extern fn drw_map(drw: ?*Drw, win: Window, x: c_int, y: c_int, w: c_uint, h: c_uint) void;
};

pub const create = drw.drw_create;
pub const resize = drw.drw_resize;
pub const free = drw.drw_free;
pub const fontset = struct {
    pub const create = drw.drw_fontset_create;
    pub const free = drw.drw_fontset_free;
    pub const getWidth = drw.drw_fontset_getwidth;
    pub const getWidthClamp = drw.drw_fontset_getwidth_clamp;
};
pub const fontGetExts = drw.drw_font_getexts;
pub const clrCreate = drw.drw_clr_create;
pub const scmCreate = drw.drw_scm_create;
pub const cur = struct {
    pub const create = drw.drw_cur_create;
    pub const free = drw.drw_cur_free;
};
pub const setFontSet = drw.drw_setfontset;
pub const setScheme = drw.drw_setscheme;
pub const rect = drw.drw_rect;
pub const text = drw.drw_text;
pub const map = drw.drw_map;
