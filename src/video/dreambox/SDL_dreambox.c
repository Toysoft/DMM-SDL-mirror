/*
	Simple DirectMedia Layer
	Copyright (C) 1997-2017 Sam Lantinga <slouken@libsdl.org>

	This software is provided 'as-is', without any express or implied
	warranty.	In no event will the authors be held liable for any damages
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

*	SDL dreambox backend
*	Copyright (C) 2017 Emanuel Strobel
*/
#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_DREAMBOX

/* SDL internals */
#include "../SDL_sysvideo.h"
#include "SDL_version.h"
#include "SDL_syswm.h"
#include "SDL_events.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_keyboard_c.h"

/* DREAM declarations */
#include "SDL_dreambox.h"
#include "SDL_dreambox_events.h"
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include "EGL/egl.h"
#include "GLES2/gl2.h"

#define DREAMBOX_DEBUG

static int
DREAM_available(void)
{
	char name[32]={0x0};
	FILE *fd = fopen("/proc/stb/info/model","r");
	
	if (fd<0) return 0;
	
	fgets(name,32,fd);
	fclose(fd);

	if ( (SDL_strncmp(name, "dm820", 5) == 0) || 
		(SDL_strncmp(name, "dm900", 5) == 0) || 
		(SDL_strncmp(name, "dm7080", 6) == 0) ) {
		
#ifdef DREAMBOX_DEBUG
	fprintf(stderr, "DREAM: available: %s", name);
#endif
		return 1;
	}
	return 0;
}

void 
DREAM_setfbresolution(int width, int height)
{
	int fd;
	struct fb_var_screeninfo vinfo;
	
	fd = open("/dev/fb0", O_RDWR, 0);
	
	if (fd<0) {
#ifdef DREAMBOX_DEBUG
		fprintf(stderr, "ERROR: DREAM: open framebuffer failed\n");
#endif
		return;
	}
	
	if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) == 0) {
		vinfo.xres = width;
		vinfo.yres = height;
		vinfo.xres_virtual = width;
		vinfo.yres_virtual = height * 2;
		vinfo.bits_per_pixel = 32;
		vinfo.activate = FB_ACTIVATE_ALL;
		ioctl(fd, FBIOPUT_VSCREENINFO, &vinfo);
		
#ifdef DREAMBOX_DEBUG
		fprintf(stderr, "DREAM: set framebuffer resolution: %dx%d\n", width, height);
#endif
		
	}
	close(fd);
}

void
DREAM_setvideomode(const char *mode)
{
	FILE *fp = fopen("/proc/stb/video/videomode", "w");
	
#ifdef DREAMBOX_DEBUG
	fprintf(stderr, "DREAM: setvideomode %s\n",mode);
#endif
	
	if (fp) {
		fprintf(fp, "%s", mode);
		fclose(fp);
	}
	else
		SDL_SetError("DREAM: i/o file /proc/stb/video/videomode");
}

void 
DREAM_waitforsync(void)
{
	int fd;
	int c;
	
	c = 0;
	fd = open("/dev/fb0", O_RDWR, 0);
	
	if (fd<0) {
#ifdef DREAMBOX_DEBUG
		fprintf(stderr, "ERROR: DREAM: open framebuffer failed\n");
#endif
		return;
	}
	if ( ioctl(fd, FBIO_WAITFORVSYNC, &c) == 0) {
		
#ifdef DREAMBOX_DEBUG
		fprintf(stderr, "DREAM: Wait for Sync\n");
#endif
		
	}
	close(fd);
}

static void
DREAM_destroy(SDL_VideoDevice * device)
{
#ifdef DREAMBOX_DEBUG
	fprintf(stderr, "DREAM: destroy\n");
#endif
	if (device->driverdata != NULL) {
		SDL_free(device->driverdata);
		device->driverdata = NULL;
	}
	SDL_free(device);
}

