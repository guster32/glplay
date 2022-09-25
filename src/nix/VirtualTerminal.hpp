/*
 * This file implements handling of DRM device nodes as well as setting
 * up the VT/TTY layer in order to let us do graphics in the first place.
 *
 * As it currently only uses direct device access, it needs to be run
 * as root.
 *
 * The preferred alternative on modern systems is to use systemd's logind,
 * a D-Bus API which allows us access to privileged devices such as DRM
 * and input devices without being root. It also handles resetting the TTY
 * for us if we screw up and crash, which is really excellent.
 *
 * An example of using logind is available from Weston. It uses the raw libdbus
 * API which can be pretty painful; GDBus is much more pleasant:
 * https://gitlab.freedesktop.org/wayland/weston/blob/master/libweston/launcher-logind.c
 *
 * VT switching is currently unimplemented. launcher-direct.c from Weston is
 * a decent reference of how to handle it, however it also involves handling
 * raw signals.
 */

/*
 * Copyright © 2018-2019 Collabora, Ltd.
 * Copyright © 2018-2019 DAQRI, LLC and its affiliates
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Author: Daniel Stone <daniels@collabora.com>
 */

#include <cassert>
#include <cerrno>
#include <fcntl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <linux/kd.h>
#include <linux/major.h>
#include <linux/vt.h>

#include "log.hpp"


/*
 * Set up the VT/TTY so it runs in graphics mode and lets us handle our own
 * input. This uses the VT specified in $TTYNO if specified, or the current VT
 * if we're running directly from a VT, or failing that tries to find a free
 * one to switch to.
 */
static int vt_setup(struct device *device)
{
	const char *tty_num_env = getenv("TTYNO");
	int tty_num = 0;
	int cur_vt;
	char tty_dev[32];

	/* If $TTYNO is set in the environment, then use that first. */
	if (tty_num_env) {
		char *endptr = NULL;

		tty_num = strtoul(tty_num_env, &endptr, 10);
		if (tty_num == 0 || *endptr != '\0') {
			fprintf(stderr, "invalid $TTYNO environment variable\n");
			return -1;
		}
		snprintf(tty_dev, sizeof(tty_dev), "/dev/tty%d", tty_num);
	} else if (ttyname(STDIN_FILENO)) {
		/* Otherwise, if we're running from a VT ourselves, just reuse
		 * that. */
		ttyname_r(STDIN_FILENO, tty_dev, sizeof(tty_dev));
	} else {
		int tty0;

		/* Other-other-wise, look for a free VT we can use by querying
		 * /dev/tty0. */
		tty0 = open("/dev/tty0", O_WRONLY | O_CLOEXEC);
		if (tty0 < 0) {
			fprintf(stderr, "couldn't open /dev/tty0\n");
			return -errno;
		}

		if (ioctl(tty0, VT_OPENQRY, &tty_num) < 0 || tty_num < 0) {
			fprintf(stderr, "couldn't get free TTY\n");
			close(tty0);
			return -errno;
		}
		close(tty0);
		sprintf(tty_dev, "/dev/tty%d", tty_num);
	}

	device->vt_fd = open(tty_dev, O_RDWR | O_NOCTTY);
	if (device->vt_fd < 0) {
		fprintf(stderr, "failed to open VT %d\n", tty_num);
		return -errno;
	}

	/* If we get our VT from stdin, work painfully backwards to find its
	 * VT number. */
	if (tty_num == 0) {
		struct stat buf;

		if (fstat(device->vt_fd, &buf) == -1 ||
		    major(buf.st_rdev) != TTY_MAJOR) {
			fprintf(stderr, "VT file %s is bad\n", tty_dev);
			return -1;
		}

		tty_num = minor(buf.st_rdev);
	}
	assert(tty_num != 0);

	printf("using VT %d\n", tty_num);

	/* Switch to the target VT. */
	if (ioctl(device->vt_fd, VT_ACTIVATE, tty_num) != 0 ||
	    ioctl(device->vt_fd, VT_WAITACTIVE, tty_num) != 0) {
		fprintf(stderr, "couldn't switch to VT %d\n", tty_num);
		return -errno;
	}

	debug("switched to VT %d\n", tty_num);

	/* Completely disable kernel keyboard processing: this prevents us
	 * from being killed on Ctrl-C. */
	if (ioctl(device->vt_fd, KDGKBMODE, &device->saved_kb_mode) != 0 ||
	    ioctl(device->vt_fd, KDSKBMODE, K_OFF) != 0) {
		fprintf(stderr, "failed to disable TTY keyboard processing\n");
		return -errno;
	}

	/* Change the VT into graphics mode, so the kernel no longer prints
	 * text out on top of us. */
	if (ioctl(device->vt_fd, KDSETMODE, KD_GRAPHICS) != 0) {
		fprintf(stderr, "failed to switch TTY to graphics mode\n");
		return -errno;
	}

	debug("VT setup complete\n");

	/* Normally we would also call VT_SETMODE to change the mode to
	 * VT_PROCESS here, which would allow us to intercept VT-switching
	 * requests and tear down KMS. But we don't, since that requires
	 * signal handling. */
	return 0;
}

static void vt_reset(struct device *device)
{
	ioctl(device->vt_fd, KDSKBMODE, device->saved_kb_mode);
	ioctl(device->vt_fd, KDSETMODE, KD_TEXT);
}