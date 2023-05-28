#include "../kms/kms.hpp"
#include "../drm/drm.hpp"
#include "../gbm/gbm.hpp"
#include "../nix/nix.hpp"
#include <EGL/egl.h>
#include <algorithm>
#include <cstdio>
#include <iostream>
#include <signal.h>

#include "../kms/time.hpp"
#include <poll.h>
/* The dma-fence explicit fencing API was previously called sync-file. */
#include <linux/sync_file.h>

/* Allow the driver to drift half a millisecond every frame. */
const auto FRAME_TIMING_TOLERANCE = (NSEC_PER_SEC / 2000);
#define NUM_ANIM_FRAMES 240 /* how many frames before we wrap around */

/*
 * These two helpers operate on sync_file FDs, which contain dma-fences used
 * for explicit fencing. We get these fences from EGLSync and from KMS.
 */
static bool
linux_sync_file_is_valid(int fd)
{
	struct sync_file_info file_info;
	memset(&file_info, 0, sizeof(file_info));

	if (ioctl(fd, SYNC_IOC_FILE_INFO, &file_info) < 0)
			return false;

	return file_info.num_fences > 0;
}


static uint64_t
linux_sync_file_get_fence_time(int fd)
{
	struct sync_file_info file_info;
	struct sync_fence_info fence_info;
	int ret;

	memset(&file_info, 0, sizeof(file_info));
	memset(&fence_info, 0, sizeof(fence_info));
	file_info.sync_fence_info = (uint64_t) (uintptr_t) &fence_info;
	file_info.num_fences = 1;

	/*
	 * One of the ways this ioctl can fail is if there is insufficient
	 * storage for the number of fences; a sync_file FD can hold multiple
	 * individual fences which are all merged together.
	 *
	 * This is fine for us since we only use single fences, but if you use
	 * merged fences, you can query the number of fences by setting
	 * num_fences == 0 and calling this ioctl, which will return the number
	 * of fences in the num_fences parameter.
	 */
	ret = ioctl(fd, SYNC_IOC_FILE_INFO, &file_info);
	assert(ret == 0);

	return fence_info.timestamp_ns;
}


static void
fd_replace(int *target, int source)
{
	if (*target >= 0)
		close(*target);
	*target = source;
}

/*
 * Informs us that an atomic commit has completed for the given CRTC. This will
 * be called onence for each output (identified by the crtc_id) for each commit.
 * We will be given the user_data parameter we passed to drmModeAtomicCommit
 * (which for us is just the device struct), as well as the frame sequence
 * counter as well as the actual time that our commit became active in hardware.
 *
 * This time is usually close to the start of the vblank period of the previous
 * frame, but depends on the driver.
 *
 * If the driver declares DRM_CAP_TIMESTAMP_MONOTONIC in its capabilities,
 * these times will be given as CLOCK_MONOTONIC values. If not (e.g. VMware),
 * all bets are off.
 */