static SDL_VideoDevice *
DREAM_create()
{
	SDL_VideoDevice *device;
	SDL_VideoData *phdata;
	int status;
	
#ifdef DREAMBOX_DEBUG
	fprintf(stderr, "DREAM: create\n");
#endif
	/* Check if dreambox could be initialized */
	status = DREAM_available();
	if (status == 0) {
		/* DREAM could not be used */
		return NULL;
	}

	/* Initialize SDL_VideoDevice structure */
	device = (SDL_VideoDevice *) SDL_calloc(1, sizeof(SDL_VideoDevice));
	if (device == NULL) {
		SDL_OutOfMemory();
		return NULL;
	}

	/* Initialize internal Dreambox specific data */
	phdata = (SDL_VideoData *) SDL_calloc(1, sizeof(SDL_VideoData));
	if (phdata == NULL) {
		SDL_OutOfMemory();
		SDL_free(device);
		return NULL;
	}

	device->driverdata = phdata;

	phdata->egl_initialized = SDL_TRUE;


	/* Setup amount of available displays */
	device->num_displays = 0;

	/* Set device free function */
	device->free = DREAM_destroy;

	/* Setup all functions which we can handle */
	device->VideoInit = DREAM_videoinit;
	device->VideoQuit = DREAM_videoquit;
	device->GetDisplayModes = DREAM_getdisplaymodes;
	device->SetDisplayMode = DREAM_setdisplaymode;
	device->CreateWindow = DREAM_createwindow;
	device->CreateWindowFrom = DREAM_createwindowfrom;
	device->SetWindowTitle = DREAM_setwindowtitle;
	device->SetWindowIcon = DREAM_setwindowicon;
	device->SetWindowPosition = DREAM_setwindowposition;
	device->SetWindowSize = DREAM_setwindowsize;
	device->ShowWindow = DREAM_showwindow;
	device->HideWindow = DREAM_hidewindow;
	device->RaiseWindow = DREAM_raisewindow;
	device->MaximizeWindow = DREAM_maximizewindow;
	device->MinimizeWindow = DREAM_minimizewindow;
	device->RestoreWindow = DREAM_restorewindow;
	device->SetWindowGrab = DREAM_setwindowgrab;
	device->DestroyWindow = DREAM_destroywindow;
	device->GetWindowWMInfo = DREAM_getwindowwminfo;
	device->GL_LoadLibrary = DREAM_gl_loadlibrary;
	device->GL_GetProcAddress = DREAM_gl_getprocaddres;
	device->GL_UnloadLibrary = DREAM_gl_unloadlibrary;
	device->GL_CreateContext = DREAM_gl_createcontext;
	device->GL_MakeCurrent = DREAM_gl_makecurrent;
	device->GL_SetSwapInterval = DREAM_gl_setswapinterval;
	device->GL_GetSwapInterval = DREAM_gl_getswapinterval;
	device->GL_SwapWindow = DREAM_gl_swapwindow;
	device->GL_DeleteContext = DREAM_gl_deletecontext;
	device->PumpEvents = DREAM_PumpEvents;

	/* !!! FIXME: implement SetWindowBordered */

	return device;
}

VideoBootStrap DREAM_bootstrap = {

	"dreambox",
	"SDL Dreambox Video Driver",
	
	DREAM_available,
	DREAM_create
};

/*****************************************************************************/
/* SDL Video and Display initialization/handling functions					 */
/*****************************************************************************/
int
DREAM_videoinit(_THIS)
{
	SDL_VideoDisplay display;
	SDL_DisplayMode current_mode;

#ifdef DREAMBOX_DEBUG
	fprintf(stderr, "DREAM: videoinit\n");
#endif

	SDL_zero(current_mode);

	current_mode.w = 1280;
	current_mode.h = 720;

	current_mode.refresh_rate = 50;
	current_mode.format = SDL_PIXELFORMAT_RGBA8888;
	current_mode.driverdata = NULL;

	SDL_zero(display);
	display.desktop_mode = current_mode;
	display.current_mode = current_mode;
	display.driverdata = NULL;
	
	DREAM_setvideomode("1080p");
	DREAM_setfbresolution(current_mode.w, current_mode.h);

	SDL_AddVideoDisplay(&display);

	return 1;
}

