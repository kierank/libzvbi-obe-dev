/*
 *  libzvbi - V4L2 (version 2002-10) interface
 *
 *  Copyright (C) 2002-2003 Michael H. Schimek
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

static char rcsid[] = "$Id: io-v4l2k.c,v 1.13 2003/10/14 20:19:59 mschimek Exp $";

/*
 *  Around Oct-Nov 2002 the V4L2 API was revised for inclusion into
 *  Linux 2.5/2.6/3.0. There are a few subtle differences, in order to
 *  keep the source clean this interface has been forked off from the
 *  old V4L2 interface. "v4l2k" is no official designation, there is
 *  none, take it as v4l2-kernel or v4l-2000.
 */

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

#include "videodev2k.h"

/*#define USE_V4L2K_CHNPRIO*/ /* XXX enable for patched drivers only */
#ifdef USE_V4L2K_CHNPRIO
/* XXX note: the following should go into videodev2.h */
#define BASE_VIDIOCPRIVATE	192		/* 192-255 are private */
#define VIDIOC_G_CHNPRIO        _IOR('V' , BASE_VIDIOCPRIVATE+10, int)
#define VIDIOC_S_CHNPRIO        _IOW('V' , BASE_VIDIOCPRIVATE+11, int)
#endif

/* same as ioctl(), but repeat if interrupted */
#define IOCTL(fd, cmd, data)						\
({ int __result; do __result = ioctl(fd, cmd, data);			\
   while (__result == -1L && errno == EINTR); __result; })

#undef REQUIRE_SELECT
#undef REQUIRE_G_FMT		/* before S_FMT */
#undef REQUIRE_S_FMT		/* else accept current format */

#define printv(format, args...)						\
do {									\
	if (v->do_trace) {							\
		fprintf(stderr, format ,##args);			\
		fflush(stderr);						\
	}								\
} while (0)

#define ENQUEUE_SUSPENDED       -3
#define ENQUEUE_STREAM_OFF      -2
#define ENQUEUE_BUFS_QUEUED     -1
#define ENQUEUE_IS_UNQUEUED(X)  ((X) >= 0)

typedef struct vbi_capture_v4l2 {
	vbi_capture		capture;

	int			fd;
	vbi_bool		close_me;
#ifdef USE_V4L2K_CHNPRIO
	int                     chn_prio;
#endif
	int			btype;			/* v4l2 stream type */
	vbi_bool		streaming;
	vbi_bool		read_active;
	vbi_bool		do_trace;
	signed char		has_try_fmt;
	int			enqueue;
	struct v4l2_capability  vcap;
	char                  * p_dev_name;

	vbi_raw_decoder		dec;

	double			time_per_frame;

	vbi_capture_buffer	*raw_buffer;
	unsigned int		num_raw_buffers;
	int			buf_req_count;

	vbi_capture_buffer	sliced_buffer;

} vbi_capture_v4l2;


static void
v4l2_stream_stop(vbi_capture_v4l2 *v)
{
	if (v->enqueue >= ENQUEUE_BUFS_QUEUED) {
		printv("Suspending stream...\n");

		if (IOCTL(v->fd, VIDIOC_STREAMOFF, &v->btype) != 0)
		        printv("VIDIOC_STREAMOFF failed: %d (%s)\n", errno, strerror(errno));
	}

	for (; v->num_raw_buffers > 0; v->num_raw_buffers--) {
		munmap(v->raw_buffer[v->num_raw_buffers - 1].data,
		       v->raw_buffer[v->num_raw_buffers - 1].size);
	}

	if (v->raw_buffer != NULL) {
	        free(v->raw_buffer);
	        v->raw_buffer = NULL;
	}

	v->enqueue = ENQUEUE_SUSPENDED;
}


static int
v4l2_stream_alloc(vbi_capture_v4l2 *v, char ** errorstr)
{
	struct v4l2_requestbuffers vrbuf;
	struct v4l2_buffer vbuf;
	char * guess;

	assert(v->enqueue == ENQUEUE_SUSPENDED);
	assert(v->raw_buffer == NULL);
	printv("Requesting streaming i/o buffers\n");

	vrbuf.type = v->btype;
	vrbuf.count = v->buf_req_count;

	if (IOCTL(v->fd, VIDIOC_REQBUFS, &vrbuf) == -1) {
		vbi_asprintf(errorstr, _("Cannot request streaming i/o buffers "
				       "from %s (%s): %d, %s."),
			     v->p_dev_name, v->vcap.card, errno, strerror(errno));
		guess = _("Possibly a driver bug.");
		goto failure;
	}

	if (vrbuf.count == 0) {
		vbi_asprintf(errorstr, _("%s (%s) granted no streaming i/o buffers, "
				       "perhaps the physical memory is exhausted."),
			     v->p_dev_name, v->vcap.card);
		goto failure;
	}

	printv("Mapping %d streaming i/o buffers\n", vrbuf.count);

	v->raw_buffer = calloc(vrbuf.count, sizeof(v->raw_buffer[0]));

	if (v->raw_buffer == NULL) {
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
				     v->num_raw_buffers, v->p_dev_name, v->vcap.card,
				     errno, strerror(errno));
			goto mmap_failure;
		}

		/* bttv 0.8.x wants PROT_WRITE */
		p = mmap(NULL, vbuf.length, PROT_READ | PROT_WRITE,
			 MAP_SHARED, v->fd, vbuf.m.offset); /* MAP_PRIVATE ? */

		if (MAP_FAILED == p)
		  p = mmap(NULL, vbuf.length, PROT_READ,
			   MAP_SHARED, v->fd, vbuf.m.offset); /* MAP_PRIVATE ? */

		if (MAP_FAILED == p) {
			if (errno == ENOMEM && v->num_raw_buffers >= 2) {
				printv("Memory mapping buffer #%d failed: %d, %s (ignored).",
				       v->num_raw_buffers, errno, strerror(errno));
				break;
			}

			vbi_asprintf(errorstr, _("Memory mapping streaming i/o buffer #%d "
					       "from %s (%s) failed: %d, %s."),
				     v->num_raw_buffers, v->p_dev_name, v->vcap.card,
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
				       "driver author.\n", v->p_dev_name, v->vcap.card);
				exit(EXIT_FAILURE);
			}
		}

		if (IOCTL(v->fd, VIDIOC_QBUF, &vbuf) == -1) {
			vbi_asprintf(errorstr, _("Cannot enqueue streaming i/o buffer #%d "
					       "to %s (%s): %d, %s."),
				     v->num_raw_buffers, v->p_dev_name, v->vcap.card,
				     errno, strerror(errno));
			guess = _("Probably a driver bug.");
			goto mmap_failure;
		}

		v->num_raw_buffers++;
	}

	v->enqueue = ENQUEUE_STREAM_OFF;

	return 0;