static void atomic_event_handler(int fd, 
	unsigned int sequence,
	unsigned int tv_sec,
	unsigned int tv_usec,
	unsigned int crtc_id,
	void *user_data)
{
	const auto *adapter = static_cast<const glplay::kms::DisplayAdapter*>(user_data);
	glplay::kms::Display *display = nullptr;
		struct timespec completion = {
		.tv_sec = tv_sec,
		.tv_nsec = (tv_usec * 1000),
	};
	int64_t delta_nsec;

	/* Find the output this event is delivered for. */
	for (auto disp : adapter->displays) {  
		if(disp.crtc->crtc_id == crtc_id) {
			display = &disp;
			break;
		}
	}

	if(!display) {
		debug("[CRTC:%u] received atomic completion for unknown CRTC",
		      crtc_id);
		return;
	}

	/*
	 * Compare the actual completion timestamp to what we had predicted it
	 * would be when we submitted it.
	 *
	 * This example simply screams into the logs if we hit a different
	 * time from what we had predicted. However, there are a number of
	 * things you could do to better deal with this: for example, if your
	 * frame is always late, you need to either start drawing earlier, or
	 * if that is not possible, halve your frame rate so you can draw
	 * steadily and predictably, if more slowly.
	 */
	delta_nsec = glplay::kms::timespec_sub_to_nsec(&completion, &display->next_frame);
	if (glplay::kms::timespec_to_nsec(&display->last_frame) != 0 &&
		llabs((long long) delta_nsec) > FRAME_TIMING_TOLERANCE) {
		debug("[%s] FRAME %" PRIi64 "ns %s: expected %" PRIu64 ", got %" PRIu64 "\n",
		      display->name.c_str(),
		      delta_nsec,
		      (delta_nsec < 0) ? "EARLY" : "LATE",
		      glplay::kms::timespec_to_nsec(&display->next_frame),
		      glplay::kms::timespec_to_nsec(&completion));
	} else {
		debug("[%s] completed at %" PRIu64 " (delta %" PRIi64 "ns)\n",
		      display->name.c_str(),
		      glplay::kms::timespec_to_nsec(&completion),
		      delta_nsec);
	}

	display->needs_repaint = true;
	display->last_frame = completion;

	/*
	 * buffer_pending is the buffer we've just committed; this event tells
	 * us that buffer_pending is now being displayed, which means that
	 * buffer_last is no longer being displayed and we can reuse it.
	 */
	assert(display->bufferPending);
	assert(display->bufferPending->in_use);

	if (display->explicitFencing && adapter->eglDevice.explicit_fencing) {
		/*
		 * Print the time that the KMS fence FD signaled, i.e. when the
		 * last commit completed. It should be the same time as passed
		 * to this event handler in the function arguments.
		 */
		if (display->bufferLast &&
		    display->bufferLast->kms_fence_fd >= 0) {
			assert(linux_sync_file_is_valid(display->bufferLast->kms_fence_fd));
			debug("\tKMS fence time: %" PRIu64 "ns\n",
			      linux_sync_file_get_fence_time(display->bufferLast->kms_fence_fd));
		}

		/*
		 * Print the time that the render fence FD signaled, i.e. when
		 * we finished writing to the buffer that we have now started
		 * displaying. It should be strictly before the KMS fence FD
		 * time.
		 */
		assert(linux_sync_file_is_valid(display->bufferPending->render_fence_fd));
		debug("\trender fence time: %" PRIu64 "ns\n",
		      linux_sync_file_get_fence_time(display->bufferPending->render_fence_fd));
	}

	if (display->bufferLast) {
		assert(display->bufferLast->in_use);
		debug("\treleasing buffer with FB ID %" PRIu32 "\n", display->bufferLast->fb_id);
		display->bufferLast->in_use = false;
		display->bufferLast = NULL;
	}
	display->bufferLast = display->bufferPending;
	display->bufferPending = NULL;

}

/*
 * Advance the output's frame counter, aiming to achieve linear animation
 * speed: if we miss a frame, try to catch up by dropping frames.
 */
static void advance_frame(glplay::kms::Display *display, struct timespec *now)
{
	struct timespec too_soon;

	/* For our first tick, we won't have predicted a time. */
	if (glplay::kms::timespec_to_nsec(&display->last_frame) == 0L)
		return;

	/*
	 * Starting from our last frame completion time, advance the predicted
	 * completion for our next frame by one frame's refresh time, until we
	 * have at least a 4ms margin in which to paint a new buffer and submit
	 * our frame to KMS.
	 *
	 * This will skip frames in the animation if necessary, so it is
	 * temporally correct.
	 */
	glplay::kms::timespec_add_msec(&too_soon, now, 4);
	display->next_frame = display->last_frame;

	while (glplay::kms::timespec_sub_to_nsec(&too_soon, &display->next_frame) >= 0) {
		glplay::kms::timespec_add_nsec(&display->next_frame, &display->next_frame,
				  display->refreshIntervalNsec);
		display->frame_num = (display->frame_num + 1) % NUM_ANIM_FRAMES;
	}
}

static struct glplay::kms::Buffer *find_free_buffer(struct glplay::kms::Display *display)
{
	for (auto &buffer : display->buffers) {  
		if (!buffer.in_use) {
			return &buffer;
		}
	}
	assert(0 && "could not find free buffer for output!");
}