void
DREAM_videoquit(_THIS)
{
#ifdef DREAMBOX_DEBUG
	fprintf(stderr, "DREAM: videoquit\n");
#endif
}

void
DREAM_getdisplaymodes(_THIS, SDL_VideoDisplay * display)
{
#ifdef DREAMBOX_DEBUG
	fprintf(stderr, "DREAM: getdisplaymodes\n");
#endif
}

int
DREAM_setdisplaymode(_THIS, SDL_VideoDisplay * display, SDL_DisplayMode * mode)
{
#ifdef DREAMBOX_DEBUG
	fprintf(stderr, "DREAM: setdisplaymode\n");
#endif
	return 0;
}

int
DREAM_createwindow(_THIS, SDL_Window * window)
{
	SDL_VideoData *phdata = (SDL_VideoData *) _this->driverdata;
	SDL_WindowData *wdata;
	
#ifdef DREAMBOX_DEBUG
	fprintf(stderr, "DREAM: createwindow\n");
#endif
	
	/* Allocate window internal data */
	wdata = (SDL_WindowData *) SDL_calloc(1, sizeof(SDL_WindowData));
	if (wdata == NULL) {
		return SDL_OutOfMemory();
	}

	/* Setup driver data for this window */
	window->driverdata = wdata;

	/* Check if window must support OpenGL ES rendering */
	if ((window->flags & SDL_WINDOW_OPENGL) == SDL_WINDOW_OPENGL) {

		EGLBoolean initstatus;

		/* Mark this window as OpenGL ES compatible */
		wdata->uses_gles = SDL_TRUE;

		/* Create connection to OpenGL ES */
		if (phdata->egl_display == EGL_NO_DISPLAY) {
			phdata->egl_display = eglGetDisplay((NativeDisplayType) 0);
			if (phdata->egl_display == EGL_NO_DISPLAY) {
				return SDL_SetError("DREAM: Can't get connection to OpenGL ES");
			}

			initstatus = eglInitialize(phdata->egl_display, NULL, NULL);
			if (initstatus != EGL_TRUE) {
				return SDL_SetError("DREAM: Can't init OpenGL ES library");
			}
		}

		phdata->egl_refcount++;
	}

	/* Window has been successfully created */
	return 0;
}

int
DREAM_createwindowfrom(_THIS, SDL_Window * window, const void *data)
{
	return -1;
}

void
DREAM_setwindowtitle(_THIS, SDL_Window * window)
{
}
void
DREAM_setwindowicon(_THIS, SDL_Window * window, SDL_Surface * icon)
{
}
void
DREAM_setwindowposition(_THIS, SDL_Window * window)
{
}
void
DREAM_setwindowsize(_THIS, SDL_Window * window)
{
}

void
DREAM_showwindow(_THIS, SDL_Window * window)
{
	FILE *fp = fopen("/proc/stb/video/alpha", "w");
	
#ifdef DREAMBOX_DEBUG
	fprintf(stderr, "DREAM: showwindow\n");
#endif
	
	if (fp) {
		fprintf(fp, "%d", 255);
		fclose(fp);
	}
	else
		SDL_SetError("DREAM: i/o file /proc/stb/video/alpha");
}

void
DREAM_hidewindow(_THIS, SDL_Window * window)
{
	FILE *fp = fopen("/proc/stb/video/alpha", "w");
	
#ifdef DREAMBOX_DEBUG
	fprintf(stderr, "DREAM: hidewindow\n");
#endif
	
	if (fp) {
		fprintf(fp, "%d", 0);
		fclose(fp);
	}
	else
		SDL_SetError("DREAM: i/o file /proc/stb/video/alpha");
}

