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
#pragma once
#include <bits/types/FILE.h>
#include <cassert>
#include <cerrno>
#include <fcntl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <linux/kd.h>
#include <linux/major.h>
#include <linux/vt.h>
#include <array>

#include "FileDescriptor.hpp"
#include "log.hpp"


namespace glplay::nix {

	struct glplay_vt {
		FileDescriptor vt_fd;
		int tty_num;
	};

	inline auto static find_free_VT() -> glplay_vt {
		int tty_num = 0;
		std::array<char, 32> tty_dev{};
		/* if we're running from a VT ourselves, just reuse
		* Other-other-wise, look for a free VT we can use by querying /dev/tty0.
		*/
		if (ttyname(STDIN_FILENO) != nullptr) {
			ttyname_r(STDIN_FILENO, tty_dev.data(), tty_dev.size());
		} else {
			std::string tty0_str = "/dev/tty0";
			auto tty0 = FileDescriptor(tty0_str, O_WRONLY | O_CLOEXEC);
			if (tty0.fileDescriptor() < 0) {
				error("couldn't open /dev/tty0\n");
				throw std::runtime_error("Failed to open TTY");
			}

			if (ioctl(tty0.fileDescriptor(), VT_OPENQRY, &tty_num) < 0 || tty_num < 0) {
				error("couldn't get free TTY\n");
				throw std::runtime_error("Failed to free TTY");
			}
			sprintf(tty_dev.data(), "/dev/tty%d", tty_num);
		}


		auto test = std::string(tty_dev.data());
		auto vt_fd = FileDescriptor(test, O_RDWR | O_NOCTTY);
		if (vt_fd.fileDescriptor() < 0) {
			error("failed to open VT %d\n", tty_num);
			throw std::runtime_error("Failed to open VT");
		}

		/* If we get our VT from stdin, work painfully backwards to find its
		* VT number. */
		if (tty_num == 0) {
			struct stat buf{};

			if (fstat(vt_fd.fileDescriptor(), &buf) == -1 ||
					major(buf.st_rdev) != TTY_MAJOR) {
				error("VT file %s is bad\n", tty_dev.data());
				throw std::runtime_error("Bad VT file");
			}

			tty_num = minor(buf.st_rdev);
		}
		assert(tty_num != 0);
		debug("using VT %d\n", tty_num);
		return { vt_fd, tty_num };
	}

	inline auto get_active_vt(FileDescriptor &vt_fd) -> int {
		struct vt_stat vt_state{};
    if (ioctl(vt_fd.fileDescriptor(), VT_GETSTATE, &vt_state) != 0) {
        throw std::runtime_error("Failed to retrieve VT state");
    }
		debug("active VT is %d\n", vt_state.v_active);
    return vt_state.v_active;
	}

	inline auto activate_vt(FileDescriptor &vt_fd, int tty_num) -> void {
		if (ioctl(vt_fd.fileDescriptor(), VT_ACTIVATE, tty_num) != 0 ||
				ioctl(vt_fd.fileDescriptor(), VT_WAITACTIVE, tty_num) != 0) {
			error("couldn't switch to VT %d\n", 0);
			throw std::runtime_error("Failed to switch to target VT");
		}
		debug("switched to VT %d\n", tty_num);
	}

	inline auto disable_keyboard(FileDescriptor &vt_fd) -> int {
		/* Completely disable kernel keyboard processing: this prevents us
		* from being killed on Ctrl-C. */
		int saved_kb_mode = K_UNICODE;
		if (ioctl(vt_fd.fileDescriptor(), KDGKBMODE, &saved_kb_mode) != 0 ||
				ioctl(vt_fd.fileDescriptor(), KDSKBMODE, K_OFF) != 0) {
			throw std::runtime_error("failed to disable TTY keyboard processing");
		}
		debug("Keyboard mode disabled. Original mode was: %d\n", saved_kb_mode);
		return saved_kb_mode;
	}

	inline auto set_graphics(FileDescriptor &vt_fd) -> void {
		/* Change the VT into graphics mode, so the kernel no longer prints
		* text out on top of us.
		* Normally we would also call VT_SETMODE to change the mode to
		* VT_PROCESS here, which would allow us to intercept VT-switching
		* requests and tear down KMS. But we don't, since that requires
		* signal handling. */
		if (ioctl(vt_fd.fileDescriptor(), KDSETMODE, KD_GRAPHICS) != 0) {
			error("failed to switch TTY to graphics mode\n");
			throw std::runtime_error("Failed to switch to graphics mode");
		}

	}

	inline auto static set_text(FileDescriptor &vt_fd, int mode) ->void {
		ioctl(vt_fd.fileDescriptor(), KDSKBMODE, mode);
		ioctl(vt_fd.fileDescriptor(), KDSETMODE, KD_TEXT);
	}
}