static void fill_verts(GLfloat *verts, GLfloat *col, int frame_num, int loc)
{
	float factor = ((frame_num * 2.0) / (float) NUM_ANIM_FRAMES) - 1.0f;
	GLfloat top, bottom, left, right;

	assert(loc >= 0 && loc < 4);

	switch (loc) {
	case 0:
		col[0] = 0.0f;
		col[1] = 0.0f;
		col[2] = 0.0f;
		col[3] = 1.0f;
		top = -1.0f;
		left = -1.0f;
		bottom = factor;
		right = factor;
		break;
	case 1:
		col[0] = 1.0f;
		col[1] = 0.0f;
		col[2] = 0.0f;
		col[3] = 1.0f;
		top = -1.0f;
		left = factor;
		right = 1.0f;
		bottom = factor;
		break;
	case 2:
		col[0] = 0.0f;
		col[1] = 0.0f;
		col[2] = 1.0f;
		col[3] = 1.0f;
		top = factor;
		left = -1.0f;
		bottom = 1.0f;
		right = factor;
		break;
	case 3:
		col[0] = 1.0f;
		col[1] = 0.0f;
		col[2] = 1.0f;
		col[3] = 1.0f;
		top = factor;
		left = factor;
		bottom = 1.0f;
		right = 1.0f;
		break;
	}

	verts[0] = left;
	verts[1] = bottom;
	verts[2] = left;
	verts[3] = top;
	verts[4] = right;
	verts[5] = top;
	verts[6] = right;
	verts[7] = bottom;
}

inline auto buffer_egl_fill(glplay::kms::DisplayAdapter *adapter, glplay::kms::Display *display) -> glplay::kms::Buffer* {
    static PFNEGLCREATESYNCKHRPROC create_sync = NULL;
    static PFNEGLWAITSYNCKHRPROC wait_sync = NULL;
    static PFNEGLDESTROYSYNCKHRPROC destroy_sync = NULL;
    static PFNEGLDUPNATIVEFENCEFDANDROIDPROC dup_fence_fd = NULL;
    EGLSyncKHR sync;
    EGLBoolean ret;

	/*
	 * Find a free buffer, predict the time our next frame will be
	 * displayed, use this to derive a target position for our animation
	 * (such that it remains as linear as possible over time, even at
	 * the cost of dropping frames), render the content for that position.
	 */
	auto buffer = find_free_buffer(display);
	assert(buffer);

    ret = eglMakeCurrent(adapter->eglDevice.egl_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE,
            adapter->eglDevice.ctx);
    assert(ret);

    if (display->explicitFencing && adapter->eglDevice.explicit_fencing) {
      if (!create_sync) {
        create_sync = (PFNEGLCREATESYNCKHRPROC)
          eglGetProcAddress("eglCreateSyncKHR");
      }
      assert(create_sync);

      if (!wait_sync) {
        wait_sync = (PFNEGLWAITSYNCKHRPROC)
          eglGetProcAddress("eglWaitSyncKHR");
      }
      assert(wait_sync);

      if (!destroy_sync) {
        destroy_sync = (PFNEGLDESTROYSYNCKHRPROC)
          eglGetProcAddress("eglDestroySyncKHR");
      }
      assert(destroy_sync);

      if (!dup_fence_fd) {
        dup_fence_fd = (PFNEGLDUPNATIVEFENCEFDANDROIDPROC)
          eglGetProcAddress("eglDupNativeFenceFDANDROID");
      }
      assert(dup_fence_fd);

      /*
      * If this buffer was previously used by KMS, insert a sync
      * wait before we use it, to ensure that the GPU doesn't render
      * to the buffer whilst KMS is still using it.
      *
      * This isn't actually necessary with our current model, since we
      * have more buffers than we need, and we wait in software until
      * they've been released. But if you want to start rendering
      * ahead of time, this fence will protect us.
      */
      if (buffer->kms_fence_fd >= 0) {
        EGLint attribs[] = {
          EGL_SYNC_NATIVE_FENCE_FD_ANDROID, buffer->kms_fence_fd,
          EGL_NONE,
        };

        assert(linux_sync_file_is_valid(buffer->kms_fence_fd));
        sync = create_sync(adapter->eglDevice.egl_dpy,
              EGL_SYNC_NATIVE_FENCE_ANDROID,
              attribs);
		auto err = eglGetError();
        assert(sync);
        buffer->kms_fence_fd = -1;
        ret = wait_sync(adapter->eglDevice.egl_dpy, sync, 0);
        assert(ret);
        destroy_sync(adapter->eglDevice.egl_dpy, sync);
        sync = EGL_NO_SYNC_KHR;
      }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, buffer->gbm.fbo_id);
    glViewport(0, 0, buffer->width, buffer->height);

    for (unsigned int i = 0; i < 4; i++) {
      GLfloat col[4];
      GLfloat verts[8];
      GLuint err = glGetError();
      fill_verts(verts, col, display->frame_num, i);
      glBindBuffer(GL_ARRAY_BUFFER, adapter->eglDevice.vbo);
      /* glBufferSubData is most supported across GLES2 / Core profile,
      * Core profile / GLES3 might have better ways */
      glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(GLfloat) * 8, verts);
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      glBindVertexArray(adapter->eglDevice.vao);
      glUniform4f(adapter->eglDevice.col_uniform, col[0], col[1], col[2], col[3]);
      glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
      glBindVertexArray(0);
      err = glGetError();
      if (err != GL_NO_ERROR)
        debug("GL error state 0x%x\n", err);
    }

    /*
    * All our rendering has now been prepared. Create an EGLSyncKHR
    * object which we _will_ extract a native fence FD from, but not
    * yet.
    *
    * Since none of our commands have yet been flushed, we insert an
    * explicit flush before we pull the native fence FD.
    *
    * This flush also acts as our guarantee when using implicit fencing
    * that the rendering will actually be issued.
    */
    if (display->explicitFencing && adapter->eglDevice.explicit_fencing) {
      EGLint attribs[] = {
        EGL_SYNC_NATIVE_FENCE_FD_ANDROID, EGL_NO_NATIVE_FENCE_FD_ANDROID,
        EGL_NONE,
      };

      sync = create_sync(adapter->eglDevice.egl_dpy,
            EGL_SYNC_NATIVE_FENCE_ANDROID,
            attribs);
	  auto err = eglGetError();
	  debug("Error %d\n", err);
      assert(sync);
    }

    glFlush();

    /*
    * Now we've flushed, we can get the fence FD associated with our
    * rendering, which we can pass to KMS to wait for.
    */
    if (display->explicitFencing && adapter->eglDevice.explicit_fencing) {
      int fd = dup_fence_fd(adapter->eglDevice.egl_dpy, sync);
      assert(fd >= 0);
      assert(linux_sync_file_is_valid(fd));
      fd_replace(&buffer->render_fence_fd, fd);
      destroy_sync(adapter->eglDevice.egl_dpy, sync);
    }

	buffer->in_use = true;
	display->bufferPending = buffer;
	display->needs_repaint = false;
	return buffer;
}


