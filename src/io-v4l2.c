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

static char rcsid[] = "$Id: io-v4l2.c,v 1.19 2003/05/24 12:18:04 tomzo Exp $";

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#include "vbi.h"
#include "io.h"

#ifdef ENABLE_V4L2

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
#include <asm/types.h>		/* for videodev2.h */
#include <pthread.h>

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

#define ENQUEUE_SUSPENDED       -3
#define ENQUEUE_STREAM_OFF      -2
#define ENQUEUE_BUFS_QUEUED     -1
#define ENQUEUE_IS_UNQUEUED(X)  ((X) >= 0)

#define printv(format, args...)						\
do {									\
	if (v->do_trace) {							\
		fprintf(stderr, format ,##args);			\
		fflush(stderr);						\
	}								\
} while (0)

typedef struct vbi_capture_v4l2 {
	vbi_capture		capture;

	int			fd;
	int			btype;			/* v4l2 stream type */
	vbi_bool		streaming;
	vbi_bool		read_active;
	vbi_bool		has_select;
	vbi_bool		do_trace;
	int			enqueue;
	struct v4l2_capability  vcap;
        char                  * p_dev_name;

	vbi_raw_decoder		dec;

	double			time_per_frame;

	vbi_capture_buffer	*raw_buffer;
	int			num_raw_buffers;
	int			buf_req_count;

	vbi_capture_buffer	sliced_buffer;

} vbi_capture_v4l2;


static void
v4l2_stream_stop(vbi_capture_v4l2 *v)
{
	if (v->enqueue >= ENQUEUE_BUFS_QUEUED)
		IOCTL(v->fd, VIDIOC_STREAMOFF, &v->btype);

	v->enqueue = ENQUEUE_SUSPENDED;

	for (; v->num_raw_buffers > 0; v->num_raw_buffers--)
		munmap(v->raw_buffer[v->num_raw_buffers - 1].data,
		       v->raw_buffer[v->num_raw_buffers - 1].size);

	free(v->raw_buffer);
	v->raw_buffer = NULL;
}


static int
v4l2_stream_alloc(vbi_capture_v4l2 *v, char ** errorstr)
{
	struct v4l2_requestbuffers vrbuf;
	struct v4l2_buffer vbuf;
	char *guess = "";

	assert(v->enqueue == ENQUEUE_SUSPENDED);
	assert(v->raw_buffer == NULL);
	printv("Fifo initialized\nRequesting streaming i/o buffers\n");

	vrbuf.type = v->btype;
	vrbuf.count = v->buf_req_count;

	if (IOCTL(v->fd, VIDIOC_REQBUFS, &vrbuf) == -1) {
		vbi_asprintf(errorstr, _("Cannot request streaming i/o buffers "
				       "from %s (%s): %d, %s."),
			     v->p_dev_name, v->vcap.name, errno, strerror(errno));
		guess = _("Possibly a driver bug.");
		goto failure;
	}

	if (vrbuf.count == 0) {
		vbi_asprintf(errorstr, _("%s (%s) granted no streaming i/o buffers, "
				       "perhaps the physical memory is exhausted."),
			     v->p_dev_name, v->vcap.name);
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
				     v->num_raw_buffers, v->p_dev_name, v->vcap.name,
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
				     v->num_raw_buffers, v->p_dev_name, v->vcap.name,
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
				       "driver author.\n", v->p_dev_name, v->vcap.name);
				exit(EXIT_FAILURE);
			}
		}

		if (IOCTL(v->fd, VIDIOC_QBUF, &vbuf) == -1) {
			vbi_asprintf(errorstr, _("Cannot enqueue streaming i/o buffer #%d "
					       "to %s (%s): %d, %s."),
				     v->num_raw_buffers, v->p_dev_name, v->vcap.name,
				     errno, strerror(errno));
			guess = _("Probably a driver bug.");
			goto mmap_failure;
		}

		v->num_raw_buffers++;
	}
	return 0;

mmap_failure:
failure:
	v4l2_stream_stop(v);

	return -1;
}


