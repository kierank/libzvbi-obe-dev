/*
 *  libzvbi - V4L interface
 *
 *  Copyright (C) 1999, 2000, 2001, 2002 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

static char rcsid[] = "$Id: io-v4l.c,v 1.15 2003/05/24 12:17:41 tomzo Exp $";

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#include "vbi.h"
#include "io.h"

#ifdef ENABLE_V4L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <sys/time.h>		/* timeval */
#include <sys/types.h>		/* fd_set */
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <pthread.h>

#include "videodev.h"

/* same as ioctl(), but repeat if interrupted */
#define IOCTL(fd, cmd, data)						\
({ int __result; do __result = ioctl(fd, cmd, data);			\
   while (__result == -1L && errno == EINTR); __result; })

#define BTTV_VBISIZE		_IOR('v' , BASE_VIDIOCPRIVATE+8, int)

#undef REQUIRE_SELECT
#undef REQUIRE_SVBIFMT		/* else accept current parameters */
#undef REQUIRE_VIDEOSTD		/* if clueless, assume PAL/SECAM */

#define printv(format, args...)						\
do {									\
	if (v->do_trace) {							\
		fprintf(stderr, format ,##args);			\
		fflush(stderr);						\
	}								\
} while (0)

typedef struct vbi_capture_v4l {
	vbi_capture		capture;

	int			fd;
	vbi_bool		has_select;
	vbi_bool		read_active;
	vbi_bool		do_trace;
	signed char		has_s_fmt;
	struct video_capability vcap;
	char		      * p_dev_name;

	vbi_raw_decoder		dec;

	double			time_per_frame;

	vbi_capture_buffer	*raw_buffer;
	int			num_raw_buffers;

	vbi_capture_buffer	sliced_buffer;

} vbi_capture_v4l;


static void
v4l_read_stop(vbi_capture_v4l *v)
{
	for (; v->num_raw_buffers > 0; v->num_raw_buffers--) {
		free(v->raw_buffer[v->num_raw_buffers - 1].data);
		v->raw_buffer[v->num_raw_buffers - 1].data = NULL;
	}

	free(v->raw_buffer);
	v->raw_buffer = NULL;
}


static int
v4l_suspend(vbi_capture_v4l *v)
{
	int    fd;

	v4l_read_stop(v);

	if (v->read_active) {
		printv("Suspending read: re-open device...\n");

		/* hack: cannot suspend read to allow SVBIFMT, need to close device */
		fd = open(v->p_dev_name, O_RDWR);
		if (fd == -1) {
			printv("v4l2-suspend: failed to re-open VBI device: %d: %s\n", errno, strerror(errno));
			return -1;
		}

		/* use dup2() to keep the same fd, which may be used by our client */
		close(v->fd);
		dup2(fd, v->fd);
		close(fd);

		v->read_active = FALSE;
	}
	return 0;
}


static int
v4l_read_alloc(vbi_capture_v4l *v, char ** errorstr)
{
	assert(v->raw_buffer == NULL);

	v->raw_buffer = calloc(1, sizeof(v->raw_buffer[0]));

	if (v->raw_buffer == NULL) {
		vbi_asprintf(errorstr, _("Virtual memory exhausted."));
		errno = ENOMEM;
		goto failure;
	}

	v->raw_buffer[0].size = (v->dec.count[0] + v->dec.count[1])
				* v->dec.bytes_per_line;

	v->raw_buffer[0].data = malloc(v->raw_buffer[0].size);

	if (v->raw_buffer[0].data == NULL) {
		vbi_asprintf(errorstr, _("Not enough memory to allocate "
					 "vbi capture buffer (%d KB)."),
			     (v->raw_buffer[0].size + 1023) >> 10);
		goto failure;
	}

	v->num_raw_buffers = 1;

	printv("Capture buffer allocated\n");

	return 0;

failure:
	v4l_read_stop(v);
	return -1;
}


static int
v4l_read(vbi_capture *vc, vbi_capture_buffer **raw,
	 vbi_capture_buffer **sliced, struct timeval *timeout)
{
	vbi_capture_v4l *v = PARENT(vc, vbi_capture_v4l, capture);
	vbi_capture_buffer *my_raw = v->raw_buffer;
	struct timeval tv;
	int r;

	if (my_raw == NULL) {
		printv("read buffer not allocated (must add services first)\n");
		errno = EINVAL;
		return -1;
	}

	while (v->has_select) {
		fd_set fds;

		FD_ZERO(&fds);
		FD_SET(v->fd, &fds);

		tv = *timeout; /* Linux kernel overwrites this */

		r = select(v->fd + 1, &fds, NULL, NULL, &tv);

		if (r < 0 && errno == EINTR)
			continue;

		if (r <= 0)
			return r; /* timeout or error */

		break;
	}

	if (!raw)
		raw = &my_raw;
	if (!*raw)
		*raw = v->raw_buffer;
	else
		(*raw)->size = v->raw_buffer[0].size;

	v->read_active = TRUE;

	for (;;) {
		/* from zapping/libvbi/v4lx.c */
		pthread_testcancel();

		r = read(v->fd, (*raw)->data, (*raw)->size);

		if (r == -1 && (errno == EINTR || errno == ETIME))
			continue;

		if (r == (*raw)->size)
			break;
		else
			return -1;
	}

	gettimeofday(&tv, NULL);

	(*raw)->timestamp = tv.tv_sec + tv.tv_usec * (1 / 1e6);

	if (sliced) {
		int lines;

		if (*sliced) {
			lines = vbi_raw_decode(&v->dec, (*raw)->data,
					       (vbi_sliced *)(*sliced)->data);
		} else {
			*sliced = &v->sliced_buffer;
			lines = vbi_raw_decode(&v->dec, (*raw)->data,
					       (vbi_sliced *)(v->sliced_buffer.data));
		}

		(*sliced)->size = lines * sizeof(vbi_sliced);
		(*sliced)->timestamp = (*raw)->timestamp;
	}

	return 1;
}

static void v4l_flush(vbi_capture *vc)
{
	vbi_capture_v4l *v = PARENT(vc, vbi_capture_v4l, capture);
	struct timeval tv;
	int fd_flags = 0;
	int r;
	fd_set fds;

	while (v->has_select) {
		FD_ZERO(&fds);
		FD_SET(v->fd, &fds);

		memset(&tv, 0, sizeof(tv));

		r = select(v->fd + 1, &fds, NULL, NULL, &tv);

		if ((r < 0) && (errno == EINTR))
			continue;

		/* if no data is ready or an error occurred, return */
		if (r <= 0)
			return;

		break;
	}

	if (v->has_select == FALSE) {
		fd_flags = fcntl(v->fd, F_GETFL, NULL);
		if (fd_flags == -1)
			return;
		/* no select supported by driver -> make read non-blocking */
		if ((fd_flags & O_NONBLOCK) == 0) {
			fcntl(v->fd, F_SETFL, fd_flags | O_NONBLOCK);
		}
	}

	r = read(v->fd, v->raw_buffer->data, v->raw_buffer->size);

	if ((v->has_select == FALSE) && ((fd_flags & O_NONBLOCK) == 0)) {
		fcntl(v->fd, F_SETFL, fd_flags);
	}
}


/* Molto rumore per nulla. */

#include <dirent.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

static void
perm_check(vbi_capture_v4l *v, const char *name)
{
	struct stat st;
	int old_errno = errno;
	uid_t uid = geteuid();
	gid_t gid = getegid();

	if (stat(name, &st) == -1) {
		printv("stat %s failed: %d, %s\n", name, errno, strerror(errno));
		errno = old_errno;
		return;
	}

	printv("%s permissions: user=%d.%d mode=0%o, I am %d.%d\n",
		name, st.st_uid, st.st_gid, st.st_mode, uid, gid);

	errno = old_errno;
}

static vbi_bool
reverse_lookup(vbi_capture_v4l *v, int fd, struct stat *vbi_stat)
{
	struct video_capability vcap;
	struct video_unit vunit;

	if (IOCTL(fd, VIDIOCGCAP, &vcap) != 0) {
		printv("Driver doesn't support VIDIOCGCAP, probably not v4l\n");
		return FALSE;
	}

	if (!(vcap.type & VID_TYPE_CAPTURE)) {
		printv("Driver is no video capture device\n");
		return FALSE;
	}

	if (IOCTL(fd, VIDIOCGUNIT, &vunit) != 0) {
		printv("Driver doesn't support VIDIOCGUNIT\n");
		return FALSE;
	}

	if (vunit.vbi != minor(vbi_stat->st_rdev)) {
		printv("Driver reports vbi minor %d, need %d\n",
			vunit.vbi, minor(vbi_stat->st_rdev));
		return FALSE;
	}

	printv("Matched\n");
	return TRUE;
}

static vbi_bool
get_videostd(vbi_capture_v4l *v, int fd, int *mode)
{
	struct video_tuner vtuner;
	struct video_channel vchan;

	memset(&vtuner, 0, sizeof(vtuner));
	memset(&vchan, 0, sizeof(vchan));

	if (IOCTL(fd, VIDIOCGTUNER, &vtuner) != -1) {
		printv("Driver supports VIDIOCGTUNER: mode %d (0=PAL, 1=NTSC, 2=SECAM)\n", vtuner.mode);
		*mode = vtuner.mode;
		return TRUE;
	} else if (IOCTL(fd, VIDIOCGCHAN, &vchan) != -1) {
		printv("Driver supports VIDIOCGCHAN: norm %d\n", vchan.norm);
		*mode = vchan.norm;
		return TRUE;
	} else
		printv("Driver doesn't support VIDIOCGTUNER or VIDIOCGCHAN\n");

	return FALSE;
}

static vbi_bool
probe_video_device(vbi_capture_v4l *v, char *name, struct stat *vbi_stat,
		   int *mode)
{
	struct stat vid_stat;
	int fd;

	if (stat(name, &vid_stat) == -1) {
		printv("stat failed: %d, %s\n",	errno, strerror(errno));
		return FALSE;
	}

	if (!S_ISCHR(vid_stat.st_mode)) {
		printv("%s is no character special file\n", name);
		return FALSE;
	}

	if (major(vid_stat.st_rdev) != major(vbi_stat->st_rdev)) {
		printv("Mismatch of major device number: "
			"%s: %d, %d; vbi: %d, %d\n", name,
			major(vid_stat.st_rdev), minor(vid_stat.st_rdev),
			major(vbi_stat->st_rdev), minor(vbi_stat->st_rdev));
		return FALSE;
	}

	if (!(fd = open(name, O_RDONLY | O_TRUNC))) {
		printv("Cannot open %s: %d, %s\n", name, errno, strerror(errno));
		perm_check(v, name);
		return FALSE;
	}

	if (!reverse_lookup(v, fd, vbi_stat)
	    || !get_videostd(v, fd, mode)) {
		close(fd);
		return FALSE;
	}

	close(fd);

	return TRUE;
}

static vbi_bool
guess_bttv_v4l(vbi_capture_v4l *v, int *strict,
	       int given_fd, int scanning)
{
	static char *video_devices[] = {
		"/dev/video",
		"/dev/video0",
		"/dev/video1",
		"/dev/video2",
		"/dev/video3",
	};
	struct dirent dirent, *pdirent = &dirent;
	struct stat vbi_stat;
	DIR *dir;
	int mode = -1;
	unsigned int i;

	if (scanning) {
		v->dec.scanning = scanning;
		return TRUE;
	}

	printv("Attempt to guess the videostandard\n");

	if (get_videostd(v, v->fd, &mode))
		goto finish;

	/*
	 *  Bttv v4l has no VIDIOCGUNIT pointing back to
	 *  the associated video device, now it's getting
	 *  dirty. We'll walk /dev, first level of, and
	 *  assume v4l major is still 81. Not tested with devfs.
	 */
	printv("Attempt to find a reverse VIDIOCGUNIT\n");

	if (fstat(v->fd, &vbi_stat) == -1) {
		printv("fstat failed: %d, %s\n", errno, strerror(errno));
		goto finish;
	}

	if (!S_ISCHR(vbi_stat.st_mode)) {
		printv("VBI device is no character special file, reject\n");
		return FALSE;
	}

	if (major(vbi_stat.st_rdev) != 81) {
		printv("VBI device CSF has major number %d, expect 81\n"
			"Warning: will assume this is still a v4l device\n",
			major(vbi_stat.st_rdev));
		goto finish;
	}

	printv("VBI device type verified\n");

	if (given_fd > -1) {
		printv("Try suggested corresponding video fd\n");

		if (reverse_lookup(v, given_fd, &vbi_stat))
			if (get_videostd(v, given_fd, &mode))
				goto finish;
	}

	for (i = 0; i < sizeof(video_devices) / sizeof(video_devices[0]); i++) {
		printv("Try %s: ", video_devices[i]);

		if (probe_video_device(v, video_devices[i], &vbi_stat, &mode))
			goto finish;
	}

	printv("Traversing /dev\n");

	if (!(dir = opendir("/dev"))) {
		printv("Cannot open /dev: %d, %s\n", errno, strerror(errno));
		perm_check(v, "/dev");
		goto finish;
	}

	while (readdir_r(dir, &dirent, &pdirent) == 0 && pdirent) {
		char name[256];

		snprintf(name, sizeof(name), "/dev/%s", dirent.d_name);

		printv("Try %s: ", name);

		if (probe_video_device(v, name, &vbi_stat, &mode))
			goto finish;
	}

	closedir(dir);

	printv("Traversing finished\n");

 finish:
	switch (mode) {
	case VIDEO_MODE_NTSC:
		printv("Videostandard is NTSC\n");
		v->dec.scanning = 525;
		break;

	case VIDEO_MODE_PAL:
	case VIDEO_MODE_SECAM:
		printv("Videostandard is PAL/SECAM\n");
		v->dec.scanning = 625;
		break;

	default:
		/*
		 *  One last chance, we'll try to guess
		 *  the scanning if GVBIFMT is available.
		 */
		printv("Videostandard unknown (%d)\n", mode);
		v->dec.scanning = 0;
		*strict = TRUE;
		break;
	}

	return TRUE;
}

static vbi_bool
set_parameters(vbi_capture_v4l *v, struct vbi_format *p_vfmt, int *p_max_rate,
	       unsigned int *services, int strict,
	       char **errorstr)
{
	struct vbi_format vfmt_temp;
	vbi_raw_decoder dec_temp;
	unsigned int sup_services;

	/* check if the driver supports CSVBIFMT: try with unchanged parameters */
	if (v->has_s_fmt == -1) {
		vfmt_temp = *p_vfmt;
		v->has_s_fmt = ((IOCTL(v->fd, VIDIOCSVBIFMT, &vfmt_temp) == 0) || (errno == EBUSY));
		printv("Driver does%s support VIDIOCSVBIFMT\n", v->has_s_fmt ? "" : " not");
	}

	if (v->has_s_fmt == 0)
		return TRUE;

	/* Speculative, vbi_format is not documented */

	printv("Attempt to set vbi capture parameters\n");

	memset(&dec_temp, 0, sizeof(dec_temp));
	sup_services = vbi_raw_decoder_parameters(&dec_temp, *services | dec_temp.services,
					          dec_temp.scanning, p_max_rate);

	if ((sup_services & *services) == 0) {
		vbi_asprintf(errorstr, _("Sorry, %s (%s) cannot capture any of the "
					 "requested data services."),
			     v->p_dev_name, v->vcap.name);
		return FALSE;
	}

	*services &= sup_services;

	vfmt_temp = *p_vfmt;
	memset(p_vfmt, 0, sizeof(*p_vfmt));

	p_vfmt->sample_format		= VIDEO_PALETTE_RAW;
	p_vfmt->sampling_rate		= dec_temp.sampling_rate;
	p_vfmt->samples_per_line	= dec_temp.bytes_per_line;
	p_vfmt->start[0]		= dec_temp.start[0];
	p_vfmt->count[0]		= dec_temp.count[1];
	p_vfmt->start[1]		= dec_temp.start[0];
	p_vfmt->count[1]		= dec_temp.count[1];

	/* Single field allowed? */

	if (!p_vfmt->count[0]) {
		p_vfmt->start[0] = (dec_temp.scanning == 625) ? 6 : 10;
		p_vfmt->count[0] = 1;
	} else if (!p_vfmt->count[1]) {
		p_vfmt->start[1] = (dec_temp.scanning == 625) ? 318 : 272;
		p_vfmt->count[1] = 1;
	}

	if (IOCTL(v->fd, VIDIOCSVBIFMT, p_vfmt) == 0)
		return TRUE;

	p_vfmt->sampling_rate		= vfmt_temp.sampling_rate;
	p_vfmt->samples_per_line	= vfmt_temp.samples_per_line;
	if (IOCTL(v->fd, VIDIOCSVBIFMT, p_vfmt) == 0)
		return TRUE;

	/* XXX correct count */
	p_vfmt->start[0]		= vfmt_temp.start[0];
	p_vfmt->start[1]		= vfmt_temp.start[1];
	if (IOCTL(v->fd, VIDIOCSVBIFMT, p_vfmt) == 0)
		return TRUE;

	switch (errno) {
	case EBUSY:
#ifndef REQUIRE_SVBIFMT
		printv("VIDIOCSVBIFMT returned EBUSY, "
		       "will try the current parameters\n");
		*p_vfmt = vfmt_temp;
		return TRUE;
#endif
		vbi_asprintf(errorstr, _("Cannot initialize %s (%s), "
					 "the device is already in use."),
			     v->p_dev_name, v->vcap.name);
		break;

	case EINVAL:
                if (strict < 2) {
		        printv("VIDIOCSVBIFMT returned EINVAL, "
		               "will try the current parameters\n");
                        *p_vfmt = vfmt_temp;
                        return TRUE;
                }
		break;
	default:
		vbi_asprintf(errorstr, _("Could not set the vbi "
					 "capture parameters for %s (%s): %d, %s."),
			     v->p_dev_name, v->vcap.name, errno, strerror(errno));
		/* guess = _("Maybe a bug in the driver or libzvbi."); */
		break;
	}

	return FALSE;
}

static vbi_raw_decoder *
v4l_parameters(vbi_capture *vc)
{
	vbi_capture_v4l *v = PARENT(vc, vbi_capture_v4l, capture);

	return &v->dec;
}

static void
v4l_delete(vbi_capture *vc)
{
	vbi_capture_v4l *v = PARENT(vc, vbi_capture_v4l, capture);

	v4l_read_stop(v);

	vbi_raw_decoder_destroy(&v->dec);

	if (v->sliced_buffer.data)
		free(v->sliced_buffer.data);

	if (v->fd != -1)
		close(v->fd);

	free(v);
}

static int
v4l_get_read_fd(vbi_capture *vc)
{
	vbi_capture_v4l *v = PARENT(vc, vbi_capture_v4l, capture);

	return v->fd;
}

static int
v4l_get_poll_fd(vbi_capture *vc)
{
	vbi_capture_v4l *v = PARENT(vc, vbi_capture_v4l, capture);

	if (v->has_select)
		return v->fd;
	else
		return -1;
}

static void
print_vfmt(const char *s, struct vbi_format *vfmt)
{
	fprintf(stderr, "%sformat %08x, %d Hz, %d bpl, "
		"F1 %d+%d, F2 %d+%d, flags %08x\n", s,
		vfmt->sample_format,
		vfmt->sampling_rate, vfmt->samples_per_line,
		vfmt->start[0], vfmt->count[0],
		vfmt->start[1], vfmt->count[1],
		vfmt->flags);
}

static unsigned int
v4l_add_services(vbi_capture *vc,
		  vbi_bool reset, vbi_bool commit,
		  unsigned int services, int strict,
		  char ** errorstr)
{
	vbi_capture_v4l *v = PARENT(vc, vbi_capture_v4l, capture);
	struct vbi_format vfmt;
	int max_rate;

	max_rate = 0;

	/* suspend capturing, or driver will return EBUSY */
	v4l_suspend(v);

	if (reset)
		vbi_raw_decoder_reset(&v->dec);

	if (IOCTL(v->fd, VIDIOCGVBIFMT, &vfmt) == 0) {
		if (v->dec.start[1] > 0 && v->dec.count[1]) {
			if (v->dec.start[1] >= 286)
				v->dec.scanning = 625;
			else
				v->dec.scanning = 525;
		}

		printv("Driver supports VIDIOCGVBIFMT, "
		       "guessed videostandard %d\n", v->dec.scanning);

		if (v->do_trace)
			print_vfmt("VBI capture parameters supported: ", &vfmt);

		if (strict >= 0 && v->dec.scanning)
			if (!set_parameters(v, &vfmt, &max_rate,
					    &services, strict,
					    errorstr))
				goto failure;

		if (v->do_trace)
			print_vfmt("VBI capture parameters granted: ", &vfmt);

		printv("Accept current vbi parameters\n");

		if (vfmt.sample_format != VIDEO_PALETTE_RAW) {
			vbi_asprintf(errorstr, _("%s (%s) offers unknown vbi "
						 "sampling format #%d. "
						 "This may be a driver bug "
						 "or libzvbi is too old."),
				     v->p_dev_name, v->vcap.name, vfmt.sample_format);
			goto failure;
		}

		/* grow pattern array if necessary
		** note: must do this even if service add fails later, to stay in sync with driver */
		vbi_raw_decoder_resize(&v->dec, vfmt.start, vfmt.count);

		v->dec.sampling_rate		= vfmt.sampling_rate;
		v->dec.bytes_per_line 		= vfmt.samples_per_line;
		if (v->dec.scanning == 625)
			/* v->dec.offset 		= (int)(10.2e-6 * vfmt.sampling_rate); */
			v->dec.offset           = (int)(6.8e-6 * vfmt.sampling_rate);
		else if (v->dec.scanning == 525)
			v->dec.offset		= (int)(9.2e-6 * vfmt.sampling_rate);
		else /* we don't know */
			v->dec.offset		= (int)(9.7e-6 * vfmt.sampling_rate);
		v->dec.start[0] 		= vfmt.start[0];
		v->dec.count[0] 		= vfmt.count[0];
		v->dec.start[1] 		= vfmt.start[1];
		v->dec.count[1] 		= vfmt.count[1];
		v->dec.interlaced		= !!(vfmt.flags & VBI_INTERLACED);
		v->dec.synchronous		= !(vfmt.flags & VBI_UNSYNC);
		v->time_per_frame 		= (v->dec.scanning == 625) ?
						  1.0 / 25 : 1001.0 / 30000;
	} else { 
		int size;

		/*
		 *  If a more reliable method exists to identify the bttv
		 *  driver I'll be glad to hear about it. Lesson: Don't
		 *  call a v4l private IOCTL without knowing who's
		 *  listening. All we know at this point: It's a csf, and
		 *  it may be a v4l device.
		 *  garetxe: This isn't reliable, bttv doesn't return
		 *  anything useful in vcap.name.
		 */
		printv("Driver doesn't support VIDIOCGVBIFMT (errno %d), "
		       "will assume bttv interface\n", errno);

		v->has_select = TRUE; /* it does */

		if (0 && !strstr(v->vcap.name, "bttv")
		      && !strstr(v->vcap.name, "BTTV")) {
			vbi_asprintf(errorstr, _("Cannot capture with %s (%s), "
						 "has no standard vbi interface."),
				     v->p_dev_name, v->vcap.name);
			goto failure;
		}

		v->dec.bytes_per_line 		= 2048;
		v->dec.interlaced		= FALSE;
		v->dec.synchronous		= TRUE;

		printv("Attempt to determine vbi frame size\n");

		if ((size = IOCTL(v->fd, BTTV_VBISIZE, 0)) == -1) {
			printv("Driver does not support BTTV_VBISIZE, "
				"assume old BTTV driver\n");
			v->dec.count[0] = 16;
			v->dec.count[1] = 16;
		} else if (size % 2048) {
			vbi_asprintf(errorstr, _("Cannot identify %s (%s), reported "
						 "vbi frame size suggests this is "
						 "not a bttv driver."),
				     v->p_dev_name, v->vcap.name);
			goto failure;
		} else {
			printv("Driver supports BTTV_VBISIZE: %d bytes, "
			       "assume top field dominance and 2048 bpl\n", size);
			size /= 2048;
			v->dec.count[0] = size >> 1;
			v->dec.count[1] = size - v->dec.count[0];
		}

		switch (v->dec.scanning) {
		default:
#ifdef REQUIRE_VIDEOSTD
			vbi_asprintf(errorstr, _("Cannot set or determine current "
						 "videostandard of %s (%s)."),
				     v->p_dev_name, v->vcap.name);
			goto failure;
#endif
			printv("Warning: Videostandard not confirmed, "
			       "will assume PAL/SECAM\n");

			v->dec.scanning = 625;

			/* fall through */

		case 625:
			/* Not confirmed */
			v->dec.sampling_rate = 35468950;
			v->dec.offset = (int)(9.2e-6 * 35468950);
			v->dec.start[0] = 22 + 1 - v->dec.count[0];
			v->dec.start[1] = 335 + 1 - v->dec.count[1];
			break;

		case 525:
			/* Confirmed for bttv 0.7.52 */
			v->dec.sampling_rate = 28636363;
			v->dec.offset = (int)(9.2e-6 * 28636363);
			v->dec.start[0] = 10;
			v->dec.start[1] = 273;
			break;
		}

		v->time_per_frame =
			(v->dec.scanning == 625) ? 1.0 / 25 : 1001.0 / 30000;
	}

	v->dec.sampling_format = VBI_PIXFMT_YUV420;

	if (services & ~(VBI_SLICED_VBI_525 | VBI_SLICED_VBI_625)) {
		/* Nyquist */

		if (v->dec.sampling_rate < max_rate * 3 / 2) {
			vbi_asprintf(errorstr, _("Cannot capture the requested "
						 "data services with "
						 "%s (%s), the sampling frequency "
						 "%.2f MHz is too low."),
				     v->p_dev_name, v->vcap.name,
				     v->dec.sampling_rate / 1e6);
			goto failure;
		}

		printv("Nyquist check passed\n");

		printv("Request decoding of services 0x%08x, strict level %d\n", services, strict);

		/* those services which are already set must be checked for strictness */
		if ( (strict > 0) && ((services & v->dec.services) != 0) ) {
			unsigned int tmp_services;
			tmp_services = vbi_raw_decoder_check_services(&v->dec, services & v->dec.services, strict);
			/* mask out unsupported services */
			services &= tmp_services | ~(services & v->dec.services);
		}

		if ( (services & ~v->dec.services) != 0 )
		        services &= vbi_raw_decoder_add_services(&v->dec,
							         services & ~ v->dec.services,
							         strict);

		if (services == 0) {
			vbi_asprintf(errorstr, _("Sorry, %s (%s) cannot "
						 "capture any of "
						 "the requested data services."),
				     v->p_dev_name, v->vcap.name);
			goto failure;
		}

		if (v->sliced_buffer.data != NULL)
			free(v->sliced_buffer.data);

		v->sliced_buffer.data =
			malloc((v->dec.count[0] + v->dec.count[1])
			       * sizeof(vbi_sliced));

		if (!v->sliced_buffer.data) {
			vbi_asprintf(errorstr, _("Virtual memory exhausted."));
			errno = ENOMEM;
			goto failure;
		}
	}

	if (commit)
		v4l_read_alloc(v, errorstr);

	printv("Will decode services 0x%08x, added 0x%0x\n", v->dec.services, services);

failure:
	return services;
}

static vbi_capture *
v4l_new(const char *dev_name, int given_fd, int scanning,
	unsigned int *services, int strict,
	char **errorstr, vbi_bool trace)
{
	vbi_capture_v4l *v;

	pthread_once (&vbi_init_once, vbi_init);

	assert(services && *services != 0);

	if (scanning != 525 && scanning != 625)
		scanning = 0;

	if (!(v = (vbi_capture_v4l *) calloc(1, sizeof(*v)))) {
		vbi_asprintf(errorstr, _("Virtual memory exhausted."));
		errno = ENOMEM;
		return NULL;
	}

	v->do_trace = trace;
	printv("Try to open v4l vbi device, libzvbi interface rev.\n"
	       "%s", rcsid);

	v->capture.parameters = v4l_parameters;
	v->capture._delete = v4l_delete;
	v->capture.get_fd = v4l_get_read_fd;
	v->capture.get_poll_fd = v4l_get_poll_fd;
	v->capture.read = v4l_read;
	v->capture.flush = v4l_flush;
	v->capture.add_services = v4l_add_services;

	v->p_dev_name = strdup(dev_name);

	if (v->p_dev_name == NULL) {
		vbi_asprintf(errorstr, _("Virtual memory exhausted."));
		errno = ENOMEM;
		goto failure;
	}

	if ((v->fd = open(v->p_dev_name, O_RDONLY)) == -1) {
		vbi_asprintf(errorstr, _("Cannot open '%s': %d, %s."),
			     v->p_dev_name, errno, strerror(errno));
		perm_check(v, v->p_dev_name);
		goto io_error;
	}

	printv("Opened %s\n", v->p_dev_name);

	if (IOCTL(v->fd, VIDIOCGCAP, &v->vcap) == -1) {
		/*
		 *  Older bttv drivers don't support any
		 *  v4l ioctls, let's see if we can guess the beast.
		 */
		printv("Driver doesn't support VIDIOCGCAP\n");
		strncpy(v->vcap.name, _("driver unknown"), sizeof(v->vcap.name));
		v->vcap.name[sizeof(v->vcap.name) - 1] = 0;

		if (!guess_bttv_v4l(v, &strict, given_fd, scanning))
			goto failure;
	} else {
		if (v->vcap.name[0] != 0) {
			printv("Driver name '%s'\n", v->vcap.name);
		} else {
			strncpy(v->vcap.name, _("driver unknown"), sizeof(v->vcap.name));
			v->vcap.name[sizeof(v->vcap.name) - 1] = 0;
		}

		if (!(v->vcap.type & VID_TYPE_TELETEXT)) {
			vbi_asprintf(errorstr,
				     _("%s (%s) is not a raw vbi device."),
				     v->p_dev_name, v->vcap.name);
			goto failure;
		}

		guess_bttv_v4l(v, &strict, given_fd, scanning);
	}

	printv("%s (%s) is a v4l vbi device\n", v->p_dev_name, v->vcap.name);

	v->has_select = FALSE; /* FIXME if possible */
	v->has_s_fmt = -1;

	v->read_active = FALSE;

	printv("Hinted video standard %d, guessed %d\n",
	       scanning, v->dec.scanning);

	*services = v4l_add_services(&v->capture, FALSE, TRUE,
				     *services, strict, errorstr);

#ifdef REQUIRE_SELECT
	if (!v->select) {
		vbi_asprintf(errorstr, _("%s (%s) does not support "
					 "the select() function."),
			     v->p_dev_name, v->vcap.name);
		goto failure;
	}
#endif

	if (*services == 0) {
		vbi_asprintf(errorstr, _("Sorry, %s (%s) cannot capture any of the "
					 "requested data services."),
			     v->p_dev_name, v->vcap.name);
		goto failure;
	}

	if (!v->dec.scanning && strict >= 1) {
		printv("Try to guess video standard from vbi bottom field "
			"boundaries: start=%d, count=%d\n",
		       v->dec.start[1], v->dec.count[1]);

		if (v->dec.start[1] <= 0 || !v->dec.count[1]) {
			/*
			 *  We may have requested single field capture
			 *  ourselves, but then we had guessed already.
			 */
#ifdef REQUIRE_VIDEOSTD
			vbi_asprintf(errorstr, _("Cannot set or determine current "
						 "videostandard of %s (%s)."),
				     v->p_dev_name, v->vcap.name);
			goto failure;
#endif
			printv("Warning: Videostandard not confirmed, "
			       "will assume PAL/SECAM\n");

			v->dec.scanning = 625;
			v->time_per_frame = 1.0 / 25;
		} else if (v->dec.start[1] < 286) {
			v->dec.scanning = 525;
			v->time_per_frame = 1001.0 / 30000;
		} else {
			v->dec.scanning = 625;
			v->time_per_frame = 1.0 / 25;
		}
	}

	printv("Guessed videostandard %d\n", v->dec.scanning);

	if (!v->has_select)
		printv("Warning: no read select, reading will block\n");

	printv("Successful opened %s (%s)\n",
	       v->p_dev_name, v->vcap.name);

	return &v->capture;

failure:
io_error:
	v4l_delete(&v->capture);

	return NULL;
}

vbi_capture *
vbi_capture_v4l_sidecar_new(const char *dev_name, int video_fd,
			    unsigned int *services, int strict,
			    char **errorstr, vbi_bool trace)
{
	return v4l_new(dev_name, video_fd, 0,
		       services, strict, errorstr, trace);
}

vbi_capture *
vbi_capture_v4l_new(const char *dev_name, int scanning,
		    unsigned int *services, int strict,
		    char **errorstr, vbi_bool trace)
{
	return v4l_new(dev_name, -1, scanning,
		       services, strict, errorstr, trace);
}

#else

vbi_capture *
vbi_capture_v4l_sidecar_new(const char *dev_name, int given_fd,
			    unsigned int *services, int strict,
			    char **errorstr, vbi_bool trace)
{
	pthread_once (&vbi_init_once, vbi_init);
	vbi_asprintf(errorstr, _("V4L interface not compiled."));
	return NULL;
}

vbi_capture *
vbi_capture_v4l_new(const char *dev_name, int scanning,
		     unsigned int *services, int strict,
		     char **errorstr, vbi_bool trace)
{
	pthread_once (&vbi_init_once, vbi_init);
	vbi_asprintf(errorstr, _("V4L interface not compiled."));
	return NULL;
}

#endif /* !ENABLE_V4L */
