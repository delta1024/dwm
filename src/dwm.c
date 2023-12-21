/* See LICENSE file for copyright and license details.
 *
 * dynamic window manager is designed like any other X client as well. It is
 * driven through handling X events. In contrast to other X clients, a window
 * manager selects for SubstructureRedirectMask on the root window, to receive
 * events about window (dis-)appearance. Only one X connection at a time is
 * allowed to select for this event mask.
 *
 * The event handlers of dwm are organized in an array which is accessed
 * whenever a new event has been fetched. This allows event dispatching
 * in O(1) time.
 *
 * Each child of the root window is called a client, except windows which have
 * set the override_redirect flag. Clients are organized in a linked client
 * list on each monitor, the focus history is remembered through a stack list
 * on each monitor. Each client contains a bit array to indicate the tags of a
 * client.
 *
 * Keys and tagging rules are organized as arrays and defined in config.h.
 *
 * To understand everything else, start reading main().
 */

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */
#include <X11/Xft/Xft.h>

#include "dwm.h"
#include "util.h"

/* macros */
#define BUTTONMASK (ButtonPressMask | ButtonReleaseMask)
#define CLEANMASK(mask)                                                        \
  (mask & ~(numlockmask | LockMask) &                                          \
   (ShiftMask | ControlMask | Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask |      \
    Mod5Mask))
#define INTERSECT(x, y, w, h, m)                                               \
  (MAX(0, MIN((x) + (w), (m)->wx + (m)->ww) - MAX((x), (m)->wx)) *             \
   MAX(0, MIN((y) + (h), (m)->wy + (m)->wh) - MAX((y), (m)->wy)))
#define ISVISIBLE(C) ((C->tags & C->mon->tagset[C->mon->seltags]))
#define LENGTH(X) (sizeof X / sizeof X[0])
#define MOUSEMASK (BUTTONMASK | PointerMotionMask)
#define WIDTH(X) ((X)->w + 2 * (X)->bw)
#define HEIGHT(X) ((X)->h + 2 * (X)->bw)
#define TAGMASK ((1 << LENGTH(tags)) - 1)
#define TEXTW(X) (drw_fontset_getwidth(drw, (X)) + lrpad)

/* enums */
enum { CurNormal, CurResize, CurMove, CurLast }; /* cursor */
enum { SchemeNorm, SchemeSel };                  /* color schemes */
enum {
  NetSupported,
  NetWMName,
  NetWMState,
  NetWMCheck,
  NetWMFullscreen,
  NetActiveWindow,
  NetWMWindowType,
  NetWMWindowTypeDialog,
  NetClientList,
  NetLast
}; /* EWMH atoms */
enum {
  WMProtocols,
  WMDelete,
  WMState,
  WMTakeFocus,
  WMLast

}; /* default atoms */
enum {
  ClkTagBar,
  ClkLtSymbol,
  ClkStatusText,
  ClkWinTitle,
  ClkClientWin,
  ClkRootWin,
  ClkLast
}; /* clicks */
static int screen;
struct internal_state {
  Display *dpy;
};

/* variables */
static const char broken[] = "broken";
static char stext[256];
// static int screen;
static int sw, sh; /* X display screen geometry width, height */

static int bh;    /* bar height */
static int lrpad; /* sum of left and right padding for text */
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int numlockmask = 0;
static void (*handler[LASTEvent])(program_state *, XEvent *) = {
    [ButtonPress] = buttonpress,
    [ClientMessage] = clientmessage,
    [ConfigureRequest] = configurerequest,
    [ConfigureNotify] = configurenotify,
    [DestroyNotify] = destroynotify,
    [EnterNotify] = enternotify,
    [Expose] = expose,
    [FocusIn] = focusin,
    [KeyPress] = keypress,
    [MappingNotify] = mappingnotify,
    [MapRequest] = maprequest,
    [MotionNotify] = motionnotify,
    [PropertyNotify] = propertynotify,
    [UnmapNotify] = unmapnotify};
static Atom wmatom[WMLast], netatom[NetLast];
static int running = 1;
static Cur *cursor[CurLast];
static Clr **scheme;

Drw *drw;
static Monitor *mons, *selmon;
static Window root, wmcheckwin;

/* configuration, allows nested code to access above variables */
#include "config.h"

/* compile-time check if all tags fit into an unsigned int bit array. */
struct NumTags {
  char limitexceeded[LENGTH(tags) > 31 ? -1 : 1];
};

/* function implementations */
void applyrules(program_state *state, Client *c) {

  const char *class, *instance;
  unsigned int i;
  const Rule *r;
  Monitor *m;
  XClassHint ch = {NULL, NULL};

  /* rule matching */
  c->isfloating = 0;
  c->tags = 0;
  XGetClassHint(state->dpy, c->win, &ch);
  class = ch.res_class ? ch.res_class : broken;
  instance = ch.res_name ? ch.res_name : broken;

  for (i = 0; i < LENGTH(rules); i++) {
    r = &rules[i];
    if ((!r->title || strstr(c->name, r->title)) &&
        (!r->class || strstr(class, r->class)) &&
        (!r->instance || strstr(instance, r->instance))) {
      c->isfloating = r->isfloating;
      c->tags |= r->tags;
      for (m = mons; m && m->num != r->monitor; m = m->next)
        ;
      if (m)
        c->mon = m;
    }
  }
  if (ch.res_class)
    XFree(ch.res_class);
  if (ch.res_name)
    XFree(ch.res_name);
  c->tags =
      c->tags & TAGMASK ? c->tags & TAGMASK : c->mon->tagset[c->mon->seltags];
}

int applysizehints(program_state *state, Client *c, int *x, int *y, int *w,
                   int *h, int interact) {
  int baseismin;
  Monitor *m = c->mon;

  /* set minimum possible */
  *w = MAX(1, *w);
  *h = MAX(1, *h);
  if (interact) {
    if (*x > sw)
      *x = sw - WIDTH(c);
    if (*y > sh)
      *y = sh - HEIGHT(c);
    if (*x + *w + 2 * c->bw < 0)
      *x = 0;
    if (*y + *h + 2 * c->bw < 0)
      *y = 0;
  } else {
    if (*x >= m->wx + m->ww)
      *x = m->wx + m->ww - WIDTH(c);
    if (*y >= m->wy + m->wh)
      *y = m->wy + m->wh - HEIGHT(c);
    if (*x + *w + 2 * c->bw <= m->wx)
      *x = m->wx;
    if (*y + *h + 2 * c->bw <= m->wy)
      *y = m->wy;
  }
  if (*h < bh)
    *h = bh;
  if (*w < bh)
    *w = bh;
  if (resizehints || c->isfloating || !c->mon->lt[c->mon->sellt]->arrange) {
    if (!c->hintsvalid)
      updatesizehints(state, c);
    /* see last two sentences in ICCCM 4.1.2.3 */
    baseismin = c->basew == c->minw && c->baseh == c->minh;
    if (!baseismin) { /* temporarily remove base dimensions */
      *w -= c->basew;
      *h -= c->baseh;
    }
    /* adjust for aspect limits */
    if (c->mina > 0 && c->maxa > 0) {
      if (c->maxa < (float)*w / *h)
        *w = *h * c->maxa + 0.5;
      else if (c->mina < (float)*h / *w)
        *h = *w * c->mina + 0.5;
    }
    if (baseismin) { /* increment calculation requires this */
      *w -= c->basew;
      *h -= c->baseh;
    }
    /* adjust for increment value */
    if (c->incw)
      *w -= *w % c->incw;
    if (c->inch)
      *h -= *h % c->inch;
    /* restore base dimensions */
    *w = MAX(*w + c->basew, c->minw);
    *h = MAX(*h + c->baseh, c->minh);
    if (c->maxw)
      *w = MIN(*w, c->maxw);
    if (c->maxh)
      *h = MIN(*h, c->maxh);
  }
  return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}

void arrange(program_state *state, Monitor *m) {
  if (m)
    showhide(state, m->stack);
  else
    for (m = mons; m; m = m->next)
      showhide(state, m->stack);
  if (m) {
    arrangemon(state, m);
    restack(state, m);
  } else
    for (m = mons; m; m = m->next)
      arrangemon(state, m);
}