void
DREAM_raisewindow(_THIS, SDL_Window * window)
{
}
void
DREAM_maximizewindow(_THIS, SDL_Window * window)
{
}
void
DREAM_minimizewindow(_THIS, SDL_Window * window)
{
}
void
DREAM_restorewindow(_THIS, SDL_Window * window)
{
}
void
DREAM_setwindowgrab(_THIS, SDL_Window * window, SDL_bool grabbed)
{
}
void
DREAM_destroywindow(_THIS, SDL_Window * window)
{
	SDL_VideoData *phdata = (SDL_VideoData *) _this->driverdata;
	eglTerminate(phdata->egl_display);
}

/*****************************************************************************/
/* SDL Window Manager function                                               */
/*****************************************************************************/
SDL_bool
DREAM_getwindowwminfo(_THIS, SDL_Window * window, struct SDL_SysWMinfo *info)
{
	if (info->version.major <= SDL_MAJOR_VERSION) {
		return SDL_TRUE;
	} else {
		SDL_SetError("application not compiled with SDL %d.%d",
			SDL_MAJOR_VERSION, SDL_MINOR_VERSION);
		return SDL_FALSE;
	}

	/* Failed to get window manager information */
	return SDL_FALSE;
}

/*****************************************************************************/
/* SDL OpenGL/OpenGL ES functions                                            */
/*****************************************************************************/
int
DREAM_gl_loadlibrary(_THIS, const char *path)
{
	int ret;
	SDL_VideoData *phdata = (SDL_VideoData *) _this->driverdata;
	
	/* Check if OpenGL ES library is specified for GF driver */
	if (path == NULL) {
		path = SDL_getenv("SDL_OPENGL_LIBRARY");
		if (path == NULL) {
			path = SDL_getenv("SDL_OPENGLES_LIBRARY");
		}
	}

	/* Check if default library loading requested */
	if (path == NULL) {
		/* Already linked with GF library which provides egl* subset of	*/
		/* functions, use Common profile of OpenGL ES library by default */
		path = "/usr/lib/libGLESv2.so";
	}
	
	ret = SDL_EGL_LoadLibrary(_this, path, (NativeDisplayType) phdata->egl_display);
	
#ifdef DREAMBOX_DEBUG
	fprintf(stderr, "DREAM: SDL_EGL_LoadLibrary ret=%d\n",ret);
#endif
	
	DREAM_PumpEvents(_this);
	
	return ret;
}

void *
DREAM_gl_getprocaddres(_THIS, const char *proc)
{
#ifdef DREAMBOX_DEBUG
	fprintf(stderr, "DREAM: gl_getprocaddres %s\n",proc);
#endif
	return NULL;
}

void
DREAM_gl_unloadlibrary(_THIS)
{
}

