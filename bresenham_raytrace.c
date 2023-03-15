/*  Bresenham raytracing voxel renderer.
 */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void on_init() { }
void on_cleanup() { }
void on_key_down(ptrdiff_t const key) { }
void on_key_up(ptrdiff_t const key) { }
void on_button_down(ptrdiff_t const button) { }
void on_button_up(ptrdiff_t const button) { }
void on_mouse_wheel(ptrdiff_t const x_delta,
                    ptrdiff_t const y_delta) { }
void on_mouse_motion(ptrdiff_t const x, ptrdiff_t const y,
                     ptrdiff_t const x_delta,
                     ptrdiff_t const y_delta) { }
void on_update(int64_t const time_elapsed) { }

typedef struct {
  uint8_t v[4];
} color_t;

static_assert(sizeof(color_t) == 4, "Pixel size check");

void on_render(ptrdiff_t const width, ptrdiff_t const height,
               ptrdiff_t const pitch, color_t *const data) {
  assert(data != NULL);

  for (ptrdiff_t j = 0; j < height; j++)
    for (ptrdiff_t i = 0; i < width; i++) {
      color_t *pixel = (color_t *) ((char *) data + j * pitch) + i;
      pixel->v[0]    = 0xff;
      pixel->v[1]    = 0xff & (i + j);
      pixel->v[2]    = 0xff & j;
      pixel->v[3]    = 0xff & i;
    }
}

/*  System setup with SDL.
 */

#include <SDL2/SDL.h>
#include <SDL2/SDL_video.h>

#ifdef __EMSCRIPTEN__
#  include <emscripten.h>
#  include <emscripten/html5.h>
#endif

enum {
#ifdef __EMSCRIPTEN__
  DEFAULT_WINDOW_WIDTH  = 800,
  DEFAULT_WINDOW_HEIGHT = 600,
#else
  DEFAULT_WINDOW_WIDTH  = 1024,
  DEFAULT_WINDOW_HEIGHT = 768,
#endif
  FULLSCREEN_WIDTH  = 1280,
  FULLSCREEN_HEIGHT = 720,
  FRAME_WINDOW      = 200,
  MAX_FPS           = 120
};

enum {
  STATUS_OK = 0,
  STATUS_DONE,
  STATUS_ERROR_BAD_ALLOC,
  STATUS_ERROR_SDL_INIT_FAILED,
  STATUS_ERROR_SDL_CREATE_WINDOW_FAILED,
  STATUS_ERROR_SDL_CREATE_RENDERER_FAILED,
  STATUS_ERROR_SDL_GET_RENDERER_OUTPUT_SIZE_FAILED,
  STATUS_ERROR_SDL_CREATE_TEXTURE_FAILED,
  STATUS_ERROR_SDL_LOCK_TEXTURE_FAILED,
  STATUS_ERROR_SDL_RENDER_COPY_FAILED
};

struct {
  SDL_Window   *window;
  SDL_Renderer *renderer;
  SDL_Texture  *buffer;
  int           done;
  int           width;
  int           height;
  int64_t       time;
  int           is_alt;
  int           is_fullscreen;
  int           frames;
  int64_t       time_frame;
  int64_t       frame_padding;
} g_app;

int frame(int64_t time_elapsed) {
  int width, height;

  if (SDL_GetRendererOutputSize(g_app.renderer, &width, &height) !=
      0) {
    printf("SDL_GetRendererOutputSize failed: %s\n", SDL_GetError());
    g_app.done = 1;
    return STATUS_ERROR_SDL_GET_RENDERER_OUTPUT_SIZE_FAILED;
  }

  if (width > 0 && height > 0 &&
      (g_app.width != width || g_app.height != height)) {
    if (g_app.buffer != NULL)
      SDL_DestroyTexture(g_app.buffer);

    g_app.buffer = SDL_CreateTexture(
        g_app.renderer, SDL_PIXELFORMAT_RGBX8888,
        SDL_TEXTUREACCESS_STREAMING, width, height);

    if (g_app.buffer == NULL) {
      printf("SDL_CreateTexture failed: %s\n", SDL_GetError());
      g_app.done = 1;
      return STATUS_ERROR_SDL_CREATE_TEXTURE_FAILED;
    }

    g_app.width  = width;
    g_app.height = height;
  }

  on_update(time_elapsed);

  if (g_app.buffer != NULL) {
    int      pitch;
    color_t *data;

    if (SDL_LockTexture(g_app.buffer, NULL, (void **) &data,
                        &pitch) != 0) {
      printf("SDL_LockTexture failed: %s\n", SDL_GetError());
      g_app.done = 1;
      return STATUS_ERROR_SDL_LOCK_TEXTURE_FAILED;
    }

    if (g_app.width > 0 && g_app.height > 0 && pitch > 0 &&
        data != NULL)
      on_render(g_app.width, g_app.height, pitch, data);

    SDL_UnlockTexture(g_app.buffer);

    if (SDL_RenderCopy(g_app.renderer, g_app.buffer, NULL, NULL) !=
        0) {
      printf("SDL_RenderCopy failed: %s\n", SDL_GetError());
      g_app.done = 1;
      return STATUS_ERROR_SDL_RENDER_COPY_FAILED;
    }
  }

  SDL_RenderPresent(g_app.renderer);

  return STATUS_OK;
}