/*
 * Using the CPU mapping, fill the buffer with a simple pixel-by-pixel
 * checkerboard; the boundaries advance from top-left to bottom-right.
 */
auto buffer_fill(glplay::kms::DisplayAdapter *adapter, glplay::kms::Display *display) -> glplay::kms::Buffer* {
	
	
	// if (buffer->gbm.bo) {
		// if (buffer->display->device->vk_device) {
			// TODO: handle return value
			// buffer_vk_fill(buffer, frame_num);
		// } else {
			auto buff = buffer_egl_fill(adapter, display);
		// }

		return buff;
	// }

	// for (unsigned int y = 0; y < buffer->height; y++) {
	// 	/*
	// 	 * We play silly games with pointer types so we advance
	// 	 * by (y*pitch) in bytes rather than in pixels, then cast back.
	// 	 */
	// 	uint8_t b;
	// 	uint32_t *pix =
	// 		(uint32_t *) ((uint8_t *) buffer->dumb.mem + (y * buffer->pitches[0]));
	// 	if (y >= (buffer->height * display->frame_num) / NUM_ANIM_FRAMES)
	// 		b = 0xff;
	// 	else
	// 		b = 0;

	// 	for (unsigned int x = 0; x < buffer->width; x++) {
	// 		uint32_t r;

	// 		if (x >= (buffer->width * display->frame_num) / NUM_ANIM_FRAMES)
	// 			r = 0xff;
	// 		else
	// 			r = 0;

	// 		*pix++ = (0xff << 24 /* A */) | (r << 16) | \
	// 			 (0x00 <<  8 /* G */) | b;
	// 	}
	// }
}

/* Sets a plane property inside an atomic request. */
static int
plane_add_prop(drmModeAtomicReq *req, glplay::kms::Display * display,
	       enum glplay::drm::wdrm_plane_property prop, uint64_t val)
{
	auto info = &display->props.plane[prop];
	int ret;

	if (info->prop_id == 0)
		return -1;

	ret = drmModeAtomicAddProperty(req, display->primary_plane->plane_id,
				       info->prop_id, val);
	debug("\t[PLANE:%lu] %lu (%s) -> %llu (0x%llx)\n",
	      (unsigned long) display->primary_plane->plane_id,
	      (unsigned long) info->prop_id, info->name,
	      (unsigned long long) val, (unsigned long long) val);
	return (ret <= 0) ? -1 : 0;
}