void arrangemon(program_state *state, Monitor *m) {
  strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, sizeof m->ltsymbol);
  if (m->lt[m->sellt]->arrange)
    m->lt[m->sellt]->arrange(state, m);
}

void attach(Client *c) {
  c->next = c->mon->clients;
  c->mon->clients = c;
}

void attachstack(Client *c) {
  c->snext = c->mon->stack;
  c->mon->stack = c;
}

void buttonpress(program_state *state, XEvent *e) {

  unsigned int i, x, click;
  Arg arg = {0};
  Client *c;
  Monitor *m;
  XButtonPressedEvent *ev = &e->xbutton;

  click = ClkRootWin;
  /* focus monitor if necessary */
  if ((m = wintomon(state, ev->window)) && m != selmon) {
    unfocus(state, selmon->sel, 1);
    selmon = m;
    focus(state, NULL);
  }
  if (ev->window == selmon->barwin) {
    i = x = 0;
    do
      x += TEXTW(tags[i]);
    while (ev->x >= x && ++i < LENGTH(tags));
    if (i < LENGTH(tags)) {
      click = ClkTagBar;
      arg.ui = 1 << i;
    } else if (ev->x < x + TEXTW(selmon->ltsymbol))
      click = ClkLtSymbol;
    else if (ev->x > selmon->ww - (int)TEXTW(stext))
      click = ClkStatusText;
    else
      click = ClkWinTitle;
  } else if ((c = wintoclient(ev->window))) {
    focus(state, c);
    restack(state, selmon);
    XAllowEvents(state->dpy, ReplayPointer, CurrentTime);
    click = ClkClientWin;
  }
  for (i = 0; i < LENGTH(buttons); i++)
    if (click == buttons[i].click && buttons[i].func &&
        buttons[i].button == ev->button &&
        CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
      buttons[i].func(state, click == ClkTagBar && buttons[i].arg.i == 0
                                 ? &arg
                                 : &buttons[i].arg);
}

void checkotherwm(program_state *state) {

  xerrorxlib = XSetErrorHandler(xerrorstart);
  /* this causes an error if some other window manager is running */
  XSelectInput(state->dpy, DefaultRootWindow(state->dpy),
               SubstructureRedirectMask);
  XSync(state->dpy, False);
  XSetErrorHandler(xerror);
  XSync(state->dpy, False);
}

void cleanup(program_state *state) {

  Arg a = {.ui = ~0};
  Layout foo = {"", NULL};
  Monitor *m;
  size_t i;

  view(state, &a);
  selmon->lt[selmon->sellt] = &foo;
  for (m = mons; m; m = m->next)
    while (m->stack)
      unmanage(state, m->stack, 0);
  XUngrabKey(state->dpy, AnyKey, AnyModifier, root);
  while (mons)
    cleanupmon(state, mons);
  for (i = 0; i < CurLast; i++)
    drw_cur_free(drw, cursor[i]);
  for (i = 0; i < LENGTH(colors); i++)
    free(scheme[i]);
  free(scheme);
  XDestroyWindow(state->dpy, wmcheckwin);
  drw_free(drw);
  XSync(state->dpy, False);
  XSetInputFocus(state->dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
  XDeleteProperty(state->dpy, root, netatom[NetActiveWindow]);
}

void cleanupmon(program_state *state, Monitor *mon) {
  Monitor *m;

  if (mon == mons)
    mons = mons->next;
  else {
    for (m = mons; m && m->next != mon; m = m->next)
      ;
    m->next = mon->next;
  }
  XUnmapWindow(state->dpy, mon->barwin);
  XDestroyWindow(state->dpy, mon->barwin);
  free(mon);
}

void clientmessage(program_state *state, XEvent *e) {
  XClientMessageEvent *cme = &e->xclient;
  Client *c = wintoclient(cme->window);

  if (!c)
    return;
  if (cme->message_type == netatom[NetWMState]) {
    if (cme->data.l[1] == netatom[NetWMFullscreen] ||
        cme->data.l[2] == netatom[NetWMFullscreen])
      setfullscreen(state, c,
                    (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD    */
                     || (cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */ &&
                         !c->isfullscreen)));
  } else if (cme->message_type == netatom[NetActiveWindow]) {
    if (c != selmon->sel && !c->isurgent)
      seturgent(state, c, 1);
  }
}

void configure(program_state *state, Client *c) {
  XConfigureEvent ce;

  ce.type = ConfigureNotify;
  ce.display = state->dpy;
  ce.event = c->win;
  ce.window = c->win;
  ce.x = c->x;
  ce.y = c->y;
  ce.width = c->w;
  ce.height = c->h;
  ce.border_width = c->bw;
  ce.above = None;
  ce.override_redirect = False;
  XSendEvent(state->dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce);
}

void configurenotify(program_state *state, XEvent *e) {

  Monitor *m;
  Client *c;

  XConfigureEvent *ev = &e->xconfigure;
  int dirty;

  /* TODO: updategeom handling sucks, needs to be simplified */
  if (ev->window == root) {
    dirty = (sw != ev->width || sh != ev->height);
    sw = ev->width;
    sh = ev->height;
    if (updategeom(state) || dirty) {
      drw_resize(drw, sw, bh);
      updatebars(state);
      for (m = mons; m; m = m->next) {
        for (c = m->clients; c; c = c->next)
          if (c->isfullscreen)
            resizeclient(state, c, m->mx, m->my, m->mw, m->mh);
        XMoveResizeWindow(state->dpy, m->barwin, m->wx, m->by, m->ww, bh);
      }
      focus(state, NULL);
      arrange(state, NULL);
    }
  }
}

void configurerequest(program_state *state, XEvent *e) {

  Client *c;
  Monitor *m;
  XConfigureRequestEvent *ev = &e->xconfigurerequest;
  XWindowChanges wc;

  if ((c = wintoclient(ev->window))) {
    if (ev->value_mask & CWBorderWidth)
      c->bw = ev->border_width;
    else if (c->isfloating || !selmon->lt[selmon->sellt]->arrange) {
      m = c->mon;
      if (ev->value_mask & CWX) {
        c->oldx = c->x;
        c->x = m->mx + ev->x;
      }
      if (ev->value_mask & CWY) {
        c->oldy = c->y;
        c->y = m->my + ev->y;
      }
      if (ev->value_mask & CWWidth) {
        c->oldw = c->w;
        c->w = ev->width;
      }
      if (ev->value_mask & CWHeight) {
        c->oldh = c->h;
        c->h = ev->height;
      }
      if ((c->x + c->w) > m->mx + m->mw && c->isfloating)
        c->x = m->mx + (m->mw / 2 - WIDTH(c) / 2); /* center in x direction */
      if ((c->y + c->h) > m->my + m->mh && c->isfloating)
        c->y = m->my + (m->mh / 2 - HEIGHT(c) / 2); /* center in y direction */
      if ((ev->value_mask & (CWX | CWY)) &&
          !(ev->value_mask & (CWWidth | CWHeight)))
        configure(state, c);
      if (ISVISIBLE(c))
        XMoveResizeWindow(state->dpy, c->win, c->x, c->y, c->w, c->h);
    } else
      configure(state, c);
  } else {
    wc.x = ev->x;
    wc.y = ev->y;
    wc.width = ev->width;
    wc.height = ev->height;
    wc.border_width = ev->border_width;
    wc.sibling = ev->above;
    wc.stack_mode = ev->detail;
    XConfigureWindow(state->dpy, ev->window, ev->value_mask, &wc);
  }
  XSync(state->dpy, False);
}

Monitor *createmon(void) {
  Monitor *m;

  m = ecalloc(1, sizeof(Monitor));
  m->tagset[0] = m->tagset[1] = 1;
  m->mfact = mfact;
  m->nmaster = nmaster;
  m->showbar = showbar;
  m->topbar = topbar;
  m->lt[0] = &layouts[0];
  m->lt[1] = &layouts[1 % LENGTH(layouts)];
  strncpy(m->ltsymbol, layouts[0].symbol, sizeof m->ltsymbol);
  return m;
}

void destroynotify(program_state *state, XEvent *e) {
  Client *c;
  XDestroyWindowEvent *ev = &e->xdestroywindow;

  if ((c = wintoclient(ev->window)))
    unmanage(state, c, 1);
}

void detach(Client *c) {
  Client **tc;

  for (tc = &c->mon->clients; *tc && *tc != c; tc = &(*tc)->next)
    ;
  *tc = c->next;
}

void detachstack(Client *c) {
  Client **tc, *t;

  for (tc = &c->mon->stack; *tc && *tc != c; tc = &(*tc)->snext)
    ;
  *tc = c->snext;

  if (c == c->mon->sel) {
    for (t = c->mon->stack; t && !ISVISIBLE(t); t = t->snext)
      ;
    c->mon->sel = t;
  }
}

Monitor *dirtomon(int dir) {
  Monitor *m = NULL;

  if (dir > 0) {
    if (!(m = selmon->next))
      m = mons;
  } else if (selmon == mons)
    for (m = mons; m->next; m = m->next)
      ;
  else
    for (m = mons; m->next != selmon; m = m->next)
      ;
  return m;
}

void drawbar(Monitor *m) {
  int x, w, tw = 0;
  int boxs = drw->fonts->h / 9;
  int boxw = drw->fonts->h / 6 + 2;
  unsigned int i, occ = 0, urg = 0;
  Client *c;

  if (!m->showbar)
    return;

  /* draw status first so it can be overdrawn by tags later */
  if (m == selmon) { /* status is only drawn on selected monitor */
    drw_setscheme(drw, scheme[SchemeNorm]);
    tw = TEXTW(stext) - lrpad + 2; /* 2px right padding */
    drw_text(drw, m->ww - tw, 0, tw, bh, 0, stext, 0);
  }

  for (c = m->clients; c; c = c->next) {
    occ |= c->tags;
    if (c->isurgent)
      urg |= c->tags;
  }
  x = 0;
  for (i = 0; i < LENGTH(tags); i++) {
    w = TEXTW(tags[i]);
    drw_setscheme(
        drw, scheme[m->tagset[m->seltags] & 1 << i ? SchemeSel : SchemeNorm]);
    drw_text(drw, x, 0, w, bh, lrpad / 2, tags[i], urg & 1 << i);
    if (occ & 1 << i)
      drw_rect(drw, x + boxs, boxs, boxw, boxw,
               m == selmon && selmon->sel && selmon->sel->tags & 1 << i,
               urg & 1 << i);
    x += w;
  }
  w = TEXTW(m->ltsymbol);
  drw_setscheme(drw, scheme[SchemeNorm]);
  x = drw_text(drw, x, 0, w, bh, lrpad / 2, m->ltsymbol, 0);

  if ((w = m->ww - tw - x) > bh) {
    if (m->sel) {
      drw_setscheme(drw, scheme[m == selmon ? SchemeSel : SchemeNorm]);
      drw_text(drw, x, 0, w, bh, lrpad / 2, m->sel->name, 0);
      if (m->sel->isfloating)
        drw_rect(drw, x + boxs, boxs, boxw, boxw, m->sel->isfixed, 0);
    } else {
      drw_setscheme(drw, scheme[SchemeNorm]);
      drw_rect(drw, x, 0, w, bh, 1, 1);
    }
  }
  drw_map(drw, m->barwin, 0, 0, m->ww, bh);
}

void drawbars(void) {
  Monitor *m;

  for (m = mons; m; m = m->next)
    drawbar(m);
}

void enternotify(program_state *state, XEvent *e) {
  Client *c;
  Monitor *m;
  XCrossingEvent *ev = &e->xcrossing;

  if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) &&
      ev->window != root)
    return;
  c = wintoclient(ev->window);
  m = c ? c->mon : wintomon(state, ev->window);
  if (m != selmon) {
    unfocus(state, selmon->sel, 1);
    selmon = m;
  } else if (!c || c == selmon->sel)
    return;
  focus(state, c);
}

