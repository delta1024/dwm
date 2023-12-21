//! This module provides an idiomatic interface to the functions in dwm.c
const x11 = @cImport({
    @cInclude("X11/Xatom.h");
    @cInclude("X11/Xlib.h");
    @cInclude("X11/Xproto.h");
    @cInclude("X11/cursorfont.h");
    @cInclude("X11/keysym.h");
    @cInclude("X11/Xft/Xft.h");
});

pub const ProgramState = extern struct {
    dpy: ?*x11.Display,
};
pub const Arg = extern union {
    i: c_int,
    ui: c_uint,
    f: f32,
    v: ?*const anyopaque,
};
pub const Button = extern struct {
    click: c_uint,
    mask: c_uint,
    button: c_uint,
    func: *const fn (?*ProgramState, ?*const Arg) void,
    arg: Arg,
};
pub const Client = extern struct {
    name: [256]c_char,
    mina: f32,
    maxa: f32,
    x: c_int,
    y: c_int,
    w: c_int,
    h: c_int,
    oldx: c_int,
    oldy: c_int,
    oldw: c_int,
    oldh: c_int,
    basew: c_int,
    baseh: c_int,
    incw: c_int,
    inch: c_int,
    maxw: c_int,
    maxh: c_int,
    minw: c_int,
    minh: c_int,
    hintsvalid: c_int,
    bw: c_int,
    oldbw: c_int,
    tags: c_uint,
    isfixed: c_int,
    isfloating: c_int,
    isurgent: c_int,
    neverfocus: c_int,
    oldstate: c_int,
    isfullscreen: c_int,
    next: ?*Client,
    snext: ?*Client,
    mon: ?*x11.Monitor,
    win: x11.Window,
};
pub const Key = extern struct {
    mod: c_uint,
    keysym: x11.KeySym,
    func: *const fn (?*ProgramState, ?*const Arg) void,
    arg: Arg,
};
pub const Layout = extern struct {
    symbol: [*:0]const u8,
    arrange: *const fn (?*ProgramState, ?*Monitor) void,
};

pub const Monitor = extern struct {
    ltsymbol: [16]c_char,
    mfact: f32,
    nmaster: c_int,
    num: c_int,
    by: c_int,

    mx: c_int,
    my: c_int,
    mw: c_int,
    mh: c_int,
    wx: c_int,
    wy: c_int,
    ww: c_int,
    wh: c_int,

    seltags: c_uint,
    sellt: c_uint,
    tagset: [2]c_uint,
    showbar: c_int,
    top_bar: c_int,
    clients: ?*Client,
    sel: ?*Client,
    stack: ?*Client,
    next: ?*Monitor,
    barwin: x11.Window,
    lt: [2]?*const Layout,
};

pub const Rule = extern struct {
    class: [*:0]const u8,
    instance: [*:0]const u8,
    title: [*:0]const u8,
    tags: c_uint,
    isfloating: c_int,
    monitor: c_int,
};