/* Sets a CRTC property inside an atomic request. */
static int
crtc_add_prop(drmModeAtomicReq *req, glplay::kms::Display * display,
	      enum glplay::drm::wdrm_crtc_property prop, uint64_t val)
{
	auto info = &display->props.crtc[prop];
	int ret;

	if (info->prop_id == 0)
		return -1;

	ret = drmModeAtomicAddProperty(req, display->crtc->crtc_id, info->prop_id,
				       val);
	debug("\t[CRTC:%lu] %lu (%s) -> %llu (0x%llx)\n",
	      (unsigned long) display->crtc->crtc_id,
	      (unsigned long) info->prop_id, info->name,
	      (unsigned long long) val, (unsigned long long) val);
	return (ret <= 0) ? -1 : 0;
}

/* Sets a connector property inside an atomic request. */
static int
connector_add_prop(drmModeAtomicReq *req, glplay::kms::Display * display,
		   enum glplay::drm::wdrm_connector_property prop, uint64_t val)
{
	auto info = &display->props.crtc[prop];
	int ret;

	if (info->prop_id == 0)
		return -1;

	ret = drmModeAtomicAddProperty(req, display->connector->connector_id,
				       info->prop_id, val);
	debug("\t[CONN:%lu] %lu (%s) -> %llu (0x%llx)\n",
	      (unsigned long) display->connector->connector_id,
	      (unsigned long) info->prop_id, info->name,
	      (unsigned long long) val, (unsigned long long) val);
	return (ret <= 0) ? -1 : 0;
}

/*
 * Populates an atomic request structure with this output's current
 * configuration.
 *
 * Atomic requests are applied incrementally on top of the current state, so
 * there is no need here to apply the entire output state, except on the first
 * modeset if we are changing the display routing (per output_create comments).
 */
void output_add_atomic_req(glplay::kms::Display * display, drmModeAtomicReqPtr req,
			   glplay::kms::Buffer *buffer)
{
	int ret;

	debug("[%s] atomic state for commit:\n", display->name.c_str());

	ret = plane_add_prop(req, display, glplay::drm::WDRM_PLANE_CRTC_ID, display->crtc->crtc_id);

	/*
	 * SRC_X/Y/W/H are the co-ordinates to use as the dimensions of the
	 * framebuffer source: you can use these to crop an image. Source
	 * co-ordinates are in 16.16 fixed-point to allow for better scaling;
	 * as we just use a full-size uncropped image, we don't need this.
	 */
	ret |= plane_add_prop(req, display, glplay::drm::WDRM_PLANE_FB_ID, buffer->fb_id);
	//TODO: need adapter here to replace false.
	if (display->explicitFencing && false && buffer->render_fence_fd >= 0) {
		assert(linux_sync_file_is_valid(buffer->render_fence_fd));
		ret |= plane_add_prop(req, display, glplay::drm::WDRM_PLANE_IN_FENCE_FD,
				      buffer->render_fence_fd);
	}
	ret |= plane_add_prop(req, display, glplay::drm::WDRM_PLANE_SRC_X, 0);
	ret |= plane_add_prop(req, display, glplay::drm::WDRM_PLANE_SRC_Y, 0);
	ret |= plane_add_prop(req, display, glplay::drm::WDRM_PLANE_SRC_W,
			      buffer->width << 16);
	ret |= plane_add_prop(req, display, glplay::drm::WDRM_PLANE_SRC_H,
			      buffer->height << 16);

	/*
	 * DST_X/Y/W/H position the plane's display within the CRTC's display
	 * space; these positions are plain integer, as it makes no sense for
	 * display positions to be expressed in subpixels.
	 *
	 * Anyway, we just use a full-screen buffer with no scaling.
	 */
	ret |= plane_add_prop(req, display, glplay::drm::WDRM_PLANE_CRTC_X, 0);
	ret |= plane_add_prop(req, display, glplay::drm::WDRM_PLANE_CRTC_Y, 0);
	ret |= plane_add_prop(req, display, glplay::drm::WDRM_PLANE_CRTC_W, buffer->width);
	ret |= plane_add_prop(req, display, glplay::drm::WDRM_PLANE_CRTC_H, buffer->height);

	/* Ensure we do actually have a full-screen buffer. */
	assert(buffer->width == display->mode.hdisplay);
	assert(buffer->height == display->mode.vdisplay);

	/*
	 * Changing any of these three properties requires the ALLOW_MODESET
	 * flag to be set on the atomic commit.
	 */
	ret |= crtc_add_prop(req, display, glplay::drm::WDRM_CRTC_MODE_ID,
			     display->mode_blob_id);
	ret |= crtc_add_prop(req, display, glplay::drm::WDRM_CRTC_ACTIVE, 1);

	//TODO: add adapter to fix false.
	if (display->explicitFencing && false) {
		if (display->commitFenceFD >= 0)
			close(display->commitFenceFD);
		display->commitFenceFD = -1;

		/*
		 * OUT_FENCE_PTR takes a pointer as a value, which the kernel
		 * fills in at commit time. The fence signals when the commit
		 * completes, i.e. when the event we request is sent.
		 */
		ret |= crtc_add_prop(req, display, glplay::drm::WDRM_CRTC_OUT_FENCE_PTR,
				     (uint64_t) (uintptr_t) &display->commitFenceFD);
	}

	ret |= connector_add_prop(req, display, glplay::drm::WDRM_CONNECTOR_CRTC_ID,
				  display->crtc->crtc_id);

	assert(ret == 0);
}