void expose(program_state *state, XEvent *e) {
  Monitor *m;
  XExposeEvent *ev = &e->xexpose;

  if (ev->count == 0 && (m = wintomon(state, ev->window)))
    drawbar(m);
}

void focus(program_state *state, Client *c) {

  if (!c || !ISVISIBLE(c))
    for (c = selmon->stack; c && !ISVISIBLE(c); c = c->snext)
      ;
  if (selmon->sel && selmon->sel != c)
    unfocus(state, selmon->sel, 0);
  if (c) {
    if (c->mon != selmon)
      selmon = c->mon;
    if (c->isurgent)
      seturgent(state, c, 0);
    detachstack(c);
    attachstack(c);
    grabbuttons(state, c, 1);
    XSetWindowBorder(state->dpy, c->win, scheme[SchemeSel][ColBorder].pixel);
    setfocus(state, c);
  } else {
    XSetInputFocus(state->dpy, root, RevertToPointerRoot, CurrentTime);
    XDeleteProperty(state->dpy, root, netatom[NetActiveWindow]);
  }
  selmon->sel = c;
  drawbars();
}

/* there are some broken focus acquiring clients needing extra handling */
void focusin(program_state *state, XEvent *e) {
  XFocusChangeEvent *ev = &e->xfocus;

  if (selmon->sel && ev->window != selmon->sel->win)
    setfocus(state, selmon->sel);
}

void focusmon(program_state *state, const Arg *arg) {

  Monitor *m;

  if (!mons->next)
    return;
  if ((m = dirtomon(arg->i)) == selmon)
    return;
  unfocus(state, selmon->sel, 0);
  selmon = m;
  focus(state, NULL);
}

void focusstack(program_state *state, const Arg *arg) {
  Client *c = NULL, *i;

  if (!selmon->sel || (selmon->sel->isfullscreen && lockfullscreen))
    return;
  if (arg->i > 0) {
    for (c = selmon->sel->next; c && !ISVISIBLE(c); c = c->next)
      ;
    if (!c)
      for (c = selmon->clients; c && !ISVISIBLE(c); c = c->next)
        ;
  } else {
    for (i = selmon->clients; i != selmon->sel; i = i->next)
      if (ISVISIBLE(i))
        c = i;
    if (!c)
      for (; i; i = i->next)
        if (ISVISIBLE(i))
          c = i;
  }
  if (c) {
    focus(state, c);
    restack(state, selmon);
  }
}

Atom getatomprop(program_state *state, Client *c, Atom prop) {

  int di;
  unsigned long dl;
  unsigned char *p = NULL;
  Atom da, atom = None;

  if (XGetWindowProperty(state->dpy, c->win, prop, 0L, sizeof atom, False,
                         XA_ATOM, &da, &di, &dl, &dl, &p) == Success &&
      p) {
    atom = *(Atom *)p;
    XFree(p);
  }
  return atom;
}