const dwm = struct {
    extern fn init_state() ?*ProgramState;
    extern fn free_state(state: ?*ProgramState) void;
    extern fn check_ussage(argc: i32, argv: [*][*:0]const u8) void;
    extern fn checkotherwm(state: ?*ProgramState) void;
    extern fn setup(state: ?*ProgramState) void;
    extern fn scan(state: ?*ProgramState) void;
    extern fn run(state: ?*ProgramState) void;
    extern fn cleanup(state: ?*ProgramState) void;

    extern fn applyrules(state: ?*ProgramState, c: ?*Client) void;
    extern fn applysizehints(state: ?*ProgramState, c: ?*Client, x: ?*c_int, y: ?*c_int, w: ?*c_int, h: ?*c_int, interact: c_int) c_int;
    extern fn arrange(state: ?*ProgramState, m: ?*Monitor) void;
    extern fn arrangemon(state: ?*ProgramState, m: ?*Monitor) void;
    extern fn attach(c: ?*Client) void;
    extern fn attachstack(c: ?*Client) void;
    extern fn buttonpress(state: ?*ProgramState, e: ?*x11.XEvent) void;

    extern fn cleanupmon(state: ?*ProgramState, mon: ?*Monitor) void;
    extern fn clientmessage(state: ?*ProgramState, e: ?*x11.XEvent) void;
    extern fn configure(state: ?*ProgramState, c: ?*Client) void;

    extern fn configurenotify(state: ?*ProgramState, e: ?*x11.XEvent) void;
    extern fn configurerequest(state: ?*ProgramState, e: ?*x11.XEvent) void;
    // Monitor *createmon(void);
    // void destroynotify(program_state *state,XEvent *e);
    // void detach(Client *c);
    // void detachstack(Client *c);
    // Monitor *dirtomon(int dir);
    // void drawbar(Monitor *m);
    // void drawbars(void);
    // void enternotify(program_state *state,XEvent *e);
    // void expose(program_state *state,XEvent *e);
    // void focus(program_state *state,Client *c);
    // void focusin(program_state *state,XEvent *e);
    // void focusmon(program_state *state, const Arg *arg);
    // void focusstack(program_state *state,const Arg *arg);
    // Atom getatomprop(program_state *state,Client *c, Atom prop);
    // int getrootptr(program_state *state,int *x, int *y);
    // long getstate(program_state *state, Window w);
    // int gettextprop(program_state *state,Window w, Atom atom, char *text, unsigned int size);
    // void grabbuttons(program_state *state,Client *c, int focused);
    // void grabkeys(program_state *state);
    // void incnmaster(program_state *state,const Arg *arg);
    // void keypress(program_state *state,XEvent *e);
    // void killclient(program_state *state,const Arg *arg);
    // void manage(program_state *state,Window w, XWindowAttributes *wa);
    // void mappingnotify(program_state *state,XEvent *e);
    // void maprequest(program_state *state,XEvent *e);
    // void monocle(program_state *state,Monitor *m);
    // void motionnotify(program_state *state,XEvent *e);
    // void movemouse(program_state *state,const Arg *arg);
    // Client *nexttiled(Client *c);
    // void pop(program_state *state,Client *c);
    // void propertynotify(program_state *state,XEvent *e);
    // void quit(program_state *state,const Arg *arg);
    // Monitor *recttomon(int x, int y, int w, int h);
    // void resize(program_state *state,Client *c, int x, int y, int w, int h, int interact);
    // void resizeclient(program_state *state,Client *c, int x, int y, int w, int h);
    // void resizemouse(program_state *state,const Arg *arg);
    // void restack(program_state *state,Monitor *m);

    // int sendevent(program_state *state,Client *c, Atom proto);
    // void sendmon(program_state *state,Client *c, Monitor *m);
    // void setclientstate(program_state *state,Client *c, long i_state);
    // void setfocus(program_state *state,Client *c);
    // void setfullscreen(program_state *state,Client *c, int fullscreen);
    // void setlayout(program_state *state,const Arg *arg);
    // void setmfact(program_state *state,const Arg *arg);

    // void seturgent(program_state *state,Client *c, int urg);
    // void showhide(program_state *state,Client *c);
    // void spawn(program_state *state,const Arg *arg);
    // void tag   (program_state *state,const Arg *arg);
    // void tagmon(program_state *state,const Arg *arg);
    // void tile(program_state *state,Monitor *m);
    // void togglebar(program_state *state,const Arg *arg);
    // void togglefloating(program_state *state,const Arg *arg);
    // void toggletag (program_state *state,const Arg *arg);
    // void toggleview(program_state *state,const Arg *arg);
    // void unfocus(program_state *state,Client *c, int setfocus);
    // void unmanage(program_state *state,Client *c, int destroyed);
    // void unmapnotify(program_state *state,XEvent *e);
    // void updatebarpos(Monitor *m);
    // void updatebars(program_state *state);
    // void updateclientlist(program_state *state);
    // int updategeom(program_state *state);
    // void updatenumlockmask(program_state *state);
    // void updatesizehints(program_state *state,Client *c);
    // void updatestatus(program_state *state);
    // void updatetitle(program_state *state,Client *c);
    // void updatewindowtype(program_state *state,Client *c);
    // void updatewmhints(program_state *state,Client *c);
    // void view(program_state *state,const Arg *arg);
    // Client *wintoclient(Window w);
    // Monitor *wintomon(program_state *state,Window w);
    // int xerror(Display *dpy, XErrorEvent *ee);
    // int xerrordummy(Display *dpy, XErrorEvent *ee);
    // int xerrorstart(Display *dpy, XErrorEvent *ee);
    // void zoom(program_state *state,const Arg *arg);
};

pub const initState = dwm.init_state;
pub const freeState = dwm.free_state;
pub const checkUssage = dwm.check_ussage;
pub const checkOtherWm = dwm.checkotherwm;
pub const setup = dwm.setup;
pub const scan = dwm.scan;
pub const run = dwm.run;
pub const cleanup = dwm.cleanup;

pub const applyRules = dwm.applyrules;
pub const applySizeHints = dwm.applysizehints;
pub const arrange = dwm.arrange;
pub const arrangeMon = dwm.arrangemon;
pub const attach = dwm.attach;
pub const attachStack = dwm.attachstack;
pub const buttonPress = dwm.buttonpress;

pub const cleanupMon = dwm.cleanupmon;
pub const clientMessage = dwm.clientmessage;
pub const configure = dwm.configure;

pub const configureNotify = dwm.configurenotify;
pub const configureRequest = dwm.configurerequest;