static void repaint_one_output(glplay::kms::DisplayAdapter &adapter, glplay::kms::Display &display, drmModeAtomicReqPtr req, bool *needs_modeset)
{
	struct timespec now;
	int ret;

	ret = clock_gettime(CLOCK_MONOTONIC, &now);
	assert(ret == 0);

	advance_frame(&display, &now);
	auto buffer = buffer_fill(&adapter, &display);

	/* Add the output's new state to the atomic modesetting request. */
	output_add_atomic_req(&display, req, buffer);

	/*
	 * If this output hasn't been painted before, then we need to set
	 * ALLOW_MODESET so we can get our first buffer on screen; if we
	 * have already presented to this output, then we don't need to since
	 * our configuration is similar enough.
	 */
	if (glplay::kms::timespec_to_nsec(&display.last_frame) == 0UL) {
		*needs_modeset = true;
	}

	if (glplay::kms::timespec_to_nsec(&display.next_frame) != 0UL) {
		debug("[%s] predicting presentation at %" PRIu64 " (%" PRIu64 "ns / %" PRIu64 "ms away)\n",
		      display.name.c_str(),
		      glplay::kms::timespec_to_nsec(&display.next_frame),
		      glplay::kms::timespec_sub_to_nsec(&display.next_frame, &now),
		      glplay::kms::timespec_sub_to_msec(&display.next_frame, &now));
	} else {
		debug("[%s] scheduling first frame\n", display.name.c_str());
	}
}

/*
 * Commits the atomic state to KMS.
 *
 * Using the NONBLOCK + PAGE_FLIP_EVENT flags means that we will return
 * immediately; when the flip has actually been completed in hardware,
 * the KMS FD will become readable via select() or poll(), and we will
 * receive an event to be read and dispatched via drmHandleEvent().
 *
 * For atomic commits, this goes to the page_flip_handler2 vfunc we set
 * in our DRM event context passed to drmHandleEvent(), which will be
 * called once for each CRTC affected by this atomic commit; the last
 * parameter of drmModeAtomicCommit() is a user-data parameter which
 * will be passed to the handler.
 *
 * The ALLOW_MODESET flag should not be used in regular operation.
 * Commits which require potentially expensive operations: changing clocks,
 * per-block power toggles, or anything with a setup time which requires
 * a longer-than-usual wait. It is used when we are changing the routing
 * or modes; here we set it on our first commit (since the prior state
 * could be very different), but make sure to not use it in steady state.
 *
 * Another flag which can be used - but isn't here - is TEST_ONLY. This
 * flag simply checks whether or not the atomic commit _would_ succeed,
 * and returns without committing the state to the kernel. Weston uses
 * this to determine whether or not we can use overlays by brute force:
 * we try to place each view on a particular plane one by one, testing
 * whether or not it succeeds for each plane. TEST_ONLY commits are very
 * cheap, so can be used to iteratively determine a successful configuration,
 * as KMS itself does not describe the constraints a driver has, e.g.
 * certain planes can only scale by certain amounts.
 */