int getrootptr(program_state *state, int *x, int *y) {
  int di;
  unsigned int dui;
  Window dummy;

  return XQueryPointer(state->dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

long getstate(program_state *state, Window w) {
  int format;
  long result = -1;
  unsigned char *p = NULL;
  unsigned long n, extra;
  Atom real;

  if (XGetWindowProperty(state->dpy, w, wmatom[WMState], 0L, 2L, False,
                         wmatom[WMState], &real, &format, &n, &extra,
                         (unsigned char **)&p) != Success)
    return -1;
  if (n != 0)
    result = *p;
  XFree(p);
  return result;
}

int gettextprop(program_state *state, Window w, Atom atom, char *text,
                unsigned int size) {
  char **list = NULL;
  int n;
  XTextProperty name;

  if (!text || size == 0)
    return 0;
  text[0] = '\0';
  if (!XGetTextProperty(state->dpy, w, &name, atom) || !name.nitems)
    return 0;
  if (name.encoding == XA_STRING) {
    strncpy(text, (char *)name.value, size - 1);
  } else if (XmbTextPropertyToTextList(state->dpy, &name, &list, &n) >=
                 Success &&
             n > 0 && *list) {
    strncpy(text, *list, size - 1);
    XFreeStringList(list);
  }
  text[size - 1] = '\0';
  XFree(name.value);
  return 1;
}

void grabbuttons(program_state *state, Client *c, int focused) {
  updatenumlockmask(state);
  {
    unsigned int i, j;
    unsigned int modifiers[] = {0, LockMask, numlockmask,
                                numlockmask | LockMask};
    XUngrabButton(state->dpy, AnyButton, AnyModifier, c->win);
    if (!focused)
      XGrabButton(state->dpy, AnyButton, AnyModifier, c->win, False, BUTTONMASK,
                  GrabModeSync, GrabModeSync, None, None);
    for (i = 0; i < LENGTH(buttons); i++)
      if (buttons[i].click == ClkClientWin)
        for (j = 0; j < LENGTH(modifiers); j++)
          XGrabButton(state->dpy, buttons[i].button,
                      buttons[i].mask | modifiers[j], c->win, False, BUTTONMASK,
                      GrabModeAsync, GrabModeSync, None, None);
  }
}

void grabkeys(program_state *state) {
  updatenumlockmask(state);
  {
    unsigned int i, j, k;
    unsigned int modifiers[] = {0, LockMask, numlockmask,
                                numlockmask | LockMask};
    int start, end, skip;
    KeySym *syms;

    XUngrabKey(state->dpy, AnyKey, AnyModifier, root);
    XDisplayKeycodes(state->dpy, &start, &end);
    syms = XGetKeyboardMapping(state->dpy, start, end - start + 1, &skip);
    if (!syms)
      return;
    for (k = start; k <= end; k++)
      for (i = 0; i < LENGTH(keys); i++)
        /* skip modifier codes, we do that ourselves */
        if (keys[i].keysym == syms[(k - start) * skip])
          for (j = 0; j < LENGTH(modifiers); j++)
            XGrabKey(state->dpy, k, keys[i].mod | modifiers[j], root, True,
                     GrabModeAsync, GrabModeAsync);
    XFree(syms);
  }
}

void incnmaster(program_state *state, const Arg *arg) {
  selmon->nmaster = MAX(selmon->nmaster + arg->i, 0);
  arrange(state, selmon);
}

#ifdef XINERAMA
static int isuniquegeom(XineramaScreenInfo *unique, size_t n,
                        XineramaScreenInfo *info) {
  while (n--)
    if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org &&
        unique[n].width == info->width && unique[n].height == info->height)
      return 0;
  return 1;
}
#endif /* XINERAMA */

void keypress(program_state *state, XEvent *e) {
  unsigned int i;
  KeySym keysym;
  XKeyEvent *ev;

  ev = &e->xkey;
  keysym = XKeycodeToKeysym(state->dpy, (KeyCode)ev->keycode, 0);
  for (i = 0; i < LENGTH(keys); i++)
    if (keysym == keys[i].keysym &&
        CLEANMASK(keys[i].mod) == CLEANMASK(ev->state) && keys[i].func)
      keys[i].func(state, &(keys[i].arg));
}

void killclient(program_state *state, const Arg *arg) {
  if (!selmon->sel)
    return;
  if (!sendevent(state, selmon->sel, wmatom[WMDelete])) {
    XGrabServer(state->dpy);
    XSetErrorHandler(xerrordummy);
    XSetCloseDownMode(state->dpy, DestroyAll);
    XKillClient(state->dpy, selmon->sel->win);
    XSync(state->dpy, False);
    XSetErrorHandler(xerror);
    XUngrabServer(state->dpy);
  }
}

void manage(program_state *state, Window w, XWindowAttributes *wa) {
  Client *c, *t = NULL;
  Window trans = None;
  XWindowChanges wc;

  c = ecalloc(1, sizeof(Client));
  c->win = w;
  /* geometry */
  c->x = c->oldx = wa->x;
  c->y = c->oldy = wa->y;
  c->w = c->oldw = wa->width;
  c->h = c->oldh = wa->height;
  c->oldbw = wa->border_width;

  updatetitle(state, c);
  if (XGetTransientForHint(state->dpy, w, &trans) && (t = wintoclient(trans))) {
    c->mon = t->mon;
    c->tags = t->tags;
  } else {
    c->mon = selmon;
    applyrules(state, c);
  }

  if (c->x + WIDTH(c) > c->mon->wx + c->mon->ww)
    c->x = c->mon->wx + c->mon->ww - WIDTH(c);
  if (c->y + HEIGHT(c) > c->mon->wy + c->mon->wh)
    c->y = c->mon->wy + c->mon->wh - HEIGHT(c);
  c->x = MAX(c->x, c->mon->wx);
  c->y = MAX(c->y, c->mon->wy);
  c->bw = borderpx;

  wc.border_width = c->bw;
  XConfigureWindow(state->dpy, w, CWBorderWidth, &wc);
  XSetWindowBorder(state->dpy, w, scheme[SchemeNorm][ColBorder].pixel);
  configure(state, c); /* propagates border_width, if size doesn't change */
  updatewindowtype(state, c);
  updatesizehints(state, c);
  updatewmhints(state, c);
  XSelectInput(state->dpy, w,
               EnterWindowMask | FocusChangeMask | PropertyChangeMask |
                   StructureNotifyMask);
  grabbuttons(state, c, 0);
  if (!c->isfloating)
    c->isfloating = c->oldstate = trans != None || c->isfixed;
  if (c->isfloating)
    XRaiseWindow(state->dpy, c->win);
  attach(c);
  attachstack(c);
  XChangeProperty(state->dpy, root, netatom[NetClientList], XA_WINDOW, 32,
                  PropModeAppend, (unsigned char *)&(c->win), 1);
  XMoveResizeWindow(state->dpy, c->win, c->x + 2 * sw, c->y, c->w,
                    c->h); /* some windows require this */
  setclientstate(state, c, NormalState);
  if (c->mon == selmon)
    unfocus(state, selmon->sel, 0);
  c->mon->sel = c;
  arrange(state, c->mon);
  XMapWindow(state->dpy, c->win);
  focus(state, NULL);
}

void mappingnotify(program_state *state, XEvent *e) {
  XMappingEvent *ev = &e->xmapping;

  XRefreshKeyboardMapping(ev);
  if (ev->request == MappingKeyboard)
    grabkeys(state);
}

void maprequest(program_state *state, XEvent *e) {
  static XWindowAttributes wa;
  XMapRequestEvent *ev = &e->xmaprequest;

  if (!XGetWindowAttributes(state->dpy, ev->window, &wa) ||
      wa.override_redirect)
    return;
  if (!wintoclient(ev->window))
    manage(state, ev->window, &wa);
}

void monocle(program_state *state, Monitor *m) {
  unsigned int n = 0;
  Client *c;

  for (c = m->clients; c; c = c->next)
    if (ISVISIBLE(c))
      n++;
  if (n > 0) /* override layout symbol */
    snprintf(m->ltsymbol, sizeof m->ltsymbol, "[%d]", n);
  for (c = nexttiled(m->clients); c; c = nexttiled(c->next))
    resize(state, c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, 0);
}

void motionnotify(program_state *state, XEvent *e) {
  static Monitor *mon = NULL;
  Monitor *m;
  XMotionEvent *ev = &e->xmotion;

  if (ev->window != root)
    return;
  if ((m = recttomon(ev->x_root, ev->y_root, 1, 1)) != mon && mon) {
    unfocus(state, selmon->sel, 1);
    selmon = m;
    focus(state, NULL);
  }
  mon = m;
}

void movemouse(program_state *state, const Arg *arg) {
  int x, y, ocx, ocy, nx, ny;
  Client *c;
  Monitor *m;
  XEvent ev;
  Time lasttime = 0;

  if (!(c = selmon->sel))
    return;
  if (c->isfullscreen) /* no support moving fullscreen windows by mouse */
    return;
  restack(state, selmon);
  ocx = c->x;
  ocy = c->y;
  if (XGrabPointer(state->dpy, root, False, MOUSEMASK, GrabModeAsync,
                   GrabModeAsync, None, cursor[CurMove]->cursor,
                   CurrentTime) != GrabSuccess)
    return;
  if (!getrootptr(state, &x, &y))
    return;
  do {
    XMaskEvent(state->dpy, MOUSEMASK | ExposureMask | SubstructureRedirectMask,
               &ev);
    switch (ev.type) {
    case ConfigureRequest:
    case Expose:
    case MapRequest:
      handler[ev.type](state, &ev);
      break;
    case MotionNotify:
      if ((ev.xmotion.time - lasttime) <= (1000 / 60))
        continue;
      lasttime = ev.xmotion.time;

      nx = ocx + (ev.xmotion.x - x);
      ny = ocy + (ev.xmotion.y - y);
      if (abs(selmon->wx - nx) < snap)
        nx = selmon->wx;
      else if (abs((selmon->wx + selmon->ww) - (nx + WIDTH(c))) < snap)
        nx = selmon->wx + selmon->ww - WIDTH(c);
      if (abs(selmon->wy - ny) < snap)
        ny = selmon->wy;
      else if (abs((selmon->wy + selmon->wh) - (ny + HEIGHT(c))) < snap)
        ny = selmon->wy + selmon->wh - HEIGHT(c);
      if (!c->isfloating && selmon->lt[selmon->sellt]->arrange &&
          (abs(nx - c->x) > snap || abs(ny - c->y) > snap))
        togglefloating(state, NULL);
      if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
        resize(state, c, nx, ny, c->w, c->h, 1);
      break;
    }
  } while (ev.type != ButtonRelease);
  XUngrabPointer(state->dpy, CurrentTime);
  if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
    sendmon(state, c, m);
    selmon = m;
    focus(state, NULL);
  }
}