static int
v4l2_stream(vbi_capture *vc, vbi_capture_buffer **raw,
	    vbi_capture_buffer **sliced, struct timeval *timeout)
{
	vbi_capture_v4l2 *v = PARENT(vc, vbi_capture_v4l2, capture);
	struct v4l2_buffer vbuf;
	double time;

	if (v->enqueue == ENQUEUE_STREAM_OFF) {
		if (IOCTL(v->fd, VIDIOC_STREAMON, &v->btype) == -1)
			return -1;
	} else if (ENQUEUE_IS_UNQUEUED(v->enqueue)) {
		vbuf.type = v->btype;
		vbuf.index = v->enqueue;

		if (IOCTL(v->fd, VIDIOC_QBUF, &vbuf) == -1)
			return -1;
	}

	v->enqueue = ENQUEUE_BUFS_QUEUED;

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

	if (v->enqueue == ENQUEUE_BUFS_QUEUED) {
		if (IOCTL(v->fd, VIDIOC_QBUF, &vbuf) == -1)
			return -1;
	}

	return 1;
}

static void v4l2_stream_flush(vbi_capture *vc)
{
	vbi_capture_v4l2 *v = PARENT(vc, vbi_capture_v4l2, capture);
	struct v4l2_buffer vbuf;
	struct timeval tv;
	fd_set fds;
	int fd_flags = 0;
	int max_loop;
	int ret;

	/* stream not enabled yet -> nothing to flush */
	if ( (v->enqueue == ENQUEUE_SUSPENDED) ||
             (v->enqueue == ENQUEUE_STREAM_OFF) )
		return;

	if (ENQUEUE_IS_UNQUEUED(v->enqueue)) {
		/* re-queue buffer of the read call on the stream */
		vbuf.type = v->btype;
		vbuf.index = v->enqueue;

		if (IOCTL(v->fd, VIDIOC_QBUF, &vbuf) == -1)
			return;
	}
	v->enqueue = ENQUEUE_BUFS_QUEUED;

	if (v->has_select == FALSE) {
		fd_flags = fcntl(v->fd, F_GETFL, NULL);
		if (fd_flags == -1)
			return;
		/* no select supported by driver -> make read non-blocking */
		if ((fd_flags & O_NONBLOCK) == 0) {
			fcntl(v->fd, F_SETFL, fd_flags | O_NONBLOCK);
		}
	}

	for (max_loop = 0; max_loop < v->num_raw_buffers; max_loop++) {

		/* check if there are any buffers pending for de-queueing */
		while (v->has_select) {
			FD_ZERO(&fds);
			FD_SET(v->fd, &fds);

			/* use zero timeout to prevent select() from blocking */
			memset(&tv, 0, sizeof(tv));

			ret = select(v->fd + 1, &fds, NULL, NULL, &tv);

			if ( !((ret < 0) && (errno == EINTR)) )
				break;

			/* no buffers ready or an error occurred -> done */
			if (ret <= 0)
				goto done;
		}

		if (IOCTL(v->fd, VIDIOC_DQBUF, &vbuf) == -1)
			goto done;

		/* immediately queue the buffer again, thereby discarding it's content */
		if (IOCTL(v->fd, VIDIOC_QBUF, &vbuf) == -1)
			goto done;
	}

done:
	if ((v->has_select == FALSE) && ((fd_flags & O_NONBLOCK) == 0)) {
		fcntl(v->fd, F_SETFL, fd_flags);
	}
}

static void
v4l2_read_stop(vbi_capture_v4l2 *v)
{
	for (; v->num_raw_buffers > 0; v->num_raw_buffers--)
		if (v->streaming)
			munmap(v->raw_buffer[v->num_raw_buffers - 1].data,
			       v->raw_buffer[v->num_raw_buffers - 1].size);
		else
			free(v->raw_buffer[v->num_raw_buffers - 1].data);

	free(v->raw_buffer);
	v->raw_buffer = NULL;
}


