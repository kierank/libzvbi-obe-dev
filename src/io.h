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

/* $Id: io.h,v 1.10 2003/06/01 19:35:40 tomzo Exp $ */

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
 * @ingroup Device
 * @brief TV input channel description for channel changes.
 *
 * This structure is passed to the channel switch function. Any of
 * the elements in the union can be set to -1 if they shall remain
 * unchanged.  Currently only analog channel switching is supported,
 * i.e. type 0.
 *
 * The mode elements describe the color encoding or video norm; only
 * one of them can be used. mode_std is a v4l2_std_id descriptor, see
 * videodev2.h (2002 revision). mode_color is a v4l1 mode descriptor,
 * i.e. 0=PAL, 1=NTSC, 2=SECAM, 3=AUTO. modes are converted as required
 * for v4l1 or v4l2 devices.
 */
typedef struct {
	int				type;
	union {
		struct {
			int 		channel;
			int 		freq;
			int 		tuner;
			int 		mode_color;
			long long 	mode_std;
		} analog;
		struct {
			char		reserved[64];
		} x;
	} u;
} vbi_channel_desc;

/**
 * @ingroup Device
 * @brief Flags for channel switching.
 */
enum {
	VBI_CHN_FLUSH_ONLY = 0x01
};

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

extern vbi_capture *    vbi_capture_proxy_new(const char *dev_name,
                                              int buffers,
                                              int scanning,
                                              unsigned int *services,
                                              int strict,
                                              char **pp_errorstr,
                                              vbi_bool trace);

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
extern int		vbi_capture_get_poll_fd(vbi_capture *capture);
extern unsigned int     vbi_capture_add_services(vbi_capture *capture,
                                                 vbi_bool reset, vbi_bool commit,
                                                 unsigned int services, int strict,
                                                 char ** errorstr);
extern int		vbi_capture_channel_change(vbi_capture *capture,
						   int chn_flags, int chn_prio,
						   vbi_channel_desc * p_chn_desc,
						   vbi_bool * p_has_tuner, int * p_scanning,
						   char ** errorstr);
extern void		vbi_capture_delete(vbi_capture *capture);
/** @} */

/* Private */

#ifndef DOXYGEN_SHOULD_IGNORE_THIS

#include <stdarg.h>
#include <stddef.h>

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

#endif /* !DOXYGEN_SHOULD_IGNORE_THIS */

/**
 * @ingroup Devmod
 */
struct vbi_capture {
	vbi_bool		(* read)(vbi_capture *, vbi_capture_buffer **,
					 vbi_capture_buffer **, struct timeval *);
	vbi_raw_decoder *	(* parameters)(vbi_capture *);
        unsigned int            (* add_services)(vbi_capture *vc,
                                         vbi_bool reset, vbi_bool commit,
                                         unsigned int services, int strict,
                                         char ** errorstr);
	int			(* channel_change)(vbi_capture *vc,
					 int chn_flags, int chn_prio,
					 vbi_channel_desc * p_chn_desc,
					 vbi_bool * p_has_tuner, int * p_scanning,
					 char ** errorstr);
	int			(* get_fd)(vbi_capture *);
	int			(* get_poll_fd)(vbi_capture *);
	void			(* _delete)(vbi_capture *);
};

#endif /* IO_H */