Client *nexttiled(Client *c) {
  for (; c && (c->isfloating || !ISVISIBLE(c)); c = c->next)
    ;
  return c;
}

void pop(program_state *state, Client *c) {
  detach(c);
  attach(c);
  focus(state, c);
  arrange(state, c->mon);
}

void propertynotify(program_state *state, XEvent *e) {
  Client *c;
  Window trans;
  XPropertyEvent *ev = &e->xproperty;

  if ((ev->window == root) && (ev->atom == XA_WM_NAME))
    updatestatus(state);
  else if (ev->state == PropertyDelete)
    return; /* ignore */
  else if ((c = wintoclient(ev->window))) {
    switch (ev->atom) {
    default:
      break;
    case XA_WM_TRANSIENT_FOR:
      if (!c->isfloating &&
          (XGetTransientForHint(state->dpy, c->win, &trans)) &&
          (c->isfloating = (wintoclient(trans)) != NULL))
        arrange(state, c->mon);
      break;
    case XA_WM_NORMAL_HINTS:
      c->hintsvalid = 0;
      break;
    case XA_WM_HINTS:
      updatewmhints(state, c);
      drawbars();
      break;
    }
    if (ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName]) {
      updatetitle(state, c);
      if (c == c->mon->sel)
        drawbar(c->mon);
    }
    if (ev->atom == netatom[NetWMWindowType])
      updatewindowtype(state, c);
  }
}

void quit(program_state *state, const Arg *arg) { running = 0; }

Monitor *recttomon(int x, int y, int w, int h) {
  Monitor *m, *r = selmon;
  int a, area = 0;

  for (m = mons; m; m = m->next)
    if ((a = INTERSECT(x, y, w, h, m)) > area) {
      area = a;
      r = m;
    }
  return r;
}

void resize(program_state *state, Client *c, int x, int y, int w, int h,
            int interact) {
  if (applysizehints(state, c, &x, &y, &w, &h, interact))
    resizeclient(state, c, x, y, w, h);
}

void resizeclient(program_state *state, Client *c, int x, int y, int w, int h) {
  XWindowChanges wc;

  c->oldx = c->x;
  c->x = wc.x = x;
  c->oldy = c->y;
  c->y = wc.y = y;
  c->oldw = c->w;
  c->w = wc.width = w;
  c->oldh = c->h;
  c->h = wc.height = h;
  wc.border_width = c->bw;
  XConfigureWindow(state->dpy, c->win,
                   CWX | CWY | CWWidth | CWHeight | CWBorderWidth, &wc);
  configure(state, c);
  XSync(state->dpy, False);
}

void resizemouse(program_state *state, const Arg *arg) {
  int ocx, ocy, nw, nh;
  Client *c;
  Monitor *m;
  XEvent ev;
  Time lasttime = 0;

  if (!(c = selmon->sel))
    return;
  if (c->isfullscreen) /* no support resizing fullscreen windows by mouse */
    return;
  restack(state, selmon);
  ocx = c->x;
  ocy = c->y;
  if (XGrabPointer(state->dpy, root, False, MOUSEMASK, GrabModeAsync,
                   GrabModeAsync, None, cursor[CurResize]->cursor,
                   CurrentTime) != GrabSuccess)
    return;
  XWarpPointer(state->dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1,
               c->h + c->bw - 1);
  do {
    XMaskEvent(state->dpy, MOUSEMASK | ExposureMask | SubstructureRedirectMask,
               &ev);
    switch (ev.type) {
    case ConfigureRequest:
    case Expose:
    case MapRequest:
      handler[ev.type](state, &ev);
      break;
    case MotionNotify:
      if ((ev.xmotion.time - lasttime) <= (1000 / 60))
        continue;
      lasttime = ev.xmotion.time;

      nw = MAX(ev.xmotion.x - ocx - 2 * c->bw + 1, 1);
      nh = MAX(ev.xmotion.y - ocy - 2 * c->bw + 1, 1);
      if (c->mon->wx + nw >= selmon->wx &&
          c->mon->wx + nw <= selmon->wx + selmon->ww &&
          c->mon->wy + nh >= selmon->wy &&
          c->mon->wy + nh <= selmon->wy + selmon->wh) {
        if (!c->isfloating && selmon->lt[selmon->sellt]->arrange &&
            (abs(nw - c->w) > snap || abs(nh - c->h) > snap))
          togglefloating(state, NULL);
      }
      if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
        resize(state, c, c->x, c->y, nw, nh, 1);
      break;
    }
  } while (ev.type != ButtonRelease);
  XWarpPointer(state->dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1,
               c->h + c->bw - 1);
  XUngrabPointer(state->dpy, CurrentTime);
  while (XCheckMaskEvent(state->dpy, EnterWindowMask, &ev))
    ;
  if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
    sendmon(state, c, m);
    selmon = m;
    focus(state, NULL);
  }
}

void restack(program_state *state, Monitor *m) {
  Client *c;
  XEvent ev;
  XWindowChanges wc;

  drawbar(m);
  if (!m->sel)
    return;
  if (m->sel->isfloating || !m->lt[m->sellt]->arrange)
    XRaiseWindow(state->dpy, m->sel->win);
  if (m->lt[m->sellt]->arrange) {
    wc.stack_mode = Below;
    wc.sibling = m->barwin;
    for (c = m->stack; c; c = c->snext)
      if (!c->isfloating && ISVISIBLE(c)) {
        XConfigureWindow(state->dpy, c->win, CWSibling | CWStackMode, &wc);
        wc.sibling = c->win;
      }
  }
  XSync(state->dpy, False);
  while (XCheckMaskEvent(state->dpy, EnterWindowMask, &ev))
    ;
}

void run(program_state *state) {
  XEvent ev;
  /* main event loop */
  XSync(state->dpy, False);
  while (running && !XNextEvent(state->dpy, &ev))
    if (handler[ev.type])
      handler[ev.type](state, &ev); /* call handler */
}

void scan(program_state *state) {
  unsigned int i, num;
  Window d1, d2, *wins = NULL;
  XWindowAttributes wa;

  if (XQueryTree(state->dpy, root, &d1, &d2, &wins, &num)) {
    for (i = 0; i < num; i++) {
      if (!XGetWindowAttributes(state->dpy, wins[i], &wa) ||
          wa.override_redirect ||
          XGetTransientForHint(state->dpy, wins[i], &d1))
        continue;
      if (wa.map_state == IsViewable || getstate(state, wins[i]) == IconicState)
        manage(state, wins[i], &wa);
    }
    for (i = 0; i < num; i++) { /* now the transients */
      if (!XGetWindowAttributes(state->dpy, wins[i], &wa))
        continue;
      if (XGetTransientForHint(state->dpy, wins[i], &d1) &&
          (wa.map_state == IsViewable ||
           getstate(state, wins[i]) == IconicState))
        manage(state, wins[i], &wa);
    }
    if (wins)
      XFree(wins);
  }
}

