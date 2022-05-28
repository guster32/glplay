// gcc -o drmgl Linux_DRM_OpenGLES.c `pkg-config --cflags --libs libdrm` -lgbm -lEGL -lGLESv2

/*
 * Copyright (c) 2012 Arvin Schnell <arvin.schnell@gmail.com>
 * Copyright (c) 2012 Rob Clark <rob@ti.com>
 * Copyright (c) 2017 Miouyouyou <Myy> <myy@miouyouyou.fr>
 * Copyright (c) 2022 Gustavo Branco <guster32> <guster32@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/* Based on a egl cube test app originally written by Arvin Schnell */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <array>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>

#define GL_GLEXT_PROTOTYPES 1
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <cassert>

#include "DisplayAdapter.hpp"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

static struct {
	EGLDisplay display;
	EGLConfig config;
	EGLContext context;
	EGLSurface surface;
	GLuint program;
	GLint modelviewmatrix, modelviewprojectionmatrix, normalmatrix;
	GLuint vbo;
	GLuint positionsoffset, colorsoffset, normalsoffset;
} gl;

struct drm_fb {
	struct gbm_bo *bo;
	DisplayAdapter *adapter;
	uint32_t fb_id;
};

static auto init_gl(DisplayAdapter &adapter) -> int {
	EGLint major = 0;
	EGLint minor = 0;
	EGLint n = 0;
	GLuint vertex_shader = 0;
	GLuint fragment_shader = 0;
	GLint ret = 0;

	static const std::array<EGLint, 3> context_attribs = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

	static const std::array<EGLint, 17> config_attribs = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 0,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_DEPTH_SIZE, EGL_DONT_CARE,
    EGL_STENCIL_SIZE, EGL_DONT_CARE,
		EGL_NONE
	};

	/// TODO: We can also get a wayland eglDisplay here for window.
	PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform_display = nullptr;
	get_platform_display =
		reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
	assert(get_platform_display != nullptr);

	gl.display = get_platform_display(EGL_PLATFORM_GBM_KHR, adapter.buffer(), nullptr);

	if (eglInitialize(gl.display, &major, &minor) == 0U) {
		printf("failed to initialize\n");
		return -1;
	}

	printf("Using display %p with EGL version %d.%d\n",
			gl.display, major, minor);

	printf("EGL Version \"%s\"\n", eglQueryString(gl.display, EGL_VERSION));
	printf("EGL Vendor \"%s\"\n", eglQueryString(gl.display, EGL_VENDOR));
	printf("EGL Extensions \"%s\"\n", eglQueryString(gl.display, EGL_EXTENSIONS));

	if (eglBindAPI(EGL_OPENGL_ES_API) == 0U) {
		printf("failed to bind api EGL_OPENGL_ES_API\n");
		return -1;
	}

	EGLint num_configs = 0;
	if (eglGetConfigs(gl.display, nullptr, 0, &num_configs) == 0U) {
		printf("failed to get configs\n");
		return -1;
	}

	EGLConfig *configs = (EGLConfig *)malloc(num_configs * sizeof(EGLConfig));
	if (eglChooseConfig(gl.display, config_attribs.data(), configs, num_configs, &n) == 0U) {
		printf("failed to choose config: %d\n", n);
		return -1;
	}

	for (int i = 0; i < n; ++i) {
		EGLint gbm_format = 0;

		if (eglGetConfigAttrib(gl.display, configs[i],EGL_NATIVE_VISUAL_ID, &gbm_format) == 0U) {
			printf("failed to choose config: %d\n", i);
			return -1;
		}

		if (gbm_format == GBM_FORMAT_XRGB8888) {
				gl.config = configs[i];
				free(configs);
				break;
		}
	}

	gl.context = eglCreateContext(gl.display, gl.config, EGL_NO_CONTEXT, context_attribs.data());
	if (gl.context == nullptr) {
		printf("failed to create context\n");
		return -1;
	}

	gl.surface = eglCreatePlatformWindowSurface(gl.display, gl.config, adapter.surface(), nullptr);
	if (gl.surface == EGL_NO_SURFACE) {
		printf("failed to create egl surface\n");
		return -1;
	}

	/* connect the context to the surface */
	eglMakeCurrent(gl.display, gl.surface, gl.surface, gl.context);

	printf("GL Extensions: \"%s\"\n", glGetString(GL_EXTENSIONS));

	return 0;
}

/* Draw code here */
static void draw() {
	glClear(GL_COLOR_BUFFER_BIT);
	glClearColor(0.2f, 0.3f, 0.5f, 1.0f);
}

static void drm_fb_destroy_callback(struct gbm_bo *bo, void *data) {
	auto *fb = static_cast<struct drm_fb *>(data);
	struct gbm_device *gbm = gbm_bo_get_device(bo);

	if (fb->fb_id) {
		drmModeRmFB(fb->adapter->fileDescriptor(), fb->fb_id);
	}

	free(fb);
}

