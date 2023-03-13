/*  gcc -o bresenham_raytrace -fsanitize=address,leak bresenham_raytrace.c -lX11
 */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

typedef struct {
  uint8_t v[4];
} color_t;

enum { DEFAULT_WIDTH = 800, DEFAULT_HEIGHT = 600 };

enum {
  STATUS_OK = 0,
  STATUS_ERROR_BAD_ALLOC,
  STATUS_ERROR_X_OPEN_DISPLAY_FAILED,
  STATUS_ERROR_X_CREATE_IMAGE_FAILED
};

void render(ptrdiff_t width, ptrdiff_t height, color_t *data) {
  assert(data != NULL);

  if (width <= 0 || height <= 0 || data == NULL)
    return;

  for (ptrdiff_t j = 0; j < height; j++)
    for (ptrdiff_t i = 0; i < width; i++) {
      color_t *pixel = data + j * width + i;
      pixel->v[0]    = 0xff & i;
      pixel->v[1]    = 0xff & j;
      pixel->v[2]    = 0xff & (i + j);
      pixel->v[3]    = 0xff;
    }
}

typedef struct {
  int       status;
  XImage   *image;
  color_t  *data;
  ptrdiff_t width;
  ptrdiff_t height;
} renderbuffer_t;

renderbuffer_t renderbuffer_init(Display *display, Window window,
                                 ptrdiff_t width, ptrdiff_t height) {
  assert(display != NULL);
  assert(width > 0);
  assert(height > 0);

  renderbuffer_t buf;
  memset(&buf, 0, sizeof buf);

  buf.width  = width;
  buf.height = height;

  buf.data = (color_t *) malloc(width * height * sizeof(color_t));

  if (buf.data == NULL) {
    fputs("Error: Bad alloc.\n", stderr);
    buf.status = STATUS_ERROR_BAD_ALLOC;
    return buf;
  }

  XWindowAttributes wa;
  XGetWindowAttributes(display, window, &wa);

  buf.image = XCreateImage(display, wa.visual, wa.depth, ZPixmap, 0,
                           (char *) buf.data, width, height, 32, 0);

  if (buf.image == NULL) {
    fputs("Error: XCreateImage failed.\n", stderr);
    buf.status = STATUS_ERROR_X_CREATE_IMAGE_FAILED;
    free(buf.data);
    buf.data = NULL;
  }

  return buf;
}

void renderbuffer_destroy(renderbuffer_t buf) {
  assert(buf.image != NULL);

  XDestroyImage(buf.image);
}

int main(int argc, char **argv) {
  Display *display = XOpenDisplay(NULL);

  if (display == NULL) {
    fputs("Error: XOpenDisplay failed.\n", stderr);
    return STATUS_ERROR_X_OPEN_DISPLAY_FAILED;
  }

  int screen_number = XDefaultScreen(display);

  Window window = XCreateSimpleWindow(
      display, RootWindow(display, screen_number), 0, 0,
      DEFAULT_WIDTH, DEFAULT_HEIGHT, 0,
      BlackPixel(display, screen_number),
      WhitePixel(display, screen_number));

  GC gc = XCreateGC(display, window, 0, NULL);

  renderbuffer_t buf = renderbuffer_init(
      display, window, DEFAULT_WIDTH, DEFAULT_HEIGHT);

  if (buf.status != STATUS_OK) {
    XFreeGC(display, gc);
    XCloseDisplay(display);
    return buf.status;
  }

  XStoreName(display, window, "Bresenham Raytrace");

  Atom wm_protocols     = XInternAtom(display, "WM_PROTOCOLS", 0);
  Atom wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", 0);
  XSetWMProtocols(display, window, &wm_delete_window, 1);

  XSelectInput(display, window,
               StructureNotifyMask | PointerMotionMask |
                   ButtonPressMask | ButtonReleaseMask |
                   FocusChangeMask | KeyPressMask | KeyReleaseMask);

  XMapWindow(display, window);

  for (int done = 0; !done;) {
    while (XPending(display) > 0) {
      XEvent event;
      XNextEvent(display, &event);

      switch (event.type) {
        case MotionNotify: break;
        case KeyPress: break;
        case KeyRelease: break;
        case ButtonPress: break;
        case ButtonRelease: break;
        case FocusIn: break;
        case FocusOut: break;

        case ConfigureNotify:
          if (event.xconfigure.width > 0 &&
              event.xconfigure.height > 0 &&
              (buf.width != event.xconfigure.width ||
               buf.height != event.xconfigure.height)) {
            renderbuffer_destroy(buf);
            buf = renderbuffer_init(display, window,
                                    event.xconfigure.width,
                                    event.xconfigure.height);
            if (buf.status != STATUS_OK)
              done = 1;
          }
          break;

        case ClientMessage:
          if (event.xclient.message_type == wm_protocols &&
              (Atom) event.xclient.data.l[0] == wm_delete_window)
            done = 1;
          break;
      }
    }

    render(buf.width, buf.height, buf.data);

    XPutImage(display, window, gc, buf.image, 0, 0, 0, 0, buf.width,
              buf.height);

    usleep(100);
  }

  renderbuffer_destroy(buf);
  XFreeGC(display, gc);
  XCloseDisplay(display);

  return STATUS_OK;
}