SDL_GLContext
DREAM_gl_createcontext(_THIS, SDL_Window * window)
{
	SDL_VideoData *phdata = (SDL_VideoData *) _this->driverdata;
	SDL_WindowData *wdata = (SDL_WindowData *) window->driverdata;
	EGLBoolean status;
	EGLint configs;
	uint32_t attr_pos;
	EGLint attr_value;
	EGLint cit;
	
#ifdef DREAMBOX_DEBUG
	fprintf(stderr, "DREAM: createcontext\n");
#endif

	/* Check if EGL was initialized */
	if (phdata->egl_initialized != SDL_TRUE) {
		SDL_SetError("DREAM: EGL initialization failed, no OpenGL ES support");
		return NULL;
	}

	/* Prepare attributes list to pass them to OpenGL ES */
	attr_pos = 0;
	wdata->gles_attributes[attr_pos++] = EGL_SURFACE_TYPE;
	wdata->gles_attributes[attr_pos++] = EGL_WINDOW_BIT;
	wdata->gles_attributes[attr_pos++] = EGL_RED_SIZE;
	wdata->gles_attributes[attr_pos++] = _this->gl_config.red_size;
	wdata->gles_attributes[attr_pos++] = EGL_GREEN_SIZE;
	wdata->gles_attributes[attr_pos++] = _this->gl_config.green_size;
	wdata->gles_attributes[attr_pos++] = EGL_BLUE_SIZE;
	wdata->gles_attributes[attr_pos++] = _this->gl_config.blue_size;
	wdata->gles_attributes[attr_pos++] = EGL_ALPHA_SIZE;

	/* Setup alpha size in bits */
	if (_this->gl_config.alpha_size) {
		wdata->gles_attributes[attr_pos++] = _this->gl_config.alpha_size;
	} else {
		wdata->gles_attributes[attr_pos++] = EGL_DONT_CARE;
	}

	/* Setup color buffer size */
	if (_this->gl_config.buffer_size) {
		wdata->gles_attributes[attr_pos++] = EGL_BUFFER_SIZE;
		wdata->gles_attributes[attr_pos++] = _this->gl_config.buffer_size;
	} else {
		wdata->gles_attributes[attr_pos++] = EGL_BUFFER_SIZE;
		wdata->gles_attributes[attr_pos++] = EGL_DONT_CARE;
	}

	/* Setup depth buffer bits */
	wdata->gles_attributes[attr_pos++] = EGL_DEPTH_SIZE;
	wdata->gles_attributes[attr_pos++] = _this->gl_config.depth_size;

	/* Setup stencil bits */
	if (_this->gl_config.stencil_size) {
		wdata->gles_attributes[attr_pos++] = EGL_STENCIL_SIZE;
		wdata->gles_attributes[attr_pos++] = _this->gl_config.buffer_size;
	} else {
		wdata->gles_attributes[attr_pos++] = EGL_STENCIL_SIZE;
		wdata->gles_attributes[attr_pos++] = EGL_DONT_CARE;
	}

	/* Set number of samples in multisampling */
	if (_this->gl_config.multisamplesamples) {
		wdata->gles_attributes[attr_pos++] = EGL_SAMPLES;
		wdata->gles_attributes[attr_pos++] =
			_this->gl_config.multisamplesamples;
	}

	/* Multisample buffers, OpenGL ES 1.0 spec defines 0 or 1 buffer */
	if (_this->gl_config.multisamplebuffers) {
		wdata->gles_attributes[attr_pos++] = EGL_SAMPLE_BUFFERS;
		wdata->gles_attributes[attr_pos++] =
			_this->gl_config.multisamplebuffers;
	}

	/* Finish attributes list */
	wdata->gles_attributes[attr_pos] = EGL_NONE;

	/* Request first suitable framebuffer configuration */
	status = eglChooseConfig(phdata->egl_display, wdata->gles_attributes,
							 wdata->gles_configs, 1, &configs);
	if (status != EGL_TRUE) {
		SDL_SetError("DREAM: Can't find closest configuration for OpenGL ES");
		return NULL;
	}

	/* Check if nothing has been found, try "don't care" settings */
	if (configs == 0) {
		int32_t it;
		int32_t jt;
		GLint depthbits[4] = { 32, 24, 16, EGL_DONT_CARE };

		for (it = 0; it < 4; it++) {
			for (jt = 16; jt >= 0; jt--) {
				/* Don't care about color buffer bits, use what exist */
				/* Replace previous set data with EGL_DONT_CARE		 */
				attr_pos = 0;
				wdata->gles_attributes[attr_pos++] = EGL_SURFACE_TYPE;
				wdata->gles_attributes[attr_pos++] = EGL_WINDOW_BIT;
				wdata->gles_attributes[attr_pos++] = EGL_RED_SIZE;
				wdata->gles_attributes[attr_pos++] = EGL_DONT_CARE;
				wdata->gles_attributes[attr_pos++] = EGL_GREEN_SIZE;
				wdata->gles_attributes[attr_pos++] = EGL_DONT_CARE;
				wdata->gles_attributes[attr_pos++] = EGL_BLUE_SIZE;
				wdata->gles_attributes[attr_pos++] = EGL_DONT_CARE;
				wdata->gles_attributes[attr_pos++] = EGL_ALPHA_SIZE;
				wdata->gles_attributes[attr_pos++] = EGL_DONT_CARE;
				wdata->gles_attributes[attr_pos++] = EGL_BUFFER_SIZE;
				wdata->gles_attributes[attr_pos++] = EGL_DONT_CARE;

				/* Try to find requested or smallest depth */
				if (_this->gl_config.depth_size) {
					wdata->gles_attributes[attr_pos++] = EGL_DEPTH_SIZE;
					wdata->gles_attributes[attr_pos++] = depthbits[it];
				} else {
					wdata->gles_attributes[attr_pos++] = EGL_DEPTH_SIZE;
					wdata->gles_attributes[attr_pos++] = EGL_DONT_CARE;
				}

				if (_this->gl_config.stencil_size) {
					wdata->gles_attributes[attr_pos++] = EGL_STENCIL_SIZE;
					wdata->gles_attributes[attr_pos++] = jt;
				} else {
					wdata->gles_attributes[attr_pos++] = EGL_STENCIL_SIZE;
					wdata->gles_attributes[attr_pos++] = EGL_DONT_CARE;
				}

				wdata->gles_attributes[attr_pos++] = EGL_SAMPLES;
				wdata->gles_attributes[attr_pos++] = EGL_DONT_CARE;
				wdata->gles_attributes[attr_pos++] = EGL_SAMPLE_BUFFERS;
				wdata->gles_attributes[attr_pos++] = EGL_DONT_CARE;
				wdata->gles_attributes[attr_pos] = EGL_NONE;

				/* Request first suitable framebuffer configuration */
				status =
					eglChooseConfig(phdata->egl_display,
									wdata->gles_attributes,
									wdata->gles_configs, 1, &configs);

				if (status != EGL_TRUE) {
					SDL_SetError
						("DREAM: Can't find closest configuration for OpenGL ES");
					return NULL;
				}
				if (configs != 0) {
					break;
				}
			}
			if (configs != 0) {
				break;
			}
		}

		/* No available configs */
		if (configs == 0) {
			SDL_SetError("DREAM: Can't find any configuration for OpenGL ES");
			return NULL;
		}
	}

	/* Initialize config index */
	wdata->gles_config = 0;

	/* Now check each configuration to find out the best */
	for (cit = 0; cit < configs; cit++) {
		uint32_t stencil_found;
		uint32_t depth_found;

		stencil_found = 0;
		depth_found = 0;

		if (_this->gl_config.stencil_size) {
			status =
				eglGetConfigAttrib(phdata->egl_display,
					wdata->gles_configs[cit], EGL_STENCIL_SIZE,
					&attr_value);
			if (status == EGL_TRUE) {
				if (attr_value != 0) {
					stencil_found = 1;
				}
			}
		} else {
			stencil_found = 1;
		}

		if (_this->gl_config.depth_size) {
			status =
				eglGetConfigAttrib(phdata->egl_display,
					wdata->gles_configs[cit], EGL_DEPTH_SIZE,
					&attr_value);
			if (status == EGL_TRUE) {
				if (attr_value != 0) {
					depth_found = 1;
				}
			}
		} else {
			depth_found = 1;
		}

		/* Exit from loop if found appropriate configuration */
		if ((depth_found != 0) && (stencil_found != 0)) {
			break;
		}
	}

	/* If best could not be found, use first */
	if (cit == configs) {
		cit = 0;
	}
	wdata->gles_config = cit;

	/* Create OpenGL ES context */
	wdata->gles_context =
		eglCreateContext(phdata->egl_display,
						 wdata->gles_configs[wdata->gles_config], NULL, NULL);
	if (wdata->gles_context == EGL_NO_CONTEXT) {
		SDL_SetError("DREAM: OpenGL ES context creation has been failed");
		return NULL;
	}

	wdata->gles_surface =
		eglCreateWindowSurface(phdata->egl_display,
			wdata->gles_configs[wdata->gles_config],
			(NativeWindowType) 0, NULL);


	if (wdata->gles_surface == 0) {
		SDL_SetError("Error : eglCreateWindowSurface failed;");
		return NULL;
	}

	/* Make just created context current */
	status =
		eglMakeCurrent(phdata->egl_display, wdata->gles_surface,
						 wdata->gles_surface, wdata->gles_context);
	if (status != EGL_TRUE) {
		/* Destroy OpenGL ES surface */
		eglDestroySurface(phdata->egl_display, wdata->gles_surface);
		eglDestroyContext(phdata->egl_display, wdata->gles_context);
		wdata->gles_context = EGL_NO_CONTEXT;
		SDL_SetError("DREAM: Can't set OpenGL ES context on creation");
		return NULL;
	}

	_this->gl_config.accelerated = 1;

	/* Always clear stereo enable, since OpenGL ES do not supports stereo */
	_this->gl_config.stereo = 0;

	/* Get back samples and samplebuffers configurations. Rest framebuffer */
	/* parameters could be obtained through the OpenGL ES API				*/
	status =
		eglGetConfigAttrib(phdata->egl_display,
			wdata->gles_configs[wdata->gles_config],
			EGL_SAMPLES, &attr_value);
	if (status == EGL_TRUE) {
		_this->gl_config.multisamplesamples = attr_value;
	}
	status =
		eglGetConfigAttrib(phdata->egl_display,
			wdata->gles_configs[wdata->gles_config],
			EGL_SAMPLE_BUFFERS, &attr_value);
	if (status == EGL_TRUE) {
		_this->gl_config.multisamplebuffers = attr_value;
	}

	/* Get back stencil and depth buffer sizes */
	status =
		eglGetConfigAttrib(phdata->egl_display,
			wdata->gles_configs[wdata->gles_config],
			EGL_DEPTH_SIZE, &attr_value);
	if (status == EGL_TRUE) {
		_this->gl_config.depth_size = attr_value;
	}
	status =
		eglGetConfigAttrib(phdata->egl_display,
			wdata->gles_configs[wdata->gles_config],
			EGL_STENCIL_SIZE, &attr_value);
	if (status == EGL_TRUE) {
		_this->gl_config.stencil_size = attr_value;
	}

	/* Under DREAM OpenGL ES output can't be double buffered */
	_this->gl_config.double_buffer = 0;

	/* GL ES context was successfully created */
	return wdata->gles_context;
}

