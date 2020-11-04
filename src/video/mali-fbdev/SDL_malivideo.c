/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2014 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_MALI

/* SDL internals */
#include "../SDL_sysvideo.h"
#include "SDL_version.h"
#include "SDL_syswm.h"
#include "SDL_loadso.h"
#include "SDL_events.h"
#include "../../events/SDL_events_c.h"

#ifdef SDL_INPUT_LINUXEV
#include "../../core/linux/SDL_evdev.h"
#endif

#include "SDL_malivideo.h"
#include "SDL_maliopengles.h"
#include "osd.h"

static int
MALI_Available(void)
{
    int fd = open("/dev/mali0", O_RDONLY);
	if (fd < 0)
        return 0;
    close(fd);
    return 1;
}

static void
MALI_Destroy(SDL_VideoDevice * device)
{
    if (device->driverdata != NULL) {
        SDL_free(device->driverdata);
        device->driverdata = NULL;
    }
}

static SDL_VideoDevice *
MALI_Create()
{
    SDL_VideoDevice *device;

    /* Initialize SDL_VideoDevice structure */
    device = (SDL_VideoDevice *) SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (device == NULL) {
        SDL_OutOfMemory();
        return NULL;
    }

    device->driverdata = NULL;

    /* Setup amount of available displays and current display */
    device->num_displays = 0;

    /* Set device free function */
    device->free = MALI_Destroy;

    /* Setup all functions which we can handle */
    device->VideoInit = MALI_VideoInit;
    device->VideoQuit = MALI_VideoQuit;
    device->GetDisplayModes = MALI_GetDisplayModes;
    device->SetDisplayMode = MALI_SetDisplayMode;
    device->CreateSDLWindow = MALI_CreateWindow;
    device->SetWindowTitle = MALI_SetWindowTitle;
    device->SetWindowPosition = MALI_SetWindowPosition;
    device->SetWindowSize = MALI_SetWindowSize;
    device->ShowWindow = MALI_ShowWindow;
    device->HideWindow = MALI_HideWindow;
    device->DestroyWindow = MALI_DestroyWindow;
    device->GetWindowWMInfo = MALI_GetWindowWMInfo;

    device->GL_LoadLibrary = MALI_GLES_LoadLibrary;
    device->GL_GetProcAddress = MALI_GLES_GetProcAddress;
    device->GL_UnloadLibrary = MALI_GLES_UnloadLibrary;
    device->GL_CreateContext = MALI_GLES_CreateContext;
    device->GL_MakeCurrent = MALI_GLES_MakeCurrent;
    device->GL_SetSwapInterval = MALI_GLES_SetSwapInterval;
    device->GL_GetSwapInterval = MALI_GLES_GetSwapInterval;
    device->GL_SwapWindow = MALI_GLES_SwapWindow;
    device->GL_DeleteContext = MALI_GLES_DeleteContext;

    device->PumpEvents = MALI_PumpEvents;

    return device;
}

VideoBootStrap MALI_bootstrap = {
    "mali",
    "Mali EGL Video Driver",
    MALI_Available,
    MALI_Create
};

/*****************************************************************************/
/* SDL Video and Display initialization/handling functions                   */
/*****************************************************************************/

int
mali_set_framebuffer_resolution(int width, int height)
{
    int fd;
    int modeFd;
    struct fb_var_screeninfo vinfo;
    __u32 src_rect[4] = { 0, 0, width - 1, height - 1 };
    __u32 dst_rect[4] = { 0, 0, width - 1, height - 1 };

	fd = open("/dev/fb0", O_RDWR, 0);
	if (fd<0) {
		SDL_SetError("MALI: Open Framebuffer failed!");
		return -1;
	}

	if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) == 0) {
		vinfo.xres = width;
		vinfo.yres = height;
		vinfo.xres_virtual = width;
		vinfo.yres_virtual = height * 3;
		vinfo.bits_per_pixel = 32;
		vinfo.activate = FB_ACTIVATE_ALL;
		ioctl(fd, FBIOPUT_VSCREENINFO, &vinfo);
#if DREAMBOX_DEBUG
		fprintf(stderr, "MALI: Set Framebuffer Resolution: %dx%d\n", width, height);
#endif
        //set video mode on amlogic
        modeFd = open("/sys/class/display/mode", O_WRONLY);
        if (modeFd > 0) {
            write(modeFd, "720p50", strlen("720p50"));
            close(modeFd);
        } else {
            fprintf(stderr, "MALI: could not set video output mode");
        }
        if (ioctl(fd, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0) < 0)
            fprintf(stderr, "MALI: FBIOPUT_OSD_FREE_SCALE_ENABLE");
        if (ioctl(fd, FBIOPUT_OSD_FREE_SCALE_MODE, 1) < 0)
            fprintf(stderr, "MALI: FBIOPUT_OSD_FREE_SCALE_MODE");

        if (ioctl(fd, FBIOPUT_OSD_FREE_SCALE_AXIS, src_rect) < 0)
            fprintf(stderr, "MALI: FBIOPUT_OSD_FREE_SCALE_AXIS");

        if (ioctl(fd, FBIOPUT_OSD_WINDOW_AXIS, dst_rect) < 0)
            fprintf(stderr, "MALI: FBIOPUT_OSD_WINDOW_AXIS");

        if (ioctl(fd, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0x10001) < 0)
            fprintf(stderr, "MALI: FBIOPUT_OSD_FREE_SCALE_ENABLE");

	} else {
		SDL_SetError("MALI: Open Framebuffer ioctl failed!");
		return -1;
	}
	close(fd);
	return 0;
}