mmap_failure:
	v4l2_stream_stop(v);

failure:
	return -1;
}


static int
v4l2_stream(vbi_capture *vc, vbi_capture_buffer **raw,
	    vbi_capture_buffer **sliced, struct timeval *timeout)
{
	vbi_capture_v4l2 *v = PARENT(vc, vbi_capture_v4l2, capture);
	struct v4l2_buffer vbuf;
	double time;

	if (v->enqueue == ENQUEUE_SUSPENDED) {
		/* stream was suspended (add_services not committed) */
	        printv("stream-read: ERROR: streaming is suspended\n");
		errno = ESRCH;
		return -1;
	}

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

	if (IOCTL(v->fd, VIDIOC_DQBUF, &vbuf) == -1) {
	        printv("stream-read: ioctl DQBUF: %d (%s)\n", errno, strerror(errno));
		return -1;
	}

	time = vbuf.timestamp.tv_sec
		+ vbuf.timestamp.tv_usec * (1 / 1e6);

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

	/* if no raw pointer returned to the caller, re-queue buffer immediately
	** else the buffer is re-queued upon the next call to read() */
	if (v->enqueue == ENQUEUE_BUFS_QUEUED) {
		if (IOCTL(v->fd, VIDIOC_QBUF, &vbuf) == -1) {
	                printv("stream-read: ioctl QBUF: %d (%s)\n", errno, strerror(errno));
			return -1;
	        }
	}

	return 1;
}