int
DREAM_gl_makecurrent(_THIS, SDL_Window * window, SDL_GLContext context)
{
	SDL_VideoData *phdata = (SDL_VideoData *) _this->driverdata;
	SDL_WindowData *wdata;
	EGLBoolean status;
	
#ifdef DREAMBOX_DEBUG
	fprintf(stderr, "DREAM: makecurrent\n");
#endif
	
	if (phdata->egl_initialized != SDL_TRUE) {
		return SDL_SetError("DREAM: GF initialization failed, no OpenGL ES support");
	}

	if ((window == NULL) && (context == NULL)) {
		status =
			eglMakeCurrent(phdata->egl_display, EGL_NO_SURFACE,
							 EGL_NO_SURFACE, EGL_NO_CONTEXT);
		if (status != EGL_TRUE) {
			/* Failed to set current GL ES context */
			return SDL_SetError("DREAM: Can't set OpenGL ES context");
		}
	} else {
		wdata = (SDL_WindowData *) window->driverdata;
		if (wdata->gles_surface == EGL_NO_SURFACE) {
			return SDL_SetError
				("DREAM: OpenGL ES surface is not initialized for this window");
		}
		if (wdata->gles_context == EGL_NO_CONTEXT) {
			return SDL_SetError
				("DREAM: OpenGL ES context is not initialized for this window");
		}
		if (wdata->gles_context != context) {
			return SDL_SetError
				("DREAM: OpenGL ES context is not belong to this window");
		}
		status =
			eglMakeCurrent(phdata->egl_display, wdata->gles_surface,
							 wdata->gles_surface, wdata->gles_context);
		if (status != EGL_TRUE) {
			/* Failed to set current GL ES context */
			return SDL_SetError("DREAM: Can't set OpenGL ES context");
		}
	}
	return 0;
}

