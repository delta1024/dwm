#ifndef dwm_h
#define dwm_h
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

#include "drw.h"
#include "util.h"

typedef struct internal_state program_state;
typedef union {
  int i;
  unsigned int ui;
  float f;
  const void *v;
} Arg;

typedef struct {
  unsigned int click;
  unsigned int mask;
  unsigned int button;
  void (*func)(program_state *,const Arg *arg);
  const Arg arg;
} Button;

typedef struct Monitor Monitor;
typedef struct Client Client;
struct Client {
  char name[256];
  float mina, maxa;
  int x, y, w, h;
  int oldx, oldy, oldw, oldh;
  int basew, baseh, incw, inch, maxw, maxh, minw, minh, hintsvalid;
  int bw, oldbw;
  unsigned int tags;
  int isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen;
  Client *next;
  Client *snext;
  Monitor *mon;
  Window win;
};

typedef struct {
  unsigned int mod;
  KeySym keysym;
  void (*func)(program_state *,const Arg *);
  const Arg arg;
} Key;

typedef struct {
  const char *symbol;
  void (*arrange)(program_state *,Monitor *);
} Layout;

struct Monitor {
  char ltsymbol[16];
  float mfact;
  int nmaster;
  int num;
  int by;             /* bar geometry */
  int mx, my, mw, mh; /* screen size */
  int wx, wy, ww, wh; /* window area  */
  unsigned int seltags;
  unsigned int sellt;
  unsigned int tagset[2];
  int showbar;
  int topbar;
  Client *clients;
  Client *sel;
  Client *stack;
  Monitor *next;
  Window barwin;
  const Layout *lt[2];
};

typedef struct {
  const char *class;
  const char *instance;
  const char *title;
  unsigned int tags;
  int isfloating;
  int monitor;
} Rule;




// extern Display *dpy;
/* function declarations */
void applyrules(program_state *state, Client *c);
int applysizehints(program_state *state,Client *c, int *x, int *y, int *w, int *h, int interact);
void arrange(program_state *state,Monitor *m);
void arrangemon(program_state *state,Monitor *m);
void attach(Client *c);
void attachstack(Client *c);
void buttonpress(program_state *state,XEvent *e);
void checkotherwm(program_state *state);
void cleanup(program_state *state);
void cleanupmon(program_state *state,Monitor *mon);
void clientmessage(program_state *state,XEvent *e);
void configure(program_state *state,Client *c);
void configurenotify(program_state *state,XEvent *e);
void configurerequest(program_state *state,XEvent *e);
Monitor *createmon(void);
void destroynotify(program_state *state,XEvent *e);
void detach(Client *c);
void detachstack(Client *c);
Monitor *dirtomon(int dir);
void drawbar(Monitor *m);
void drawbars(void);
void enternotify(program_state *state,XEvent *e);
void expose(program_state *state,XEvent *e);
void focus(program_state *state,Client *c);
void focusin(program_state *state,XEvent *e);
void focusmon(program_state *state, const Arg *arg);
void focusstack(program_state *state,const Arg *arg);
Atom getatomprop(program_state *state,Client *c, Atom prop);
int getrootptr(program_state *state,int *x, int *y);
long getstate(program_state *state, Window w);
int gettextprop(program_state *state,Window w, Atom atom, char *text, unsigned int size);
void grabbuttons(program_state *state,Client *c, int focused);
void grabkeys(program_state *state);
void incnmaster(program_state *state,const Arg *arg);
void keypress(program_state *state,XEvent *e);
void killclient(program_state *state,const Arg *arg);
void manage(program_state *state,Window w, XWindowAttributes *wa);
void mappingnotify(program_state *state,XEvent *e);
void maprequest(program_state *state,XEvent *e);
void monocle(program_state *state,Monitor *m);
void motionnotify(program_state *state,XEvent *e);
void movemouse(program_state *state,const Arg *arg);
Client *nexttiled(Client *c);
void pop(program_state *state,Client *c);
void propertynotify(program_state *state,XEvent *e);
void quit(program_state *state,const Arg *arg);
Monitor *recttomon(int x, int y, int w, int h);
void resize(program_state *state,Client *c, int x, int y, int w, int h, int interact);
void resizeclient(program_state *state,Client *c, int x, int y, int w, int h);
void resizemouse(program_state *state,const Arg *arg);
void restack(program_state *state,Monitor *m);
void run(program_state *state);
void scan(program_state *state);
int sendevent(program_state *state,Client *c, Atom proto);
void sendmon(program_state *state,Client *c, Monitor *m);
void setclientstate(program_state *state,Client *c, long i_state);
void setfocus(program_state *state,Client *c);
void setfullscreen(program_state *state,Client *c, int fullscreen);
void setlayout(program_state *state,const Arg *arg);
void setmfact(program_state *state,const Arg *arg);
void setup(program_state *state);
void seturgent(program_state *state,Client *c, int urg);
void showhide(program_state *state,Client *c);
void spawn(program_state *state,const Arg *arg);
void tag   (program_state *state,const Arg *arg);
void tagmon(program_state *state,const Arg *arg);
void tile(program_state *state,Monitor *m);
void togglebar(program_state *state,const Arg *arg);
void togglefloating(program_state *state,const Arg *arg);
void toggletag (program_state *state,const Arg *arg);
void toggleview(program_state *state,const Arg *arg);
void unfocus(program_state *state,Client *c, int setfocus);
void unmanage(program_state *state,Client *c, int destroyed);
void unmapnotify(program_state *state,XEvent *e);
void updatebarpos(Monitor *m);
void updatebars(program_state *state);
void updateclientlist(program_state *state);
int updategeom(program_state *state);
void updatenumlockmask(program_state *state);
void updatesizehints(program_state *state,Client *c);
void updatestatus(program_state *state);
void updatetitle(program_state *state,Client *c);
void updatewindowtype(program_state *state,Client *c);
void updatewmhints(program_state *state,Client *c);
void view(program_state *state,const Arg *arg);
Client *wintoclient(Window w);
Monitor *wintomon(program_state *state,Window w);
int xerror(Display *dpy, XErrorEvent *ee);
int xerrordummy(Display *dpy, XErrorEvent *ee);
int xerrorstart(Display *dpy, XErrorEvent *ee);
void zoom(program_state *state,const Arg *arg);
void check_ussage(int argc, char *argv[]);
program_state *init_state(void);
void free_state(program_state *state);
#endif // dwm_h