void sendmon(program_state *state, Client *c, Monitor *m) {
  if (c->mon == m)
    return;
  unfocus(state, c, 1);
  detach(c);
  detachstack(c);
  c->mon = m;
  c->tags = m->tagset[m->seltags]; /* assign tags of target monitor */
  attach(c);
  attachstack(c);
  focus(state, NULL);
  arrange(state, NULL);
}

void setclientstate(program_state *state, Client *c, long i_state) {
  long data[] = {i_state, None};

  XChangeProperty(state->dpy, c->win, wmatom[WMState], wmatom[WMState], 32,
                  PropModeReplace, (unsigned char *)data, 2);
}

int sendevent(program_state *state, Client *c, Atom proto) {
  int n;
  Atom *protocols;
  int exists = 0;
  XEvent ev;

  if (XGetWMProtocols(state->dpy, c->win, &protocols, &n)) {
    while (!exists && n--)
      exists = protocols[n] == proto;
    XFree(protocols);
  }
  if (exists) {
    ev.type = ClientMessage;
    ev.xclient.window = c->win;
    ev.xclient.message_type = wmatom[WMProtocols];
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = proto;
    ev.xclient.data.l[1] = CurrentTime;
    XSendEvent(state->dpy, c->win, False, NoEventMask, &ev);
  }
  return exists;
}

void setfocus(program_state *state, Client *c) {
  if (!c->neverfocus) {
    XSetInputFocus(state->dpy, c->win, RevertToPointerRoot, CurrentTime);
    XChangeProperty(state->dpy, root, netatom[NetActiveWindow], XA_WINDOW, 32,
                    PropModeReplace, (unsigned char *)&(c->win), 1);
  }
  sendevent(state, c, wmatom[WMTakeFocus]);
}

