/*
 *  libzvbi - V4L2 interface
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

static char rcsid[] = "$Id: io-v4l2.c,v 1.10 2002/10/11 12:31:49 mschimek Exp $";

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#include "vbi.h"

#ifdef ENABLE_V4L2

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <assert.h>
#include <sys/time.h>		/* timeval */
#include <sys/types.h>		/* fd_set */
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <asm/types.h>		/* for videodev2.h */
#include <pthread.h>

#include "io.h"
#include "videodev2.h"

/* same as ioctl(), but repeat if interrupted */
#define IOCTL(fd, cmd, data)						\
({ int __result; do __result = ioctl(fd, cmd, data);			\
   while (__result == -1L && errno == EINTR); __result; })

#define V4L2_LINE 0 /* API rev. Nov 2000 (-1 -> 0) */

#ifndef V4L2_BUF_TYPE_VBI /* API rev. Sep 2000 */
#define V4L2_BUF_TYPE_VBI V4L2_BUF_TYPE_CAPTURE
#endif

#undef REQUIRE_SELECT
#undef REQUIRE_G_FMT		/* before S_FMT */
#undef REQUIRE_S_FMT		/* else accept current format */

#define printv(format, args...)						\
do {									\
	if (trace) {							\
		fprintf(stderr, format ,##args);			\
		fflush(stderr);						\
	}								\
} while (0)

typedef struct vbi_capture_v4l2 {
	vbi_capture		capture;

	int			fd;
	int			btype;			/* v4l2 stream type */
	vbi_bool		streaming;
	vbi_bool		select;
	int			enqueue;

	vbi_raw_decoder		dec;

	double			time_per_frame;

	vbi_capture_buffer	*raw_buffer;
	int			num_raw_buffers;

	vbi_capture_buffer	sliced_buffer;

} vbi_capture_v4l2;

static int
v4l2_stream(vbi_capture *vc, vbi_capture_buffer **raw,
	    vbi_capture_buffer **sliced, struct timeval *timeout)
{
	vbi_capture_v4l2 *v = PARENT(vc, vbi_capture_v4l2, capture);
	struct v4l2_buffer vbuf;
	double time;

	if (v->enqueue == -2) {
		if (IOCTL(v->fd, VIDIOC_STREAMON, &v->btype) == -1)
			return -1;
	} else if (v->enqueue >= 0) {
		vbuf.type = v->btype;
		vbuf.index = v->enqueue;

		if (IOCTL(v->fd, VIDIOC_QBUF, &vbuf) == -1)
			return -1;
	}

	v->enqueue = -1;

	for (;;) {
		struct timeval tv;
		fd_set fds;
		int r;

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

	vbuf.type = v->btype;

	if (IOCTL(v->fd, VIDIOC_DQBUF, &vbuf) == -1)
		return -1;

	time = vbuf.timestamp / 1e9;

	if (raw) {
		if (*raw) {
			(*raw)->size = v->raw_buffer[vbuf.index].size;
			memcpy((*raw)->data, v->raw_buffer[vbuf.index].data,
			       (*raw)->size);
		} else {
			*raw = v->raw_buffer + vbuf.index;
			v->enqueue = vbuf.index;
		}

		(*raw)->timestamp = time;
	}

	if (sliced) {
		int lines;

		if (*sliced) {
			lines = vbi_raw_decode(&v->dec, v->raw_buffer[vbuf.index].data,
					       (vbi_sliced *)(*sliced)->data);
		} else {
			*sliced = &v->sliced_buffer;
			lines = vbi_raw_decode(&v->dec, v->raw_buffer[vbuf.index].data,
					       (vbi_sliced *)(v->sliced_buffer.data));
		}

		(*sliced)->size = lines * sizeof(vbi_sliced);
		(*sliced)->timestamp = time;
	}

	if (v->enqueue == -1) {
		if (IOCTL(v->fd, VIDIOC_QBUF, &vbuf) == -1)
			return -1;
	}

	return 1;
}

static int
v4l2_read(vbi_capture *vc, vbi_capture_buffer **raw,
	  vbi_capture_buffer **sliced, struct timeval *timeout)
{
	vbi_capture_v4l2 *v = PARENT(vc, vbi_capture_v4l2, capture);
	vbi_capture_buffer *my_raw = v->raw_buffer;
	struct timeval tv;
	int r;

	while (v->select) {
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

	for (;;) {
		/* from zapping/libvbi/v4lx.c */
		pthread_testcancel();

		r = read(v->fd, (*raw)->data, (*raw)->size);

		if (r == -1  && (errno == EINTR || errno == ETIME))
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

static vbi_raw_decoder *
v4l2_parameters(vbi_capture *vc)
{
	vbi_capture_v4l2 *v = PARENT(vc, vbi_capture_v4l2, capture);

	return &v->dec;
}

static void
v4l2_delete(vbi_capture *vc)
{
	vbi_capture_v4l2 *v = PARENT(vc, vbi_capture_v4l2, capture);

	if (v->sliced_buffer.data)
		free(v->sliced_buffer.data);

	for (; v->num_raw_buffers > 0; v->num_raw_buffers--)
		if (v->streaming)
			munmap(v->raw_buffer[v->num_raw_buffers - 1].data,
			       v->raw_buffer[v->num_raw_buffers - 1].size);
		else
			free(v->raw_buffer[v->num_raw_buffers - 1].data);

	if (v->fd != -1)
		close(v->fd);

	free(v);
}

static int
v4l2_fd(vbi_capture *vc)
{
	vbi_capture_v4l2 *v = PARENT(vc, vbi_capture_v4l2, capture);

	return v->fd;
}

static void
print_vfmt(char *s, struct v4l2_format *vfmt)
{
	fprintf(stderr, "%s%d Hz, %d bpl, offs %d, "
		"F1 %d+%d, F2 %d+%d, flags %08x\n", s,
		vfmt->fmt.vbi.sampling_rate, vfmt->fmt.vbi.samples_per_line,
		vfmt->fmt.vbi.offset,
		vfmt->fmt.vbi.start[0] - V4L2_LINE, vfmt->fmt.vbi.count[0],
		vfmt->fmt.vbi.start[1] - V4L2_LINE, vfmt->fmt.vbi.count[1],
		vfmt->fmt.vbi.flags);
}

/* document below */
vbi_capture *
vbi_capture_v4l2_new(char *dev_name, int buffers,
		     unsigned int *services, int strict,
		     char **errorstr, vbi_bool trace)
{
	struct v4l2_capability vcap;
	struct v4l2_format vfmt;
	struct v4l2_requestbuffers vrbuf;
	struct v4l2_buffer vbuf;
	struct v4l2_standard vstd;
	char *guess = "";
	vbi_capture_v4l2 *v;
	int max_rate, g_fmt;

	pthread_once (&vbi_init_once, vbi_init);

	assert(services && *services != 0);

	printv("Try to open v4l2 vbi device, libzvbi interface rev.\n"
	       "%s", rcsid);

	if (!(v = calloc(1, sizeof(*v)))) {
		vbi_asprintf(errorstr, _("Virtual memory exhausted."));
		errno = ENOMEM;
		return NULL;
	}

	v->capture.parameters = v4l2_parameters;
	v->capture._delete = v4l2_delete;
	v->capture.get_fd = v4l2_fd;

	/* O_RDWR required for PROT_WRITE */
	if ((v->fd = open(dev_name, O_RDWR)) == -1) {
		if ((v->fd = open(dev_name, O_RDONLY)) == -1) {
			vbi_asprintf(errorstr, _("Cannot open '%s': %d, %s."),
				     dev_name, errno, strerror(errno));
			goto io_error;
		}
	}

	printv("Opened %s\n", dev_name);

	if (IOCTL(v->fd, VIDIOC_QUERYCAP, &vcap) == -1) {
		vbi_asprintf(errorstr, _("Cannot identify '%s': %d, %s."),
			     dev_name, errno, strerror(errno));
		guess = _("Probably not a v4l2 device.");
		goto io_error;
	}

	if (vcap.type != V4L2_TYPE_VBI) {
		vbi_asprintf(errorstr, _("%s (%s) is not a raw vbi device."),
			     dev_name, vcap.name);
		goto failure;
	}

	printv("%s (%s) is a v4l2 vbi device\n", dev_name, vcap.name);

	v->select = !!(vcap.flags & V4L2_FLAG_SELECT);

#ifdef REQUIRE_SELECT
	if (!v->select) {
		vbi_asprintf(errorstr, _("%s (%s) does not support the select() function."),
			     dev_name, vcap.name);
		goto failure;
	}
#endif

	/* mandatory, http://www.thedirks.org/v4l2/v4l2dsi.htm */
	if (IOCTL(v->fd, VIDIOC_G_STD, &vstd) == -1) {
		vbi_asprintf(errorstr, _("Cannot query current videostandard of %s (%s): %d, %s."),
			     dev_name, vcap.name, errno, strerror(errno));
		guess = _("Probably a driver bug.");
		goto io_error;
	}

	printv("Current scanning system is %d\n", vstd.framelines);

	/* add_vbi_services() eliminates non 525/625 */
	v->dec.scanning = vstd.framelines;

	memset(&vfmt, 0, sizeof(vfmt));

	vfmt.type = v->btype = V4L2_BUF_TYPE_VBI;

	max_rate = 0;

	printv("Querying current vbi parameters... ");

	if ((g_fmt = IOCTL(v->fd, VIDIOC_G_FMT, &vfmt)) == -1) {
		printv("failed\n");
#ifdef REQUIRE_G_FMT
		vbi_asprintf(errorstr, _("Cannot query current vbi parameters of %s (%s): %d, %s."),
			     dev_name, vcap.name, errno, strerror(errno));
		goto io_error;
#else
		strict = MAX(0, strict);
#endif
	} else {
		printv("success\n");
	}

	if (strict >= 0) {
		printv("Attempt to set vbi capture parameters\n");

		*services = vbi_raw_decoder_parameters(&v->dec, *services,
						       v->dec.scanning, &max_rate);

		if (*services == 0) {
			vbi_asprintf(errorstr, _("Sorry, %s (%s) cannot capture any of the "
					       "requested data services."), dev_name, vcap.name);
			goto failure;
		}

		vfmt.fmt.vbi.sample_format	= V4L2_VBI_SF_UBYTE;
		vfmt.fmt.vbi.sampling_rate	= v->dec.sampling_rate;
		vfmt.fmt.vbi.samples_per_line	= v->dec.bytes_per_line;
		vfmt.fmt.vbi.offset		= v->dec.offset;
		vfmt.fmt.vbi.start[0]		= v->dec.start[0] + V4L2_LINE;
		vfmt.fmt.vbi.count[0]		= v->dec.count[1];
		vfmt.fmt.vbi.start[1]		= v->dec.start[0] + V4L2_LINE;
		vfmt.fmt.vbi.count[1]		= v->dec.count[1];

		/* API rev. Nov 2000 paranoia */

		if (!vfmt.fmt.vbi.count[0]) {
			vfmt.fmt.vbi.start[0] = ((v->dec.scanning == 625) ? 6 : 10) + V4L2_LINE;
			vfmt.fmt.vbi.count[0] = 1;
		} else if (!vfmt.fmt.vbi.count[1]) {
			vfmt.fmt.vbi.start[1] = ((v->dec.scanning == 625) ? 318 : 272) + V4L2_LINE;
			vfmt.fmt.vbi.count[1] = 1;
		}

		if (trace)
			print_vfmt("VBI capture parameters requested: ", &vfmt);

		if (IOCTL(v->fd, VIDIOC_S_FMT, &vfmt) == -1) {
			switch (errno) {
			case EBUSY:
#ifndef REQUIRE_S_FMT
				if (g_fmt != -1) {
					printv("VIDIOC_S_FMT returned EBUSY, "
					       "will try the current parameters\n");
					break;
				}
#endif
				vbi_asprintf(errorstr, _("Cannot initialize %s (%s), "
						       "the device is already in use."),
					     dev_name, vcap.name);
				goto failure;

			case EINVAL:
				printv("VIDIOC_S_FMT failed, trying bttv2 rev. 021100 workaround\n");

				vfmt.type = v->btype = V4L2_BUF_TYPE_CAPTURE;
				vfmt.fmt.vbi.start[0] = 0;
				vfmt.fmt.vbi.count[0] = 16;
				vfmt.fmt.vbi.start[1] = 313;
				vfmt.fmt.vbi.count[1] = 16;

				if (IOCTL(v->fd, VIDIOC_S_FMT, &vfmt) == -1) {
			default:
					vbi_asprintf(errorstr, _("Could not set the vbi capture parameters "
							       "for %s (%s): %d, %s."),
						     dev_name, vcap.name, errno, strerror(errno));
					guess = _("Possibly a driver bug.");
					goto io_error;
				}

				vfmt.fmt.vbi.start[0] = 7 + V4L2_LINE;
				vfmt.fmt.vbi.start[1] = 320 + V4L2_LINE;

				break;
			}
		}

		printv("Successful set vbi capture parameters\n");
	}

	if (trace)
		print_vfmt("VBI capture parameters granted: ", &vfmt);

	v->dec.sampling_rate		= vfmt.fmt.vbi.sampling_rate;
	v->dec.bytes_per_line		= vfmt.fmt.vbi.samples_per_line;
	v->dec.offset			= vfmt.fmt.vbi.offset;
	v->dec.start[0] 		= vfmt.fmt.vbi.start[0] - V4L2_LINE;
	v->dec.count[0] 		= vfmt.fmt.vbi.count[0];
	v->dec.start[1] 		= vfmt.fmt.vbi.start[1] - V4L2_LINE;
	v->dec.count[1] 		= vfmt.fmt.vbi.count[1];
	v->dec.interlaced		= !!(vfmt.fmt.vbi.flags & V4L2_VBI_INTERLACED);
	v->dec.synchronous		= !(vfmt.fmt.vbi.flags & V4L2_VBI_UNSYNC);
	v->time_per_frame 		= (v->dec.scanning == 625) ? 1.0 / 25 : 1001.0 / 30000;

	if (vfmt.fmt.vbi.sample_format != V4L2_VBI_SF_UBYTE) {
		vbi_asprintf(errorstr, _("%s (%s) offers unknown vbi sampling format #%d. "
				       "This may be a driver bug or libzvbi is too old."),
			     dev_name, vcap.name, vfmt.fmt.vbi.sample_format);
		goto failure;
	}

	v->dec.sampling_format = VBI_PIXFMT_YUV420;

	if (*services & ~(VBI_SLICED_VBI_525 | VBI_SLICED_VBI_625)) {
		/* Nyquist (we're generous at 1.5) */

		if (v->dec.sampling_rate < max_rate * 3 / 2) {
			vbi_asprintf(errorstr, _("Cannot capture the requested "
						 "data services with "
						 "%s (%s), the sampling frequency "
						 "%.2f MHz is too low."),
				     dev_name, vcap.name,
				     v->dec.sampling_rate / 1e6);
			goto failure;
		}

		printv("Nyquist check passed\n");

		printv("Request decoding of services 0x%08x\n", *services);

		*services = vbi_raw_decoder_add_services(&v->dec, *services, strict);

		if (*services == 0) {
			vbi_asprintf(errorstr, _("Sorry, %s (%s) cannot capture any of "
					       "the requested data services."),
				     dev_name, vcap.name);
			goto failure;
		}

		v->sliced_buffer.data =
			malloc((v->dec.count[0] + v->dec.count[1]) * sizeof(vbi_sliced));

		if (!v->sliced_buffer.data) {
			vbi_asprintf(errorstr, _("Virtual memory exhausted."));
			errno = ENOMEM;
			goto failure;
		}
	}

	printv("Will decode services 0x%08x\n", *services);

	if (vcap.flags & V4L2_FLAG_STREAMING) {
		printv("Using streaming interface\n");

		if (!v->select) {
			/* Mandatory; dequeue buffer is non-blocking. */
			vbi_asprintf(errorstr, _("%s (%s) does not support the select() function."),
				     dev_name, vcap.name);
			goto failure;
		}

		v->streaming = TRUE;
		v->enqueue = -2;

		v->capture.read = v4l2_stream;

		printv("Fifo initialized\nRequesting streaming i/o buffers\n");

		vrbuf.type = v->btype;
		vrbuf.count = buffers;

		if (IOCTL(v->fd, VIDIOC_REQBUFS, &vrbuf) == -1) {
			vbi_asprintf(errorstr, _("Cannot request streaming i/o buffers "
					       "from %s (%s): %d, %s."),
				     dev_name, vcap.name, errno, strerror(errno));
			guess = _("Possibly a driver bug.");
			goto failure;
		}

		if (vrbuf.count == 0) {
			vbi_asprintf(errorstr, _("%s (%s) granted no streaming i/o buffers, "
					       "perhaps the physical memory is exhausted."),
				     dev_name, vcap.name);
			goto failure;
		}

		printv("Mapping %d streaming i/o buffers\n", vrbuf.count);

		v->raw_buffer = calloc(vrbuf.count, sizeof(v->raw_buffer[0]));

		if (!v->raw_buffer) {
			vbi_asprintf(errorstr, _("Virtual memory exhausted."));
			errno = ENOMEM;
			goto failure;
		}

		v->num_raw_buffers = 0;

		while (v->num_raw_buffers < vrbuf.count) {
			uint8_t *p;

			vbuf.type = v->btype;
			vbuf.index = v->num_raw_buffers;

			if (IOCTL(v->fd, VIDIOC_QUERYBUF, &vbuf) == -1) {
				vbi_asprintf(errorstr, _("Querying streaming i/o buffer #%d "
						       "from %s (%s) failed: %d, %s."),
					     v->num_raw_buffers, dev_name, vcap.name,
					     errno, strerror(errno));
				goto mmap_failure;
			}

			/* bttv 0.8.x wants PROT_WRITE */
			p = mmap(NULL, vbuf.length, PROT_READ | PROT_WRITE,
				 MAP_SHARED, v->fd, vbuf.offset); /* MAP_PRIVATE ? */

			if ((int) p == -1)
			  p = mmap(NULL, vbuf.length, PROT_READ,
				   MAP_SHARED, v->fd, vbuf.offset); /* MAP_PRIVATE ? */

			if ((int) p == -1) {
				if (errno == ENOMEM && v->num_raw_buffers >= 2) {
					printv("Memory mapping buffer #%d failed: %d, %s (ignored).",
					       v->num_raw_buffers, errno, strerror(errno));
					break;
				}

				vbi_asprintf(errorstr, _("Memory mapping streaming i/o buffer #%d "
						       "from %s (%s) failed: %d, %s."),
					     v->num_raw_buffers, dev_name, vcap.name,
					     errno, strerror(errno));
				goto mmap_failure;
			} else {
				unsigned int i, s;

				v->raw_buffer[v->num_raw_buffers].data = p;
				v->raw_buffer[v->num_raw_buffers].size = vbuf.length;

				for (i = s = 0; i < vbuf.length; i++)
					s += p[i];

				if (s % vbuf.length) {
					fprintf(stderr,
					       "Security warning: driver %s (%s) seems to mmap "
					       "physical memory uncleared. Please contact the "
					       "driver author.\n", dev_name, vcap.name);
					exit(EXIT_FAILURE);
				}
			}

			if (IOCTL(v->fd, VIDIOC_QBUF, &vbuf) == -1) {
				vbi_asprintf(errorstr, _("Cannot enqueue streaming i/o buffer #%d "
						       "to %s (%s): %d, %s."),
					     v->num_raw_buffers, dev_name, vcap.name,
					     errno, strerror(errno));
				guess = _("Probably a driver bug.");
				goto mmap_failure;
			}

			v->num_raw_buffers++;
		}
	} else if (vcap.flags & V4L2_FLAG_READ) {
		printv("Using read interface\n");

		if (!v->select)
			printv("Warning: no read select, reading will block\n");

		v->capture.read = v4l2_read;

		v->raw_buffer = calloc(1, sizeof(v->raw_buffer[0]));

		if (!v->raw_buffer) {
			vbi_asprintf(errorstr, _("Virtual memory exhausted."));
			errno = ENOMEM;
			goto failure;
		}

		v->raw_buffer[0].size = (v->dec.count[0] + v->dec.count[1])
			* v->dec.bytes_per_line;

		v->raw_buffer[0].data = malloc(v->raw_buffer[0].size);

		if (!v->raw_buffer[0].data) {
			vbi_asprintf(errorstr, _("Not enough memory to allocate "
					       "vbi capture buffer (%d KB)."),
				     (v->raw_buffer[0].size + 1023) >> 10);
			goto failure;
		}

		v->num_raw_buffers = 1;

		printv("Capture buffer allocated\n");
	} else {
		vbi_asprintf(errorstr, _("%s (%s) lacks a vbi read interface, "
				       "possibly an output only device or a driver bug."),
			     dev_name, vcap.name);
		goto failure;
	}

	printv("Successful opened %s (%s)\n",
	       dev_name, vcap.name);

	return &v->capture;

mmap_failure:
	for (; v->num_raw_buffers > 0; v->num_raw_buffers--)
		munmap(v->raw_buffer[v->num_raw_buffers - 1].data,
		       v->raw_buffer[v->num_raw_buffers - 1].size);

io_error:
failure:
	v4l2_delete(&v->capture);

	return NULL;
}

#else

/**
 * @param dev_name Name of the device to open, usually one of
 *   @c /dev/vbi or @c /dev/vbi0 and up.
 * @param buffers Number of device buffers for raw vbi data, when
 *   the driver supports streaming. Otherwise one bounce buffer
 *   is allocated for vbi_capture_pull().
 * @param services This must point to a set of @ref VBI_SLICED_
 *   symbols describing the
 *   data services to be decoded. On return the services actually
 *   decodable will be stored here. See vbi_raw_decoder_add()
 *   for details. If you want to capture raw data only, set to
 *   @c VBI_SLICED_VBI_525, @c VBI_SLICED_VBI_625 or both.
 * @param strict Will be passed to vbi_raw_decoder_add().
 * @param errorstr If not @c NULL this function stores a pointer to an error
 *   description here. You must free() this string when no longer needed.
 * @param trace If @c TRUE print progress messages on stderr.
 * 
 * @return
 * Initialized vbi_capture context, @c NULL on failure.
 */
vbi_capture *
vbi_capture_v4l2_new(char *dev_name, int buffers,
		     unsigned int *services, int strict,
		     char **errorstr, vbi_bool trace)
{
	pthread_once (&vbi_init_once, vbi_init);
	vbi_asprintf(errorstr, _("V4L2 interface not compiled."));
	return NULL;
}

#endif /* !ENABLE_V4L2 */