static void v4l2_stream_flush(vbi_capture *vc)
{
	vbi_capture_v4l2 *v = PARENT(vc, vbi_capture_v4l2, capture);
	struct v4l2_buffer vbuf;
	struct timeval tv;
	fd_set fds;
	unsigned int max_loop;
	int ret;

	/* stream not enabled yet -> nothing to flush */
	if ( (v->enqueue == ENQUEUE_SUSPENDED) ||
	     (v->enqueue == ENQUEUE_STREAM_OFF) )
		return;

	if (ENQUEUE_IS_UNQUEUED(v->enqueue)) {
		vbuf.type = v->btype;
		vbuf.index = v->enqueue;

		if (IOCTL(v->fd, VIDIOC_QBUF, &vbuf) == -1) {
	                printv("stream-flush: ioctl QBUF: %d (%s)\n", errno, strerror(errno));
			return;
	        }
	}
	v->enqueue = ENQUEUE_BUFS_QUEUED;

	for (max_loop = 0; max_loop < v->num_raw_buffers; max_loop++) {

		/* check if there are any buffers pending for de-queueing */
		while (1) {
			FD_ZERO(&fds);
			FD_SET(v->fd, &fds);

			/* use zero timeout to prevent select() from blocking */
			memset(&tv, 0, sizeof(tv));

			ret = select(v->fd + 1, &fds, NULL, NULL, &tv);

			if ((ret < 0) && (errno == EINTR))
				continue;

			/* no buffers ready or an error occurred -> return */
			if (ret <= 0)
				return;

			break;
		}

		if (IOCTL(v->fd, VIDIOC_DQBUF, &vbuf) == -1)
			return;

		/* immediately queue the buffer again, thereby discarding it's content */
		if (IOCTL(v->fd, VIDIOC_QBUF, &vbuf) == -1)
			return;
	}
}


static void
v4l2_read_stop(vbi_capture_v4l2 *v)
{
	for (; v->num_raw_buffers > 0; v->num_raw_buffers--) {
		free(v->raw_buffer[v->num_raw_buffers - 1].data);
		v->raw_buffer[v->num_raw_buffers - 1].data = NULL;
	}

	free(v->raw_buffer);
	v->raw_buffer = NULL;
}