void loop() {
  SDL_Event event;
  while (SDL_PollEvent(&event) == 1) {
    switch (event.type) {
      case SDL_MOUSEMOTION:
        on_mouse_motion(event.motion.x, event.motion.y,
                        event.motion.xrel, event.motion.yrel);
        break;

      case SDL_MOUSEWHEEL:
        on_mouse_wheel(event.wheel.preciseX, event.wheel.preciseY);
        break;

      case SDL_KEYDOWN:
        if (event.key.repeat == 0) {
          if (event.key.keysym.sym == SDLK_LALT ||
              event.key.keysym.sym == SDLK_RALT)
            g_app.is_alt++;
#ifndef __EMSCRIPTEN__
          if (g_app.is_alt > 0 &&
              event.key.keysym.sym == SDLK_RETURN) {
            if (!g_app.is_fullscreen) {
              SDL_SetWindowFullscreen(g_app.window,
                                      SDL_WINDOW_FULLSCREEN);
              SDL_SetWindowSize(g_app.window, FULLSCREEN_WIDTH,
                                FULLSCREEN_HEIGHT);
              g_app.is_fullscreen = 1;
            } else {
              SDL_SetWindowFullscreen(g_app.window, 0);
              g_app.is_fullscreen = 0;
            }
          }
#endif
          on_key_down(event.key.keysym.scancode);
        }
        break;

      case SDL_KEYUP:
        if ((event.key.keysym.sym == SDLK_LALT ||
             event.key.keysym.sym == SDLK_RALT) &&
            g_app.is_alt > 0)
          g_app.is_alt--;
        on_key_up(event.key.keysym.scancode);
        break;

      case SDL_MOUSEBUTTONDOWN:
        on_button_down(event.button.button);
        break;

      case SDL_MOUSEBUTTONUP:
        on_button_up(event.button.button);
        break;

      case SDL_QUIT:
        g_app.done = 1;
        on_cleanup();
        return;
    }
  }

  int64_t time_now     = SDL_GetTicks64();
  int64_t time_elapsed = time_now - g_app.time;

  g_app.time = time_now;

  assert(time_elapsed >= 0);

  if (time_elapsed >= 0 && frame(time_elapsed) != STATUS_OK)
    g_app.done = 1;

  g_app.frames++;
  g_app.time_frame += time_elapsed;

  if (g_app.time_frame >= FRAME_WINDOW && g_app.frames > 0) {
    printf("FPS: %3d    \r",
           (int) ((g_app.frames * 1000) / FRAME_WINDOW));
    fflush(stdout);
    g_app.frame_padding += 1000 / MAX_FPS -
                           FRAME_WINDOW / g_app.frames;
    g_app.frames = 0;
    g_app.time_frame -= FRAME_WINDOW;
  }

  if (g_app.frame_padding > 0)
    SDL_Delay(g_app.frame_padding);
}

int main(int argc, char **argv) {
  memset(&g_app, 0, sizeof g_app);

  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    printf("SDL_Init failed: %s\n", SDL_GetError());
    return STATUS_ERROR_SDL_INIT_FAILED;
  }

  g_app.window = SDL_CreateWindow(
      "Bresenham Raytrace", SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED, DEFAULT_WINDOW_WIDTH,
      DEFAULT_WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE);

  if (g_app.window == NULL) {
    printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
    return STATUS_ERROR_SDL_CREATE_WINDOW_FAILED;
  }

  g_app.renderer = SDL_CreateRenderer(g_app.window, -1,
                                      SDL_RENDERER_ACCELERATED);

  if (g_app.renderer == NULL) {
    printf("SDL_CreateRenderer failed: %s\n", SDL_GetError());
    return STATUS_ERROR_SDL_CREATE_RENDERER_FAILED;
  }

#ifdef __EMSCRIPTEN__
  emscripten_set_main_loop(loop, 0, 0);
#endif

  on_init();

  g_app.time = SDL_GetTicks64();

#ifndef __EMSCRIPTEN__
  while (!g_app.done) loop();

  if (g_app.buffer != NULL)
    SDL_DestroyTexture(g_app.buffer);

  SDL_DestroyRenderer(g_app.renderer);
  SDL_DestroyWindow(g_app.window);
  SDL_Quit();
#endif

  return STATUS_OK;
}