void setfullscreen(program_state *state, Client *c, int fullscreen) {
  if (fullscreen && !c->isfullscreen) {
    XChangeProperty(state->dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
                    PropModeReplace, (unsigned char *)&netatom[NetWMFullscreen],
                    1);
    c->isfullscreen = 1;
    c->oldstate = c->isfloating;
    c->oldbw = c->bw;
    c->bw = 0;
    c->isfloating = 1;
    resizeclient(state, c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
    XRaiseWindow(state->dpy, c->win);
  } else if (!fullscreen && c->isfullscreen) {
    XChangeProperty(state->dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
                    PropModeReplace, (unsigned char *)0, 0);
    c->isfullscreen = 0;
    c->isfloating = c->oldstate;
    c->bw = c->oldbw;
    c->x = c->oldx;
    c->y = c->oldy;
    c->w = c->oldw;
    c->h = c->oldh;
    resizeclient(state, c, c->x, c->y, c->w, c->h);
    arrange(state, c->mon);
  }
}

void setlayout(program_state *state, const Arg *arg) {
  if (!arg || !arg->v || arg->v != selmon->lt[selmon->sellt])
    selmon->sellt ^= 1;
  if (arg && arg->v)
    selmon->lt[selmon->sellt] = (Layout *)arg->v;
  strncpy(selmon->ltsymbol, selmon->lt[selmon->sellt]->symbol,
          sizeof selmon->ltsymbol);
  if (selmon->sel)
    arrange(state, selmon);
  else
    drawbar(selmon);
}

/* arg > 1.0 will set mfact absolutely */
void setmfact(program_state *state, const Arg *arg) {
  float f;

  if (!arg || !selmon->lt[selmon->sellt]->arrange)
    return;
  f = arg->f < 1.0 ? arg->f + selmon->mfact : arg->f - 1.0;
  if (f < 0.05 || f > 0.95)
    return;
  selmon->mfact = f;
  arrange(state, selmon);
}

void setup(program_state *state) {

  int i;
  XSetWindowAttributes wa;
  Atom utf8string;
  struct sigaction sa;

  /* do not transform children into zombies when they terminate */
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT | SA_RESTART;
  sa.sa_handler = SIG_IGN;
  sigaction(SIGCHLD, &sa, NULL);

  /* clean up any zombies (inherited from .xinitrc etc) immediately */
  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;

  /* init screen */
  screen = DefaultScreen(state->dpy);
  sw = DisplayWidth(state->dpy, screen);
  sh = DisplayHeight(state->dpy, screen);
  root = RootWindow(state->dpy, screen);
  drw = drw_create(state->dpy, screen, root, sw, sh);
  if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
    die("no fonts could be loaded.");
  lrpad = drw->fonts->h;
  bh = drw->fonts->h + 2;
  updategeom(state);
  /* init atoms */
  utf8string = XInternAtom(state->dpy, "UTF8_STRING", False);
  wmatom[WMProtocols] = XInternAtom(state->dpy, "WM_PROTOCOLS", False);
  wmatom[WMDelete] = XInternAtom(state->dpy, "WM_DELETE_WINDOW", False);
  wmatom[WMState] = XInternAtom(state->dpy, "WM_STATE", False);
  wmatom[WMTakeFocus] = XInternAtom(state->dpy, "WM_TAKE_FOCUS", False);
  netatom[NetActiveWindow] =
      XInternAtom(state->dpy, "_NET_ACTIVE_WINDOW", False);
  netatom[NetSupported] = XInternAtom(state->dpy, "_NET_SUPPORTED", False);
  netatom[NetWMName] = XInternAtom(state->dpy, "_NET_WM_NAME", False);
  netatom[NetWMState] = XInternAtom(state->dpy, "_NET_WM_STATE", False);
  netatom[NetWMCheck] =
      XInternAtom(state->dpy, "_NET_SUPPORTING_WM_CHECK", False);
  netatom[NetWMFullscreen] =
      XInternAtom(state->dpy, "_NET_WM_STATE_FULLSCREEN", False);
  netatom[NetWMWindowType] =
      XInternAtom(state->dpy, "_NET_WM_WINDOW_TYPE", False);
  netatom[NetWMWindowTypeDialog] =
      XInternAtom(state->dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
  netatom[NetClientList] = XInternAtom(state->dpy, "_NET_CLIENT_LIST", False);
  /* init cursors */
  cursor[CurNormal] = drw_cur_create(drw, XC_left_ptr);
  cursor[CurResize] = drw_cur_create(drw, XC_sizing);
  cursor[CurMove] = drw_cur_create(drw, XC_fleur);
  /* init appearance */
  scheme = ecalloc(LENGTH(colors), sizeof(Clr *));
  for (i = 0; i < LENGTH(colors); i++)
    scheme[i] = drw_scm_create(drw, colors[i], 3);
  /* init bars */
  updatebars(state);
  updatestatus(state);
  /* supporting window for NetWMCheck */
  wmcheckwin = XCreateSimpleWindow(state->dpy, root, 0, 0, 1, 1, 0, 0, 0);
  XChangeProperty(state->dpy, wmcheckwin, netatom[NetWMCheck], XA_WINDOW, 32,
                  PropModeReplace, (unsigned char *)&wmcheckwin, 1);
  XChangeProperty(state->dpy, wmcheckwin, netatom[NetWMName], utf8string, 8,
                  PropModeReplace, (unsigned char *)"dwm", 3);
  XChangeProperty(state->dpy, root, netatom[NetWMCheck], XA_WINDOW, 32,
                  PropModeReplace, (unsigned char *)&wmcheckwin, 1);
  /* EWMH support per view */
  XChangeProperty(state->dpy, root, netatom[NetSupported], XA_ATOM, 32,
                  PropModeReplace, (unsigned char *)netatom, NetLast);
  XDeleteProperty(state->dpy, root, netatom[NetClientList]);
  /* select events */
  wa.cursor = cursor[CurNormal]->cursor;
  wa.event_mask = SubstructureRedirectMask | SubstructureNotifyMask |
                  ButtonPressMask | PointerMotionMask | EnterWindowMask |
                  LeaveWindowMask | StructureNotifyMask | PropertyChangeMask;
  XChangeWindowAttributes(state->dpy, root, CWEventMask | CWCursor, &wa);
  XSelectInput(state->dpy, root, wa.event_mask);
  grabkeys(state);
  focus(state, NULL);
}

void seturgent(program_state *state, Client *c, int urg) {
  XWMHints *wmh;

  c->isurgent = urg;
  if (!(wmh = XGetWMHints(state->dpy, c->win)))
    return;
  wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
  XSetWMHints(state->dpy, c->win, wmh);
  XFree(wmh);
}

void showhide(program_state *state, Client *c) {
  if (!c)
    return;
  if (ISVISIBLE(c)) {
    /* show clients top down */
    XMoveWindow(state->dpy, c->win, c->x, c->y);
    if ((!c->mon->lt[c->mon->sellt]->arrange || c->isfloating) &&
        !c->isfullscreen)
      resize(state, c, c->x, c->y, c->w, c->h, 0);
    showhide(state, c->snext);
  } else {
    /* hide clients bottom up */
    showhide(state, c->snext);
    XMoveWindow(state->dpy, c->win, WIDTH(c) * -2, c->y);
  }
}

void spawn(program_state *state, const Arg *arg) {
  struct sigaction sa;

  if (arg->v == dmenucmd)
    dmenumon[0] = '0' + selmon->num;
  if (fork() == 0) {
    if (state->dpy)
      close(ConnectionNumber(state->dpy));
    setsid();

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = SIG_DFL;
    sigaction(SIGCHLD, &sa, NULL);

    execvp(((char **)arg->v)[0], (char **)arg->v);
    die("dwm: execvp '%s' failed:", ((char **)arg->v)[0]);
  }
}

void tag(program_state *state, const Arg *arg) {
  if (selmon->sel && arg->ui & TAGMASK) {
    selmon->sel->tags = arg->ui & TAGMASK;
    focus(state, NULL);
    arrange(state, selmon);
  }
}

void tagmon(program_state *state, const Arg *arg) {
  if (!selmon->sel || !mons->next)
    return;
  sendmon(state, selmon->sel, dirtomon(arg->i));
}

void tile(program_state *state, Monitor *m) {
  unsigned int i, n, h, mw, my, ty;
  Client *c;

  for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++)
    ;
  if (n == 0)
    return;

  if (n > m->nmaster)
    mw = m->nmaster ? m->ww * m->mfact : 0;
  else
    mw = m->ww;
  for (i = my = ty = 0, c = nexttiled(m->clients); c;
       c = nexttiled(c->next), i++)
    if (i < m->nmaster) {
      h = (m->wh - my) / (MIN(n, m->nmaster) - i);
      resize(state, c, m->wx, m->wy + my, mw - (2 * c->bw), h - (2 * c->bw), 0);
      if (my + HEIGHT(c) < m->wh)
        my += HEIGHT(c);
    } else {
      h = (m->wh - ty) / (n - i);
      resize(state, c, m->wx + mw, m->wy + ty, m->ww - mw - (2 * c->bw),
             h - (2 * c->bw), 0);
      if (ty + HEIGHT(c) < m->wh)
        ty += HEIGHT(c);
    }
}

void togglebar(program_state *state, const Arg *arg) {
  selmon->showbar = !selmon->showbar;
  updatebarpos(selmon);
  XMoveResizeWindow(state->dpy, selmon->barwin, selmon->wx, selmon->by,
                    selmon->ww, bh);
  arrange(state, selmon);
}

void togglefloating(program_state *state, const Arg *arg) {
  if (!selmon->sel)
    return;
  if (selmon->sel->isfullscreen) /* no support for fullscreen windows */
    return;
  selmon->sel->isfloating = !selmon->sel->isfloating || selmon->sel->isfixed;
  if (selmon->sel->isfloating)
    resize(state, selmon->sel, selmon->sel->x, selmon->sel->y, selmon->sel->w,
           selmon->sel->h, 0);
  arrange(state, selmon);
}

void toggletag(program_state *state, const Arg *arg) {
  unsigned int newtags;

  if (!selmon->sel)
    return;
  newtags = selmon->sel->tags ^ (arg->ui & TAGMASK);
  if (newtags) {
    selmon->sel->tags = newtags;
    focus(state, NULL);
    arrange(state, selmon);
  }
}

void toggleview(program_state *state, const Arg *arg) {
  unsigned int newtagset =
      selmon->tagset[selmon->seltags] ^ (arg->ui & TAGMASK);

  if (newtagset) {
    selmon->tagset[selmon->seltags] = newtagset;
    focus(state, NULL);
    arrange(state, selmon);
  }
}

void unfocus(program_state *state, Client *c, int setfocus) {
  if (!c)
    return;
  grabbuttons(state, c, 0);
  XSetWindowBorder(state->dpy, c->win, scheme[SchemeNorm][ColBorder].pixel);
  if (setfocus) {
    XSetInputFocus(state->dpy, root, RevertToPointerRoot, CurrentTime);
    XDeleteProperty(state->dpy, root, netatom[NetActiveWindow]);
  }
}

void unmanage(program_state *state, Client *c, int destroyed) {
  Monitor *m = c->mon;
  XWindowChanges wc;

  detach(c);
  detachstack(c);
  if (!destroyed) {
    wc.border_width = c->oldbw;
    XGrabServer(state->dpy); /* avoid race conditions */
    XSetErrorHandler(xerrordummy);
    XSelectInput(state->dpy, c->win, NoEventMask);
    XConfigureWindow(state->dpy, c->win, CWBorderWidth,
                     &wc); /* restore border */
    XUngrabButton(state->dpy, AnyButton, AnyModifier, c->win);
    setclientstate(state, c, WithdrawnState);
    XSync(state->dpy, False);
    XSetErrorHandler(xerror);
    XUngrabServer(state->dpy);
  }
  free(c);
  focus(state, NULL);
  updateclientlist(state);
  arrange(state, m);
}

void unmapnotify(program_state *state, XEvent *e) {
  Client *c;
  XUnmapEvent *ev = &e->xunmap;

  if ((c = wintoclient(ev->window))) {
    if (ev->send_event)
      setclientstate(state, c, WithdrawnState);
    else
      unmanage(state, c, 0);
  }
}

void updatebars(program_state *state) {

  Monitor *m;
  XSetWindowAttributes wa = {.override_redirect = True,
                             .background_pixmap = ParentRelative,
                             .event_mask = ButtonPressMask | ExposureMask};
  XClassHint ch = {"dwm", "dwm"};

  for (m = mons; m; m = m->next) {
    if (m->barwin)
      continue;
    m->barwin =
        XCreateWindow(state->dpy, root, m->wx, m->by, m->ww, bh, 0,
                      DefaultDepth(state->dpy, screen), CopyFromParent,
                      DefaultVisual(state->dpy, screen),
                      CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
    XDefineCursor(state->dpy, m->barwin, cursor[CurNormal]->cursor);
    XMapRaised(state->dpy, m->barwin);
    XSetClassHint(state->dpy, m->barwin, &ch);
  }
}

void updatebarpos(Monitor *m) {
  m->wy = m->my;
  m->wh = m->mh;
  if (m->showbar) {
    m->wh -= bh;
    m->by = m->topbar ? m->wy : m->wy + m->wh;
    m->wy = m->topbar ? m->wy + bh : m->wy;
  } else
    m->by = -bh;
}

void updateclientlist(program_state *state) {
  Client *c;
  Monitor *m;

  XDeleteProperty(state->dpy, root, netatom[NetClientList]);
  for (m = mons; m; m = m->next)
    for (c = m->clients; c; c = c->next)
      XChangeProperty(state->dpy, root, netatom[NetClientList], XA_WINDOW, 32,
                      PropModeAppend, (unsigned char *)&(c->win), 1);
}

int updategeom(program_state *state) {
  int dirty = 0;

#ifdef XINERAMA
  if (XineramaIsActive(dpy)) {
    int i, j, n, nn;
    Client *c;
    Monitor *m;
    XineramaScreenInfo *info = XineramaQueryScreens(dpy, &nn);
    XineramaScreenInfo *unique = NULL;

    for (n = 0, m = mons; m; m = m->next, n++)
      ;
    /* only consider unique geometries as separate screens */
    unique = ecalloc(nn, sizeof(XineramaScreenInfo));
    for (i = 0, j = 0; i < nn; i++)
      if (isuniquegeom(unique, j, &info[i]))
        memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
    XFree(info);
    nn = j;

    /* new monitors if nn > n */
    for (i = n; i < nn; i++) {
      for (m = mons; m && m->next; m = m->next)
        ;
      if (m)
        m->next = createmon();
      else
        mons = createmon();
    }
    for (i = 0, m = mons; i < nn && m; m = m->next, i++)
      if (i >= n || unique[i].x_org != m->mx || unique[i].y_org != m->my ||
          unique[i].width != m->mw || unique[i].height != m->mh) {
        dirty = 1;
        m->num = i;
        m->mx = m->wx = unique[i].x_org;
        m->my = m->wy = unique[i].y_org;
        m->mw = m->ww = unique[i].width;
        m->mh = m->wh = unique[i].height;
        updatebarpos(m);
      }
    /* removed monitors if n > nn */
    for (i = nn; i < n; i++) {
      for (m = mons; m && m->next; m = m->next)
        ;
      while ((c = m->clients)) {
        dirty = 1;
        m->clients = c->next;
        detachstack(c);
        c->mon = mons;
        attach(c);
        attachstack(c);
      }
      if (m == selmon)
        selmon = mons;
      cleanupmon(m);
    }
    free(unique);
  } else
#endif /* XINERAMA */
  {    /* default monitor setup */
    if (!mons)
      mons = createmon();
    if (mons->mw != sw || mons->mh != sh) {
      dirty = 1;
      mons->mw = mons->ww = sw;
      mons->mh = mons->wh = sh;
      updatebarpos(mons);
    }
  }
  if (dirty) {
    selmon = mons;
    selmon = wintomon(state, root);
  }
  return dirty;
}

void updatenumlockmask(program_state *state) {
  unsigned int i, j;
  XModifierKeymap *modmap;

  numlockmask = 0;
  modmap = XGetModifierMapping(state->dpy);
  for (i = 0; i < 8; i++)
    for (j = 0; j < modmap->max_keypermod; j++)
      if (modmap->modifiermap[i * modmap->max_keypermod + j] ==
          XKeysymToKeycode(state->dpy, XK_Num_Lock))
        numlockmask = (1 << i);
  XFreeModifiermap(modmap);
}

void updatesizehints(program_state *state, Client *c) {
  long msize;
  XSizeHints size;

  if (!XGetWMNormalHints(state->dpy, c->win, &size, &msize))
    /* size is uninitialized, ensure that size.flags aren't used */
    size.flags = PSize;
  if (size.flags & PBaseSize) {
    c->basew = size.base_width;
    c->baseh = size.base_height;
  } else if (size.flags & PMinSize) {
    c->basew = size.min_width;
    c->baseh = size.min_height;
  } else
    c->basew = c->baseh = 0;
  if (size.flags & PResizeInc) {
    c->incw = size.width_inc;
    c->inch = size.height_inc;
  } else
    c->incw = c->inch = 0;
  if (size.flags & PMaxSize) {
    c->maxw = size.max_width;
    c->maxh = size.max_height;
  } else
    c->maxw = c->maxh = 0;
  if (size.flags & PMinSize) {
    c->minw = size.min_width;
    c->minh = size.min_height;
  } else if (size.flags & PBaseSize) {
    c->minw = size.base_width;
    c->minh = size.base_height;
  } else
    c->minw = c->minh = 0;
  if (size.flags & PAspect) {
    c->mina = (float)size.min_aspect.y / size.min_aspect.x;
    c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
  } else
    c->maxa = c->mina = 0.0;
  c->isfixed = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);
  c->hintsvalid = 1;
}

void updatestatus(program_state *state) {
  if (!gettextprop(state, root, XA_WM_NAME, stext, sizeof(stext)))
    strcpy(stext, "dwm-" VERSION);
  drawbar(selmon);
}

void updatetitle(program_state *state, Client *c) {
  if (!gettextprop(state, c->win, netatom[NetWMName], c->name, sizeof c->name))
    gettextprop(state, c->win, XA_WM_NAME, c->name, sizeof c->name);
  if (c->name[0] == '\0') /* hack to mark broken clients */
    strcpy(c->name, broken);
}

void updatewindowtype(program_state *state, Client *c) {
  Atom _state = getatomprop(state, c, netatom[NetWMState]);
  Atom wtype = getatomprop(state, c, netatom[NetWMWindowType]);

  if (_state == netatom[NetWMFullscreen])
    setfullscreen(state, c, 1);
  if (wtype == netatom[NetWMWindowTypeDialog])
    c->isfloating = 1;
}

void updatewmhints(program_state *state, Client *c) {
  XWMHints *wmh;

  if ((wmh = XGetWMHints(state->dpy, c->win))) {
    if (c == selmon->sel && wmh->flags & XUrgencyHint) {
      wmh->flags &= ~XUrgencyHint;
      XSetWMHints(state->dpy, c->win, wmh);
    } else
      c->isurgent = (wmh->flags & XUrgencyHint) ? 1 : 0;
    if (wmh->flags & InputHint)
      c->neverfocus = !wmh->input;
    else
      c->neverfocus = 0;
    XFree(wmh);
  }
}

void view(program_state *state, const Arg *arg) {
  if ((arg->ui & TAGMASK) == selmon->tagset[selmon->seltags])
    return;
  selmon->seltags ^= 1; /* toggle sel tagset */
  if (arg->ui & TAGMASK)
    selmon->tagset[selmon->seltags] = arg->ui & TAGMASK;
  focus(state, NULL);
  arrange(state, selmon);
}

Client *wintoclient(Window w) {
  Client *c;
  Monitor *m;

  for (m = mons; m; m = m->next)
    for (c = m->clients; c; c = c->next)
      if (c->win == w)
        return c;
  return NULL;
}

Monitor *wintomon(program_state *state, Window w) {
  int x, y;
  Client *c;
  Monitor *m;

  if (w == root && getrootptr(state, &x, &y))
    return recttomon(x, y, 1, 1);
  for (m = mons; m; m = m->next)
    if (w == m->barwin)
      return m;
  if ((c = wintoclient(w)))
    return c->mon;
  return selmon;
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's). Other types of errors call Xlibs
 * default error handler, which may call exit. */
int xerror(Display *dpy, XErrorEvent *ee) {
  if (ee->error_code == BadWindow ||
      (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch) ||
      (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable) ||
      (ee->request_code == X_PolyFillRectangle &&
       ee->error_code == BadDrawable) ||
      (ee->request_code == X_PolySegment && ee->error_code == BadDrawable) ||
      (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch) ||
      (ee->request_code == X_GrabButton && ee->error_code == BadAccess) ||
      (ee->request_code == X_GrabKey && ee->error_code == BadAccess) ||
      (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
    return 0;
  fprintf(stderr, "dwm: fatal error: request code=%d, error code=%d\n",
          ee->request_code, ee->error_code);
  return xerrorxlib(dpy, ee); /* may call exit */
}

int xerrordummy(Display *dpy, XErrorEvent *ee) { return 0; }

/* Startup Error handler to check if another window manager
 * is already running. */
int xerrorstart(Display *dpy, XErrorEvent *ee) {
  die("dwm: another window manager is already running");
  return -1;
}

void zoom(program_state *state, const Arg *arg) {
  Client *c = selmon->sel;

  if (!selmon->lt[selmon->sellt]->arrange || !c || c->isfloating)
    return;
  if (c == nexttiled(selmon->clients) && !(c = nexttiled(c->next)))
    return;
  pop(state, c);
}
void check_ussage(int argc, char *argv[]) {
  if (argc == 2 && !strcmp("-v", argv[1]))
    die("dwm-" VERSION);
  else if (argc != 1)
    die("usage: dwm [-v]");
}

program_state *init_state(void) {
  program_state *state = malloc(sizeof(program_state));

  if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
    fputs("warning: no locale support\n", stderr);
  if (!(state->dpy = XOpenDisplay(NULL)))
    die("dwm: cannot open display");
  return state;
}
void free_state(program_state *state) {
  XCloseDisplay(state->dpy);
  free(state);
}