int
DREAM_gl_setswapinterval(_THIS, int interval)
{
	SDL_VideoData *phdata = (SDL_VideoData *) _this->driverdata;
	EGLBoolean status;
	
#ifdef DREAMBOX_DEBUG
	fprintf(stderr, "DREAM: setswapinterval\n");
#endif
	
	if (phdata->egl_initialized != SDL_TRUE) {
		return SDL_SetError("DREAM: EGL initialization failed, no OpenGL ES support");
	}

	/* Check if OpenGL ES connection has been initialized */
	if (phdata->egl_display != EGL_NO_DISPLAY) {
		/* Set swap OpenGL ES interval */
		status = eglSwapInterval(phdata->egl_display, interval);
		if (status == EGL_TRUE) {
			/* Return success to upper level */
			phdata->swapinterval = interval;
			return 0;
		}
	}

	/* Failed to set swap interval */
	return SDL_SetError("DREAM: Cannot set swap interval");
}

int
DREAM_gl_getswapinterval(_THIS)
{
#ifdef DREAMBOX_DEBUG
	fprintf(stderr, "DREAM: getswapinterval\n");
#endif
	return ((SDL_VideoData *) _this->driverdata)->swapinterval;
}

int
DREAM_gl_swapwindow(_THIS, SDL_Window * window)
{
	SDL_VideoData *phdata = (SDL_VideoData *) _this->driverdata;
	SDL_WindowData *wdata = (SDL_WindowData *) window->driverdata;
	
#ifdef DREAMBOX_DEBUG
	fprintf(stderr, "DREAM: swapwindow\n");
#endif
	
	if (phdata->egl_initialized != SDL_TRUE) {
		return SDL_SetError("DREAM: GLES initialization failed, no OpenGL ES support");
	}

	/* Many applications do not uses glFinish(), so we call it for them */
	glFinish();

	/* Wait until OpenGL ES rendering is completed */
	eglWaitGL();
	
	DREAM_waitforsync();
	
	eglSwapBuffers(phdata->egl_display, wdata->gles_surface);
	return 0;
}

void
DREAM_gl_deletecontext(_THIS, SDL_GLContext context)
{
	SDL_VideoData *phdata = (SDL_VideoData *) _this->driverdata;
	EGLBoolean status;
	
#ifdef DREAMBOX_DEBUG
	fprintf(stderr, "DREAM: deletecontext\n");
#endif
	
	if (phdata->egl_initialized != SDL_TRUE) {
		SDL_SetError("DREAM: GLES initialization failed, no OpenGL ES support");
		return;
	}

	/* Check if OpenGL ES connection has been initialized */
	if (phdata->egl_display != EGL_NO_DISPLAY) {
		if (context != EGL_NO_CONTEXT) {
			status = eglDestroyContext(phdata->egl_display, context);
			if (status != EGL_TRUE) {
				/* Error during OpenGL ES context destroying */
				SDL_SetError("DREAM: OpenGL ES context destroy error");
				return;
			}
		}
	}

	return;
}

#endif /* SDL_VIDEO_DRIVER_DREAMBOX */

/* vi: set ts=4 sw=4 expandtab: */
