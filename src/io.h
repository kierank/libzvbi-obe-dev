/*
 *  libzvbi - Device interfaces
 *
 *  Copyright (C) 2002 Michael H. Schimek
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

/* $Id: io.h,v 1.13 2004/06/18 14:13:17 mschimek Exp $ */

#ifndef IO_H
#define IO_H

#include "decoder.h"

/* Public */

#include <sys/time.h> /* struct timeval */

/**
 * @ingroup Device
 */
typedef struct vbi_capture_buffer {
	void *			data;
	int			size;
	double			timestamp;
} vbi_capture_buffer;

/**
 * @ingroup Device
 * @brief Opaque device interface handle.
 **/
typedef struct vbi_capture vbi_capture;

/**
 * @addtogroup Device
 * @{
 */
extern vbi_capture *	vbi_capture_v4l2_new(const char *dev_name, int buffers,
					     unsigned int *services, int strict,
					     char **errorstr, vbi_bool trace);
extern vbi_capture *	vbi_capture_v4l2k_new(const char *	dev_name,
					      int		fd,
					      int		buffers,
					      unsigned int *	services,
					      int		strict,
					      char **		errorstr,
					      vbi_bool		trace);
extern vbi_capture *	vbi_capture_v4l_new(const char *dev_name, int scanning,
					    unsigned int *services, int strict,
					    char **errorstr, vbi_bool trace);
extern vbi_capture *	vbi_capture_v4l_sidecar_new(const char *dev_name, int given_fd,
						    unsigned int *services,
						    int strict, char **errorstr, 
						    vbi_bool trace);
extern vbi_capture *	vbi_capture_bktr_new (const char *	dev_name,
					      int		scanning,
					      unsigned int *	services,
					      int		strict,
					      char **		errstr,
					      vbi_bool		trace);
extern int		vbi_capture_dvb_filter(vbi_capture *cap, int pid);
extern vbi_capture*	vbi_capture_dvb_new(char *dev, int scanning,
				        unsigned int *services, int strict,
				        char **errstr, vbi_bool trace);

extern int		vbi_capture_read_raw(vbi_capture *capture, void *data,
					     double *timestamp, struct timeval *timeout);
extern int		vbi_capture_read_sliced(vbi_capture *capture, vbi_sliced *data, int *lines,
						double *timestamp, struct timeval *timeout);
extern int		vbi_capture_read(vbi_capture *capture, void *raw_data,
					 vbi_sliced *sliced_data, int *lines,
					 double *timestamp, struct timeval *timeout);
extern int		vbi_capture_pull_raw(vbi_capture *capture, vbi_capture_buffer **buffer,
					     struct timeval *timeout);
extern int		vbi_capture_pull_sliced(vbi_capture *capture, vbi_capture_buffer **buffer,
						struct timeval *timeout);
extern int		vbi_capture_pull(vbi_capture *capture, vbi_capture_buffer **raw_buffer,
					 vbi_capture_buffer **sliced_buffer, struct timeval *timeout);
extern vbi_raw_decoder *vbi_capture_parameters(vbi_capture *capture);
extern int		vbi_capture_fd(vbi_capture *capture);

extern void		vbi_capture_delete(vbi_capture *capture);
/** @} */

/* Private */

#ifndef DOXYGEN_SHOULD_IGNORE_THIS

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <fcntl.h>		/* open() */

extern const char _zvbi_intl_domainname[];

#ifndef _
#  ifdef ENABLE_NLS
#    include <libintl.h>
#    define _(String) dgettext (_zvbi_intl_domainname, String)
#    ifdef gettext_noop
#      define N_(String) gettext_noop (String)
#    else
#      define N_(String) (String)
#    endif
#  else /* Stubs that do something close enough.  */
#    define gettext(Msgid) ((const char *) (Msgid))
#    define dgettext(Domainname, Msgid) ((const char *) (Msgid))
#    define dcgettext(Domainname, Msgid, Category) ((const char *) (Msgid))
#    define ngettext(Msgid1, Msgid2, N) \
       ((N) == 1 ? (const char *) (Msgid1) : (const char *) (Msgid2))
