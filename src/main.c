#include "dwm.h"
int main(int argc, char *argv[]) {

  check_ussage(argc, argv);
  checkotherwm();
  setup();
#ifdef __OpenBSD__
  if (pledge("stdio rpath proc exec", NULL) == -1)
    die("pledge");
#endif /* __OpenBSD__ */
  scan();
  run();
  cleanup();
  XCloseDisplay(dpy);
  return EXIT_SUCCESS;
}