static int
v4l2_suspend(vbi_capture_v4l2 *v)
{
	int    fd;

	if (v->streaming) {
		printv("Suspending stream...\n");
		v4l2_stream_stop(v);
	}
	else {
		v4l2_read_stop(v);

		if (v->read_active) {
			printv("Suspending read: re-open device...\n");

			/* hack: cannot suspend read to allow S_FMT, need to close device */
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
	}
	return 0;
}


static int
v4l2_read_alloc(vbi_capture_v4l2 *v, char ** errorstr)
{
	assert(v->raw_buffer == NULL);

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
	return 0;

failure:
	return -1;
}

static int
v4l2_read(vbi_capture *vc, vbi_capture_buffer **raw,
	  vbi_capture_buffer **sliced, struct timeval *timeout)
{
	vbi_capture_v4l2 *v = PARENT(vc, vbi_capture_v4l2, capture);
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

	if (raw == NULL)
		raw = &my_raw;
	if (*raw == NULL)
		*raw = v->raw_buffer;
	else
		(*raw)->size = v->raw_buffer[0].size;

	v->read_active = TRUE;

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


static void v4l2_read_flush(vbi_capture *vc)
{
	vbi_capture_v4l2 *v = PARENT(vc, vbi_capture_v4l2, capture);
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

	if (v->streaming)
		v4l2_stream_stop(v);
	else
		v4l2_read_stop(v);

	vbi_raw_decoder_destroy(&v->dec);

	if (v->sliced_buffer.data)
		free(v->sliced_buffer.data);

	if (v->p_dev_name != NULL)
		free(v->p_dev_name);

	if (v->fd != -1)
		close(v->fd);

	free(v);
}

static int
v4l2_get_read_fd(vbi_capture *vc)
{
	vbi_capture_v4l2 *v = PARENT(vc, vbi_capture_v4l2, capture);

	return v->fd;
}

static int
v4l2_get_poll_fd(vbi_capture *vc)
{
	vbi_capture_v4l2 *v = PARENT(vc, vbi_capture_v4l2, capture);

	if (v->has_select)
		return v->fd;
	else
		return -1;
}

static void
print_vfmt(char *s, struct v4l2_format *vfmt)
{
	fprintf(stderr, "%sformat %08x [%c%c%c%c], %d Hz, %d bpl, offs %d, "
		"F1 %d+%d, F2 %d+%d, flags %08x\n", s,
		vfmt->fmt.vbi.sample_format,
		(char)((vfmt->fmt.vbi.sample_format      ) & 0xff),
		(char)((vfmt->fmt.vbi.sample_format >>  8) & 0xff),
		(char)((vfmt->fmt.vbi.sample_format >> 16) & 0xff),
		(char)((vfmt->fmt.vbi.sample_format >> 24) & 0xff),
		vfmt->fmt.vbi.sampling_rate, vfmt->fmt.vbi.samples_per_line,
		vfmt->fmt.vbi.offset,
		vfmt->fmt.vbi.start[0] - V4L2_LINE, vfmt->fmt.vbi.count[0],
		vfmt->fmt.vbi.start[1] - V4L2_LINE, vfmt->fmt.vbi.count[1],
		vfmt->fmt.vbi.flags);
}

static unsigned int
v4l2_add_services(vbi_capture *vc,
		  vbi_bool reset, vbi_bool commit,
		  unsigned int services, int strict,
		  char ** errorstr)
{
	vbi_capture_v4l2 *v = PARENT(vc, vbi_capture_v4l2, capture);
	struct v4l2_format vfmt;
	int max_rate, g_fmt;
	char *guess = "";

	/* suspend capturing, or driver will return EBUSY */
	v4l2_suspend(v);

	if (reset)
		vbi_raw_decoder_reset(&v->dec);

	memset(&vfmt, 0, sizeof(vfmt));

	vfmt.type = v->btype = V4L2_BUF_TYPE_VBI;

	max_rate = 0;

	printv("Querying current vbi parameters... ");

	if ((g_fmt = IOCTL(v->fd, VIDIOC_G_FMT, &vfmt)) == -1) {
		printv("failed\n");
#ifdef REQUIRE_G_FMT
		vbi_asprintf(errorstr, _("Cannot query current vbi parameters of %s (%s): %d, %s."),
			     v->p_dev_name, v->vcap.name, errno, strerror(errno));
		goto io_error;
#else
		strict = MAX(0, strict);
#endif
	} else {
		printv("success\n");
	}

	if (strict >= 0) {
		struct v4l2_format vfmt_temp = vfmt;
		vbi_raw_decoder dec_temp;
		unsigned int sup_services;

		printv("Attempt to set vbi capture parameters\n");

		memset(&dec_temp, 0, sizeof(dec_temp));
		sup_services = vbi_raw_decoder_parameters(&dec_temp, services | v->dec.services,
						          v->dec.scanning, &max_rate);

		if ((sup_services & services) == 0) {
			vbi_asprintf(errorstr, _("Sorry, %s (%s) cannot capture any of the "
					       "requested data services %d."),
				                v->p_dev_name, v->vcap.name, v->dec.scanning);
			goto failure;
		}

		services &= sup_services;

		vfmt.fmt.vbi.sample_format	= V4L2_VBI_SF_UBYTE;
		vfmt.fmt.vbi.sampling_rate	= dec_temp.sampling_rate;
		vfmt.fmt.vbi.samples_per_line	= dec_temp.bytes_per_line;
		vfmt.fmt.vbi.offset		= dec_temp.offset;
		vfmt.fmt.vbi.start[0]		= dec_temp.start[0] + V4L2_LINE;
		vfmt.fmt.vbi.count[0]		= dec_temp.count[0];
		vfmt.fmt.vbi.start[1]		= dec_temp.start[1] + V4L2_LINE;
		vfmt.fmt.vbi.count[1]		= dec_temp.count[1];

		/* API rev. Nov 2000 paranoia */
/*
		if (!vfmt.fmt.vbi.count[0]) {
			vfmt.fmt.vbi.start[0] = ((dec_temp.scanning == 625) ? 6 : 10) + V4L2_LINE;
			vfmt.fmt.vbi.count[0] = 1;
		} else if (!vfmt.fmt.vbi.count[1]) {
			vfmt.fmt.vbi.start[1] = ((dec_temp.scanning == 625) ? 318 : 272) + V4L2_LINE;
			vfmt.fmt.vbi.count[1] = 1;
		}
*/
		if (v->do_trace)
			print_vfmt("VBI capture parameters requested: ", &vfmt);

		if (IOCTL(v->fd, VIDIOC_S_FMT, &vfmt) == -1) {
			switch (errno) {
			case EBUSY:
#ifndef REQUIRE_S_FMT
				if (g_fmt != -1) {
					printv("VIDIOC_S_FMT returned EBUSY, "
					       "will try the current parameters\n");
					vfmt = vfmt_temp;
					break;
				}
#endif
				vbi_asprintf(errorstr, _("Cannot initialize %s (%s), "
						       "the device is already in use."),
					     v->p_dev_name, v->vcap.name);
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
						     v->p_dev_name, v->vcap.name, errno, strerror(errno));
					guess = _("Possibly a driver bug.");
					goto io_error;
				}

				vfmt.fmt.vbi.start[0] = 7 + V4L2_LINE;
				vfmt.fmt.vbi.start[1] = 320 + V4L2_LINE;

				break;
			}

		} else {
			printv("Successful set vbi capture parameters\n");
		}
	}

	if (v->do_trace)
		print_vfmt("VBI capture parameters granted: ", &vfmt);

        /* grow pattern array if necessary
	** note: must do this even if service add fails later, to stay in sync with driver */
	vbi_raw_decoder_resize(&v->dec, vfmt.fmt.vbi.start, vfmt.fmt.vbi.count);

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
			     v->p_dev_name, v->vcap.name, vfmt.fmt.vbi.sample_format);
		goto failure;
	}

	v->dec.sampling_format = VBI_PIXFMT_YUV420;

	if (services & ~(VBI_SLICED_VBI_525 | VBI_SLICED_VBI_625)) {
		/* Nyquist (we're generous at 1.5) */

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
			vbi_asprintf(errorstr, _("Sorry, %s (%s) cannot capture any of "
					       "the requested data services."),
				     v->p_dev_name, v->vcap.name);
			goto failure;
		}

		if (v->sliced_buffer.data != NULL)
			free(v->sliced_buffer.data);

		v->sliced_buffer.data =
			malloc((v->dec.count[0] + v->dec.count[1]) * sizeof(vbi_sliced));

		if (!v->sliced_buffer.data) {
			vbi_asprintf(errorstr, _("Virtual memory exhausted."));
			errno = ENOMEM;
			goto failure;
		}
	}

	printv("Will decode services 0x%08x, added 0x%0x\n", v->dec.services, services);

	if (commit) {
		if (v->streaming) {
			if (v4l2_stream_alloc(v, errorstr) != 0)
				goto io_error;
		} else {
			if (v4l2_read_alloc(v, errorstr) != 0)
				goto io_error;
		}
	}

	return services;