static auto drm_fb_get_from_bo(DisplayAdapter &adapter, struct gbm_bo *bo) -> struct drm_fb * {
	auto *fb = static_cast<struct drm_fb *>(gbm_bo_get_user_data(bo));
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t stride = 0;
	uint32_t handle = 0;
	int ret = 0;

	if (fb) {
		return fb;
	}

	fb = static_cast<struct drm_fb *>(calloc(1, sizeof *fb));
	fb->bo = bo;
	fb->adapter = &adapter;

	width = gbm_bo_get_width(bo);
	height = gbm_bo_get_height(bo);
	stride = gbm_bo_get_stride(bo);
	handle = gbm_bo_get_handle(bo).u32;

	ret = drmModeAddFB(adapter.fileDescriptor(), width, height, 24, 32, stride, handle, &fb->fb_id);
	if (ret != 0) {
		printf("failed to create fb: %s\n", strerror(errno));
		free(fb);
		return nullptr;
	}

	gbm_bo_set_user_data(bo, fb, drm_fb_destroy_callback);

	return fb;
}

static void page_flip_handler(int fd, unsigned int frame, unsigned int sec, unsigned int usec, void *data) {
	int *waiting_for_flip = static_cast<int *>(data);
	*waiting_for_flip = 0;
}

// struct flip_context{
// 	int fb_id[2];
// 	int current_fb_id;
// 	int crtc_id;
// 	struct timeval start;
// 	int swap_count;
// };
// static void page_flip_handler(int fd, unsigned int frame,
// 		  unsigned int sec, unsigned int usec, void *data)
// {
// 	struct flip_context *context;
// 	unsigned int new_fb_id;
// 	struct timeval end;
// 	double t;

// 	context = data;
// 	if (context->current_fb_id == context->fb_id[0])
// 		new_fb_id = context->fb_id[1];
// 	else
// 		new_fb_id = context->fb_id[0];
			
// 	drmModePageFlip(fd, context->crtc_id, new_fb_id,
// 			DRM_MODE_PAGE_FLIP_EVENT, context);
// 	context->current_fb_id = new_fb_id;
// 	context->swap_count++;
// 	if (context->swap_count == 60) {
// 		gettimeofday(&end, NULL);
// 		t = end.tv_sec + end.tv_usec * 1e-6 -
// 			(context->start.tv_sec + context->start.tv_usec * 1e-6);
// 		fprintf(stderr, "freq: %.02fHz\n", context->swap_count / t);
// 		context->swap_count = 0;
// 		context->start = end;
// 	}
// } 

auto main(int argc, char *argv[]) -> int {
	fd_set fds;
	drmEventContext evctx = {
		.version = DRM_EVENT_CONTEXT_VERSION,
		.page_flip_handler = page_flip_handler,
	};
	struct gbm_bo *bo = nullptr;
	struct drm_fb *fb = nullptr;
	uint32_t idx = 0;
	int ret = 0;

	//auto devices = DisplayAdapter::getDevices();
	auto adapter = DisplayAdapter("/dev/dri/card0");

	FD_ZERO(&fds);
	FD_SET(0, &fds);
	FD_SET(adapter.fileDescriptor(), &fds);


	ret = init_gl(adapter);
	if (ret != 0) {
		printf("failed to initialize EGL\n");
		return ret;
	}

	/* clear the color buffer */
	glClearColor(0.5, 0.5, 0.5, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);
	eglSwapBuffers(gl.display, gl.surface);
	bo = gbm_surface_lock_front_buffer(adapter.surface());
	fb = drm_fb_get_from_bo(adapter, bo);

	/* set mode: */
	uint32_t connector_id = adapter.connector();
	ret = drmModeSetCrtc(adapter.fileDescriptor(), adapter.crtc(), fb->fb_id, 0, 0,
			&connector_id, 1, adapter.displayMode());
	if (ret != 0) {
		printf("failed to set mode: %s\n", strerror(errno));
		return ret;
	}

	while (true) {
		struct gbm_bo *next_bo = nullptr;
		int waiting_for_flip = 1;

		draw();

		eglSwapBuffers(gl.display, gl.surface);
		next_bo = gbm_surface_lock_front_buffer(adapter.surface());
		fb = drm_fb_get_from_bo(adapter, next_bo);

		/*
		 * Here you could also update drm plane layers if you want
		 * hw composition
		 */

		ret = drmModePageFlip(adapter.fileDescriptor(), adapter.crtc(), fb->fb_id,
				DRM_MODE_PAGE_FLIP_EVENT, &waiting_for_flip);
		if (ret != 0) {
			printf("failed to queue page flip: %s\n", strerror(errno));
			return -1;
		}

		while (waiting_for_flip != 0) {
			ret = select(adapter.fileDescriptor() + 1, &fds, NULL, NULL, NULL);
			if (ret < 0) {
				printf("select err: %s\n", strerror(errno));
				return ret;
			} else if (ret == 0) {
				printf("select timeout!\n");
				return -1;
			} else if (FD_ISSET(0, &fds)) {
				printf("user interrupted!\n");
				break;
			}
			drmHandleEvent(adapter.fileDescriptor(), &evctx);
		}

		/* release last buffer to render on again: */
		gbm_surface_release_buffer(adapter.surface(), bo);
		bo = next_bo;
	}

	return ret;
}