#    define dngettext(Domainname, Msgid1, Msgid2, N) \
       ((N) == 1 ? (const char *) (Msgid1) : (const char *) (Msgid2))
#    define dcngettext(Domainname, Msgid1, Msgid2, N, Category) \
       ((N) == 1 ? (const char *) (Msgid1) : (const char *) (Msgid2))
#    define textdomain(Domainname) ((const char *) (Domainname))
#    define bindtextdomain(Domainname, Dirname) ((const char *) (Dirname))
#    define bind_textdomain_codeset(Domainname, Codeset) ((const char *) (Codeset))
#    define _(String) (String)
#    define N_(String) (String)
#  endif
#endif

#if defined (_IOC_SIZE) /* Linux */

#define IOCTL_ARG_SIZE(cmd)	_IOC_SIZE (cmd)
#define IOCTL_READ(cmd)		(_IOC_DIR (cmd) & _IOC_READ)
#define IOCTL_WRITE(cmd)	(_IOC_DIR (cmd) & _IOC_WRITE)
#define IOCTL_READ_WRITE(cmd)	(_IOC_DIR (cmd) == (_IOC_READ | _IOC_WRITE))
#define IOCTL_NUMBER(cmd)	_IOC_NR (cmd)

#elif defined (IOCPARM_LEN) /* FreeBSD */

#define IOCTL_ARG_SIZE(cmd)	IOCPARM_LEN (cmd)
#define IOCTL_READ(cmd)		((cmd) & IOC_OUT)
#define IOCTL_WRITE(cmd)	((cmd) & IOC_IN)
#define IOCTL_READ_WRITE(cmd)	(((cmd) & IOC_DIRMASK) == (IOC_IN | IOC_OUT))
#define IOCTL_NUMBER(cmd)	((cmd) & 0xFF)

#else /* Don't worry, only used for debugging */

#define IOCTL_ARG_SIZE(cmd)	0
#define IOCTL_READ(cmd)		0
#define IOCTL_WRITE(cmd)	0
#define IOCTL_READ_WRITE(cmd)	0
#define IOCTL_NUMBER(cmd)	0

#endif

typedef void
ioctl_log_fn			(FILE *			fp,
				 unsigned int		cmd,
				 int			rw,
				 void *			arg);
extern void
fprint_symbolic			(FILE *			fp,
				 int			mode,
				 unsigned long		value,
				 ...);
extern void
fprint_unknown_ioctl		(FILE *			fp,
				 unsigned int		cmd,
				 void *			arg);
extern int
device_open			(FILE *			fp,
				 const char *		pathname,
				 int			flags,
				 mode_t			mode);
extern int
device_close			(FILE *			fp,
				 int			fd);
extern int
device_ioctl			(FILE *			fp,
				 ioctl_log_fn *		fn,
				 int			fd,
				 unsigned int		cmd,
				 void *			arg);

extern void
vbi_capture_io_update_timeout	(const struct timeval *	tv_start,
				 struct timeval *	timeout);
extern int
vbi_capture_io_select( int fd, struct timeval * timeout );

extern void
vbi_capture_set_log_fp		(vbi_capture *		capture,
				 FILE *			fp);

/* Preliminary hack for tests. */
extern vbi_bool vbi_capture_force_read_mode;

#endif /* !DOXYGEN_SHOULD_IGNORE_THIS */

/**
 * @ingroup Devmod
 */
struct vbi_capture {
	vbi_bool		(* read)(vbi_capture *, vbi_capture_buffer **,
					 vbi_capture_buffer **, struct timeval *);
	vbi_raw_decoder *	(* parameters)(vbi_capture *);
	int			(* get_fd)(vbi_capture *);
	void			(* _delete)(vbi_capture *);

	/* Log all system calls if non-NULL. */
	FILE *			sys_log_fp;
};

#endif /* IO_H */