int
MALI_VideoInit(_THIS)
{
    SDL_VideoDisplay display;
    SDL_DisplayMode current_mode;
    SDL_DisplayData *data;

    data = (SDL_DisplayData *) SDL_calloc(1, sizeof(SDL_DisplayData));
    if (data == NULL) {
        return SDL_OutOfMemory();
    }

    SDL_zero(current_mode);
    current_mode.w = 1280;
    current_mode.h = 720;
    /* FIXME: Is there a way to tell the actual refresh rate? */
    current_mode.refresh_rate = 50;
    /* 32 bpp for default */
    //current_mode.format = SDL_PIXELFORMAT_ABGR8888;
    current_mode.format = SDL_PIXELFORMAT_RGBX8888;
    current_mode.driverdata = NULL;
    mali_set_framebuffer_resolution(current_mode.w, current_mode.h);

    data->native_display.width = current_mode.w;
    data->native_display.height = current_mode.h;

    SDL_zero(display);
    display.desktop_mode = current_mode;
    display.current_mode = current_mode;
    display.driverdata = data;

    SDL_AddVideoDisplay(&display);

#ifdef SDL_INPUT_LINUXEV
    if (SDL_EVDEV_Init() < 0) {
        return -1;
    }
#endif

    return 0;
}

void
MALI_VideoQuit(_THIS)
{
    /* Clear the framebuffer and ser cursor on again */
    int fd = open("/dev/tty", O_RDWR);
    ioctl(fd, VT_ACTIVATE, 5);
    ioctl(fd, VT_ACTIVATE, 1);
    close(fd);

#ifdef SDL_INPUT_LINUXEV
    SDL_EVDEV_Quit();
#endif

}

void
MALI_GetDisplayModes(_THIS, SDL_VideoDisplay * display)
{
    /* Only one display mode available, the current one */
    SDL_AddDisplayMode(display, &display->current_mode);
}

int
MALI_SetDisplayMode(_THIS, SDL_VideoDisplay * display, SDL_DisplayMode * mode)
{
    return 0;
}

int
MALI_CreateWindow(_THIS, SDL_Window * window)
{
    SDL_WindowData *windowdata;
    SDL_DisplayData *displaydata;

    displaydata = SDL_GetDisplayDriverData(0);

    /* Allocate window internal data */
    windowdata = (SDL_WindowData *) SDL_calloc(1, sizeof(SDL_WindowData));
    if (windowdata == NULL) {
        return SDL_OutOfMemory();
    }

    /* Windows have one size for now */
    window->w = displaydata->native_display.width;
    window->h = displaydata->native_display.height;

    /* OpenGL ES is the law here */
    window->flags |= SDL_WINDOW_OPENGL;

    if (!_this->egl_data) {
        if (SDL_GL_LoadLibrary(NULL) < 0) {
            return -1;
        }
    }
    windowdata->egl_surface = SDL_EGL_CreateSurface(_this, (NativeWindowType) &displaydata->native_display);

    if (windowdata->egl_surface == EGL_NO_SURFACE) {
        MALI_VideoQuit(_this);
        return SDL_SetError("mali-fbdev: Can't create EGL window surface");
    }

    /* Setup driver data for this window */
    window->driverdata = windowdata;

    /* One window, it always has focus */
    SDL_SetMouseFocus(window);
    SDL_SetKeyboardFocus(window);

    /* Window has been successfully created */
    return 0;
}

void
MALI_DestroyWindow(_THIS, SDL_Window * window)
{
    SDL_WindowData *data;

    data = window->driverdata;
    if (data) {
        if (data->egl_surface != EGL_NO_SURFACE) {
            SDL_EGL_DestroySurface(_this, data->egl_surface);
            data->egl_surface = EGL_NO_SURFACE;
        }
        SDL_free(data);
    }
    window->driverdata = NULL;
}

void
MALI_SetWindowTitle(_THIS, SDL_Window * window)
{
}

void
MALI_SetWindowPosition(_THIS, SDL_Window * window)
{
}

void
MALI_SetWindowSize(_THIS, SDL_Window * window)
{
}

void
MALI_ShowWindow(_THIS, SDL_Window * window)
{
}

void
MALI_HideWindow(_THIS, SDL_Window * window)
{
}

/*****************************************************************************/
/* SDL Window Manager function                                               */
/*****************************************************************************/
SDL_bool
MALI_GetWindowWMInfo(_THIS, SDL_Window * window, struct SDL_SysWMinfo *info)
{
    if (info->version.major == SDL_MAJOR_VERSION &&
        info->version.minor == SDL_MINOR_VERSION) {
        SDL_WindowData *windowdata = (SDL_WindowData *) window->driverdata;
        SDL_DisplayData *displaydata = SDL_GetDisplayDriverData(0);
        info->subsystem = SDL_SYSWM_MALI;
        info->info.mali.display = &displaydata->native_display;
        info->info.mali.window = &displaydata->native_display;
        return SDL_TRUE;

    } else {
        SDL_SetError("application not compiled with SDL %d.%d\n",
            SDL_MAJOR_VERSION, SDL_MINOR_VERSION);
    }

    /* Failed to get window manager information */
    return SDL_FALSE;
}

/*****************************************************************************/
/* SDL event functions                                                       */
/*****************************************************************************/
void MALI_PumpEvents(_THIS)
{
#ifdef SDL_INPUT_LINUXEV
    SDL_EVDEV_Poll();
#endif
}

#endif /* SDL_VIDEO_DRIVER_MALI */

/* vi: set ts=4 sw=4 expandtab: */