static int
v4l2_suspend(vbi_capture_v4l2 *v)
{
	int    fd;

	if (v->streaming) {
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

	while (1) {
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
	int r;
	fd_set fds;

	while (1) {
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

	r = read(v->fd, v->raw_buffer->data, v->raw_buffer->size);
}

static void
print_vfmt(const char *s, struct v4l2_format *vfmt)
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
		vfmt->fmt.vbi.start[0], vfmt->fmt.vbi.count[0],
		vfmt->fmt.vbi.start[1], vfmt->fmt.vbi.count[1],
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
	int    max_rate;
	int    g_fmt;
	int    s_fmt;
	char * guess;

	/* suspend capturing, or driver will return EBUSY */
	v4l2_suspend(v);

	if (reset)
		vbi_raw_decoder_reset(&v->dec);

	memset(&vfmt, 0, sizeof(vfmt));

	vfmt.type = v->btype = V4L2_BUF_TYPE_VBI_CAPTURE;

	max_rate = 0;

	printv("Querying current vbi parameters... ");

	if ((g_fmt = IOCTL(v->fd, VIDIOC_G_FMT, &vfmt)) == -1) {
		printv("failed\n");
#ifdef REQUIRE_G_FMT
		vbi_asprintf(errorstr, _("Cannot query current vbi parameters of %s (%s): %d, %s."),
			     v->p_dev_name, v->vcap.card, errno, strerror(errno));
		goto io_error;
#else
		strict = MAX(0, strict);
#endif
	} else {
		printv("success\n");

		if (v->do_trace)
			print_vfmt("VBI capture parameters supported: ", &vfmt);

		if (v->has_try_fmt == -1) {
			struct v4l2_format vfmt_temp = vfmt;

			/* test if TRY_FMT is available by feeding it the current
			** parameters, which should always succeed */
			v->has_try_fmt = ((IOCTL(v->fd, VIDIOC_TRY_FMT, &vfmt_temp)) == 0) ? 1 : 0;
		}
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
					       "requested data services with scanning %d."),
	                                       v->p_dev_name, v->vcap.card, v->dec.scanning);
			goto failure;
		}

	        services &= sup_services;

		vfmt.fmt.vbi.sample_format	= V4L2_PIX_FMT_GREY;
		vfmt.fmt.vbi.sampling_rate	= dec_temp.sampling_rate;
		vfmt.fmt.vbi.samples_per_line	= dec_temp.bytes_per_line;
		vfmt.fmt.vbi.offset		= dec_temp.offset;
		vfmt.fmt.vbi.start[0]		= dec_temp.start[0];
		vfmt.fmt.vbi.count[0]		= dec_temp.count[0];
		vfmt.fmt.vbi.start[1]		= dec_temp.start[1];
		vfmt.fmt.vbi.count[1]		= dec_temp.count[1];

		if (v->do_trace)
			print_vfmt("VBI capture parameters requested: ", &vfmt);

		s_fmt = ((v->has_try_fmt != 1) || commit) ? VIDIOC_S_FMT : VIDIOC_TRY_FMT;
		if (IOCTL(v->fd, s_fmt, &vfmt) == -1) {
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
					     v->p_dev_name, v->vcap.card);
				goto failure;

			default:
				vbi_asprintf(errorstr, _("Could not set the vbi capture parameters "
						       "for %s (%s): %d, %s."),
					     v->p_dev_name, v->vcap.card, errno, strerror(errno));
				guess = _("Possibly a driver bug.");
				goto io_error;
			}

			if (commit && (v->has_try_fmt == 1) && (v->dec.services != 0)) {
				/* FIXME strictness of services is not considered */
				unsigned int tmp_services =
					vbi_raw_decoder_check_services(&v->dec, v->dec.services, 0);
				if (v->dec.services != tmp_services)
					vbi_raw_decoder_remove_services(&v->dec, v->dec.services & ~ tmp_services);
			}

		} else {
			printv("Successfully %s vbi capture parameters\n",
	                       ((s_fmt == (int)VIDIOC_S_FMT) ? "set" : "tried"));
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
	v->dec.start[0] 		= vfmt.fmt.vbi.start[0];
	v->dec.count[0] 		= vfmt.fmt.vbi.count[0];
	v->dec.start[1] 		= vfmt.fmt.vbi.start[1];
	v->dec.count[1] 		= vfmt.fmt.vbi.count[1];
	v->dec.interlaced		= !!(vfmt.fmt.vbi.flags & V4L2_VBI_INTERLACED);
	v->dec.synchronous		= !(vfmt.fmt.vbi.flags & V4L2_VBI_UNSYNC);
	v->time_per_frame 		= (v->dec.scanning == 625) ? 1.0 / 25 : 1001.0 / 30000;
	v->dec.sampling_format          = VBI_PIXFMT_YUV420;

 	if (vfmt.fmt.vbi.sample_format != V4L2_PIX_FMT_GREY) {
		vbi_asprintf(errorstr, _("%s (%s) offers unknown vbi sampling format #%d. "
				       "This may be a driver bug or libzvbi is too old."),
			     v->p_dev_name, v->vcap.card, vfmt.fmt.vbi.sample_format);
		goto failure;
	}

	if (services & ~(VBI_SLICED_VBI_525 | VBI_SLICED_VBI_625)) {
		/* Nyquist (we're generous at 1.5) */

		if (v->dec.sampling_rate < max_rate * 3 / 2) {
			vbi_asprintf(errorstr, _("Cannot capture the requested "
						 "data services with "
						 "%s (%s), the sampling frequency "
						 "%.2f MHz is too low."),
				     v->p_dev_name, v->vcap.card,
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
				     v->p_dev_name, v->vcap.card);
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

	if (v->close_me && v->fd != -1)
		close(v->fd);

	free(v);
}

static int
v4l2_get_fd(vbi_capture *vc)
{
	vbi_capture_v4l2 *v = PARENT(vc, vbi_capture_v4l2, capture);

	/* v4l2 devices always support select (since 2002-10 revision),
	** hence no separate get_fd() function is required for get_poll_fd queries */

	return v->fd;
}

static int
v4l2_channel_change(vbi_capture *vc,
		    int chn_flags, int chn_prio,
		    vbi_channel_desc * p_chn_desc,
		    vbi_bool * p_has_tuner, int * p_scanning,
		    char ** errorstr)
{
	vbi_capture_v4l2 *v = PARENT(vc, vbi_capture_v4l2, capture);
	struct v4l2_frequency vfreq;
	v4l2_std_id mode;
	int  old_channel = -1;

#ifdef USE_V4L2K_CHNPRIO
	if (chn_prio != v->chn_prio) {
		v->chn_prio = chn_prio;
		if (ioctl(v->fd, VIDIOC_S_CHNPRIO, &v->chn_prio) != 0)
			printv("Failed to set register channel prio: %d (%s)\n",
			       errno, strerror(errno));
	}
#endif

	if (chn_flags & VBI_CHN_FLUSH_ONLY)
		goto done;

	if (p_chn_desc->type != 0) {
		vbi_asprintf(errorstr, _("Not an analog channel descriptor type"));
		goto failure;
	}

	/* convert video mode/norm ID to v4l1 */
	if (p_chn_desc->u.analog.mode_std != -1) {
		mode = p_chn_desc->u.analog.mode_std;
	}
	else if (p_chn_desc->u.analog.mode_color != -1) {
		if (p_chn_desc->u.analog.mode_color == 0)
			mode = V4L2_STD_PAL;
		else if (p_chn_desc->u.analog.mode_color == 1)
			mode = V4L2_STD_NTSC;
		else if (p_chn_desc->u.analog.mode_color == 2)
			mode = V4L2_STD_SECAM;
		else
			mode = -1;
	}
	else
		mode = -1;

	if (mode != -1) {
		if (IOCTL(v->fd, VIDIOC_S_STD, &mode) != 0) {
			printv("ioctl S_STD(0x%X) failed: %d (%s)\n", (int)mode, errno, strerror(errno));
			vbi_asprintf(errorstr, _("Failed to switch video standard"));
			goto failure;
		}
	}

	if (p_chn_desc->u.analog.channel != -1) {
		/* query old channel */
		if (IOCTL(v->fd, VIDIOC_G_INPUT, &old_channel) != 0) {
			printv("ioctl G_INPUT failed: %d (%s)\n", errno, strerror(errno));
			vbi_asprintf(errorstr, _("Failed to query channel #%d, "
						 "probably invalid index."),
						 p_chn_desc->u.analog.channel);
			goto failure;
		}

		/* switch input channel */
		if ( (p_chn_desc->u.analog.channel != old_channel) &&
		     (IOCTL(v->fd, VIDIOC_S_INPUT, &p_chn_desc->u.analog.channel) != 0) ) {
			printv("ioctl S_INPUT failed for channel #%d: %d (%s)\n",
			       p_chn_desc->u.analog.channel, errno, strerror(errno));
			vbi_asprintf(errorstr, _("Failed to switch to channel #%d."),
			             p_chn_desc->u.analog.channel);
			goto failure;
		}
	}

	/* set tuner parameters */
	if (p_chn_desc->u.analog.freq != -1) {
		memset(&vfreq, 0, sizeof(vfreq));
		vfreq.tuner      = p_chn_desc->u.analog.tuner;
		vfreq.frequency  = p_chn_desc->u.analog.freq;
		vfreq.type       = V4L2_TUNER_ANALOG_TV;
		if (IOCTL(v->fd, VIDIOC_S_FREQUENCY, &vfreq) != 0) {
			printv("ioctl S_FREQUENCY tuner=%d freq=%d failed: %d (%s)\n",
			       p_chn_desc->u.analog.tuner, p_chn_desc->u.analog.freq,
			       errno, strerror(errno));
			vbi_asprintf(errorstr, _("Failed to set TV tuner frequency."));

			if (old_channel != -1) {
				/* attempt to set old channel again */
				if (IOCTL(v->fd, VIDIOC_S_INPUT, (int *) &old_channel) != 0)
					printv("ioctl S_INPUT failed to switch to prev. channel #%d: %d (%s)\n",
					       old_channel, errno, strerror(errno));
			}
			goto failure;
		}
	}

	printv("Successfully switched channel and/or frequency.\n");
done:

	if (v->streaming)
		v4l2_stream_flush(vc);
	else
		v4l2_read_flush(vc);

	return 0;

failure:
	return -1;
}

static vbi_bool
v4l2_get_videostd(vbi_capture_v4l2 *v, char ** errorstr)
{
	struct v4l2_standard vstd;
	v4l2_std_id stdid;
	int r;
	char * guess;

	if (-1 == IOCTL(v->fd, VIDIOC_G_STD, &stdid)) {
		vbi_asprintf(errorstr, _("Cannot query current videostandard of %s (%s): %d, %s."),
			     v->p_dev_name, v->vcap.card, errno, strerror(errno));
		guess = _("Probably a driver bug.");
		return FALSE;
	}

	vstd.index = 0;

	while (0 == (r = IOCTL(v->fd, VIDIOC_ENUMSTD, &vstd))) {
		if (vstd.id & stdid)
			break;
		vstd.index++;
	}

	if (-1 == r) {
		vbi_asprintf(errorstr, _("Cannot query current videostandard of %s (%s): %d, %s."),
			     v->p_dev_name, v->vcap.card, errno, strerror(errno));
		guess = _("Probably a driver bug.");
		return FALSE;
	}

	printv ("Current scanning system is %d\n", vstd.framelines);

	/* add_vbi_services() eliminates non 525/625 */
	v->dec.scanning = vstd.framelines;

	return TRUE;
}

/* document below */
vbi_capture *
vbi_capture_v4l2k_new		(const char *		dev_name,
				 int			fd,
				 int			buffers,
				 unsigned int *		services,
				 int			strict,
				 char **		errorstr,
				 vbi_bool		trace)
{
	char *guess;
	vbi_capture_v4l2 *v;

	pthread_once (&vbi_init_once, vbi_init);

	assert(services && *services != 0);
	assert(buffers > 0);
	assert(dev_name != NULL);

	if (!(v = calloc(1, sizeof(*v)))) {
		vbi_asprintf(errorstr, _("Virtual memory exhausted."));
		errno = ENOMEM;
		return NULL;
	}

	v->do_trace = trace;
	printv("Try to open v4l2 (2002-10) vbi device, libzvbi interface rev.\n"
	       "%s\n", rcsid);

	v->p_dev_name = strdup(dev_name);

	if (v->p_dev_name == NULL) {
		vbi_asprintf(errorstr, _("Virtual memory exhausted."));
		errno = ENOMEM;
		goto failure;
	}

	v->capture.parameters = v4l2_parameters;
	v->capture._delete = v4l2_delete;
	v->capture.get_fd = v4l2_get_fd;
	v->capture.get_poll_fd = v4l2_get_fd;
	v->capture.add_services = v4l2_add_services;
	v->capture.channel_change = v4l2_channel_change;

	if (fd == -1) {
		if ((v->fd = open(v->p_dev_name, O_RDWR)) == -1) {
			vbi_asprintf(errorstr, _("Cannot open '%s': %d, %s."),
				     v->p_dev_name, errno, strerror(errno));
			goto io_error;
		}

		v->close_me = TRUE;

		printv("Opened %s\n", v->p_dev_name);
	} else {
		v->fd = fd;
		v->close_me = FALSE;

		printv("Using v4l2k device fd %d\n", fd);
	}

	if (IOCTL(v->fd, VIDIOC_QUERYCAP, &v->vcap) == -1) {
		vbi_asprintf(errorstr, _("Cannot identify '%s': %d, %s."),
			     v->p_dev_name, errno, strerror(errno));
		guess = _("Probably not a v4l2 device.");
		goto io_error;
	}

	if (!(v->vcap.capabilities & V4L2_CAP_VBI_CAPTURE)) {
		vbi_asprintf(errorstr, _("%s (%s) is not a raw vbi device."),
			     v->p_dev_name, v->vcap.card);
		goto failure;
	}

	printv("%s (%s) is a v4l2 vbi device,\n", v->p_dev_name, v->vcap.card);
	printv("driver %s, version 0x%08x\n", v->vcap.driver, v->vcap.version);

	v->has_try_fmt = -1;
	v->buf_req_count = buffers;

	if (v4l2_get_videostd(v, errorstr) == FALSE)
		goto io_error;

	if (v->vcap.capabilities & V4L2_CAP_STREAMING) {
		printv("Using streaming interface\n");

		fcntl(v->fd, F_SETFL, O_NONBLOCK);

		v->streaming = TRUE;
		v->enqueue = ENQUEUE_SUSPENDED;

		v->capture.read = v4l2_stream;

	} else if (v->vcap.capabilities & V4L2_CAP_READWRITE) {
		printv("Using read interface\n");

		v->capture.read = v4l2_read;

	        v->read_active = FALSE;

	} else {
		vbi_asprintf(errorstr, _("%s (%s) lacks a vbi read interface, "
				       "possibly an output only device or a driver bug."),
			     v->p_dev_name, v->vcap.card);
		goto failure;
	}

	*services = v4l2_add_services(&v->capture, FALSE, TRUE,
		                      *services, strict, errorstr);
	if (*services == 0)
		goto failure;

#ifdef USE_V4L2K_CHNPRIO
	if (v->close_me) {
		/* channel priority is lowered to "background" by default */
		v->chn_prio = 0;
		if (ioctl(v->fd, VIDIOC_S_CHNPRIO, &v->chn_prio) != 0)
			printv("Failed to set register channel prio: %d (%s)\n", errno, strerror(errno));
	}
#endif

	printv("Successful opened %s (%s)\n",
	       v->p_dev_name, v->vcap.card);

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
 * @param fd File handle of VBI device if already opened by caller,
 *   else value -1.
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
vbi_capture_v4l2k_new(const char *dev_name, int fd, int buffers,
		      unsigned int *services, int strict,
		      char **errorstr, vbi_bool trace)
{
	pthread_once (&vbi_init_once, vbi_init);
	vbi_asprintf(errorstr, _("V4L2 interface not compiled."));
	return NULL;
}

#endif /* !ENABLE_V4L2 */