int atomic_commit(glplay::kms::DisplayAdapter *adapter, drmModeAtomicReqPtr req,
		  bool allow_modeset)
{
	int ret;
	uint32_t flags = (DRM_MODE_ATOMIC_NONBLOCK |
			  DRM_MODE_PAGE_FLIP_EVENT);

	if (allow_modeset)
		flags |= DRM_MODE_ATOMIC_ALLOW_MODESET;

	return drmModeAtomicCommit(adapter->getAdapterFD(), req, flags, adapter);
}

static bool shall_exit = false;

static void sighandler(int signo)
{
	if (signo == SIGINT)
		shall_exit = true;
	return;
}


auto main(int argc, char *argv[]) -> int {
	auto paths = glplay::drm::getDevicePaths();
	auto adapter = glplay::kms::DisplayAdapter(paths.at(0));
	//Create renderer here  vk_device_create or device_egl_setup or software
	
	auto vt_fd = glplay::nix::vt_setup();
	debug("finished initialization\n");
	while (!shall_exit) {
		drmModeAtomicReq *req;
		bool needs_modeset = false;
		int output_count = 0;
		int ret = 0;
		drmEventContext evctx = {
			.version = 3,
			.page_flip_handler2 = atomic_event_handler,
		};
		struct pollfd poll_fd = {
			.fd = adapter.getAdapterFD(),
			.events = POLLIN,
		};

		/*
		 * Allocate an atomic-modesetting request structure for any
		 * work we will do in this loop iteration.
		 *
		 * Atomic modesetting allows us to group together KMS requests
		 * for multiple outputs, so this request may contain more than
		 * one output's repaint data.
		 */
		req = drmModeAtomicAlloc();
		assert(req);

		/*
		 * See which of our outputs needs repainting, and repaint them
		 * if any.
		 *
		 * On our first run through the loop, all our outputs will
		 * need repainting, so the request will contain the state for
		 * all the outputs, submitted together.
		 *
		 * This is good since it gives the driver a complete overview
		 * of any hardware changes it would need to perform to reach
		 * the target state.
		 */
		for (auto &display : adapter.displays) {  
			if(display.needs_repaint) {
				/*
				 * Add this output's new state to the atomic
				 * request.
				 */
				repaint_one_output(adapter, display, req, &needs_modeset);
				output_count++;
			}
		}

				/*
		 * Committing the atomic request to KMS makes the configuration
		 * current. As we request non-blocking mode, this function will
		 * return immediately, and send us events through the DRM FD
		 * when the request has actually completed. Even if we group
		 * updates for multiple outputs into a single request, the
		 * kernel will send one completion event for each output.
		 *
		 * Hence, after the first repaint, each output effectively
		 * runs its own repaint loop. This allows us to work with
		 * outputs running at different frequencies, or running out of
		 * phase from each other, without dropping our frame rate to
		 * the lowest common denominator.
		 *
		 * It does mean that we need to allocate paint the buffers for
		 * each output individually, rather than having a single buffer
		 * with the content for every output.
		 */
		if (output_count != 0)
			ret = atomic_commit(&adapter, req, needs_modeset);
		drmModeAtomicFree(req);
		if (ret != 0) {
			error("atomic commit failed: %d\n", ret);
			break;
		}

		/*
		 * The out-fence FD from KMS signals when the commit we've just
		 * made becomes active, at the same time as the event handler
		 * will fire. We can use this to find when the _previous_
		 * buffer is free to reuse again.
		 */
		for (auto &display : adapter.displays) {  
			if (display.explicitFencing && adapter.eglDevice.explicit_fencing && display.bufferLast) {
				assert(linux_sync_file_is_valid(display.commitFenceFD));
				fd_replace(&display.bufferLast->kms_fence_fd,
					   display.commitFenceFD);
				display.commitFenceFD = -1;
			}
		}

		/*
		 * Now we have (maybe) repainted some outputs, we go to sleep
		 * waiting for completion events from KMS. As each output
		 * completes, we will receive one event per output (making
		 * the DRM FD be readable and waking us from poll), which we
		 * then dispatch through drmHandleEvent into our callback.
		 */
		ret = poll(&poll_fd, 1, -1);
		if (ret == -1) {
			error("error polling KMS FD: %d\n", ret);
			break;
		}

		ret = drmHandleEvent(adapter.getAdapterFD(), &evctx);
		if (ret == -1) {
			error("error reading KMS events: %d\n", ret);
			break;
		}
	}
	glplay::nix::vt_reset(vt_fd);
  
	return 0;
}