io_error:
failure:
	return 0;
}

/* document below */
vbi_capture *
vbi_capture_v4l2_new(const char *dev_name, int buffers,
		     unsigned int *services, int strict,
		     char **errorstr, vbi_bool trace)
{
	struct v4l2_standard vstd;
	char *guess = "";
	vbi_capture_v4l2 *v;

	pthread_once (&vbi_init_once, vbi_init);

	assert(services && *services != 0);

	if (!(v = calloc(1, sizeof(*v)))) {
		vbi_asprintf(errorstr, _("Virtual memory exhausted."));
		errno = ENOMEM;
		return NULL;
	}

	v->do_trace = trace;
	printv("Try to open v4l2 vbi device, libzvbi interface rev.\n"
	       "%s", rcsid);

	v->capture.parameters = v4l2_parameters;
	v->capture._delete = v4l2_delete;
	v->capture.get_fd = v4l2_get_read_fd;
	v->capture.get_poll_fd = v4l2_get_poll_fd;
	v->capture.add_services = v4l2_add_services;

	/* O_RDWR required for PROT_WRITE */
	if ((v->fd = open(dev_name, O_RDWR)) == -1) {
		if ((v->fd = open(dev_name, O_RDONLY)) == -1) {
			vbi_asprintf(errorstr, _("Cannot open '%s': %d, %s."),
				     dev_name, errno, strerror(errno));
			goto io_error;
		}
	}

	printv("Opened %s\n", dev_name);

	if (IOCTL(v->fd, VIDIOC_QUERYCAP, &v->vcap) == -1) {
		vbi_asprintf(errorstr, _("Cannot identify '%s': %d, %s."),
			     dev_name, errno, strerror(errno));
		guess = _("Probably not a v4l2 device.");
/*
		goto io_error;
*/
		v4l2_delete (&v->capture);

		/* Try api revision 2002-10 */
		return vbi_capture_v4l2k_new (dev_name, -1, buffers,
					      services, strict, errorstr, trace);
	}

	if (v->vcap.type != V4L2_TYPE_VBI) {
		vbi_asprintf(errorstr, _("%s (%s) is not a raw vbi device."),
			     dev_name, v->vcap.name);
		goto failure;
	}

	printv("%s (%s) is a v4l2 vbi device\n", dev_name, v->vcap.name);

	v->p_dev_name = strdup(dev_name);

	if (v->p_dev_name == NULL) {
		vbi_asprintf(errorstr, _("Virtual memory exhausted."));
		errno = ENOMEM;
		goto failure;
	}

	v->has_select = !!(v->vcap.flags & V4L2_FLAG_SELECT);

#ifdef REQUIRE_SELECT
	if (!v->has_select) {
		vbi_asprintf(errorstr, _("%s (%s) does not support the select() function."),
			     v->p_dev_name, v->vcap.name);
		goto failure;
	}
#endif

	/* mandatory, http://www.thedirks.org/v4l2/v4l2dsi.htm */
	if (IOCTL(v->fd, VIDIOC_G_STD, &vstd) == -1) {
		vbi_asprintf(errorstr, _("Cannot query current videostandard of %s (%s): %d, %s."),
			     v->p_dev_name, v->vcap.name, errno, strerror(errno));
		guess = _("Probably a driver bug.");
		goto io_error;
	}

	printv("Current scanning system is %d\n", vstd.framelines);

	/* add_vbi_services() eliminates non 525/625 */
	v->dec.scanning = vstd.framelines;
	v->buf_req_count = buffers;


	if (v->vcap.flags & V4L2_FLAG_STREAMING) {
		printv("Using streaming interface\n");

		if (!v->has_select) {
			/* Mandatory; dequeue buffer is non-blocking. */
			vbi_asprintf(errorstr, _("%s (%s) does not support the select() function."),
				     v->p_dev_name, v->vcap.name);
			goto failure;
		}

		fcntl(v->fd, F_SETFL, O_NONBLOCK);

		v->streaming = TRUE;
		v->enqueue = ENQUEUE_SUSPENDED;

		v->capture.read = v4l2_stream;
		v->capture.flush = v4l2_stream_flush;

	} else if (v->vcap.flags & V4L2_FLAG_READ) {
		printv("Using read interface\n");

		if (!v->has_select)
			printv("Warning: no read select, reading will block\n");

		v->capture.read = v4l2_read;
		v->capture.flush = v4l2_read_flush;

                v->read_active = FALSE;

	} else {
		vbi_asprintf(errorstr, _("%s (%s) lacks a vbi read interface, "
				       "possibly an output only device or a driver bug."),
			     v->p_dev_name, v->vcap.name);
		goto failure;
	}

	*services = v4l2_add_services(&v->capture, FALSE, TRUE,
		                      *services, strict, errorstr);
	if (*services == 0)
		goto failure;

	printv("Successful opened %s (%s)\n", v->p_dev_name, v->vcap.name);

	return &v->capture;

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
vbi_capture_v4l2_new(const char *dev_name, int buffers,
		     unsigned int *services, int strict,
		     char **errorstr, vbi_bool trace)
{
	pthread_once (&vbi_init_once, vbi_init);
	vbi_asprintf(errorstr, _("V4L2 interface not compiled."));
	return NULL;
}

#endif /* !ENABLE_V4L2 */
