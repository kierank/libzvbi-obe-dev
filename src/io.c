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

/* $Id: io.c,v 1.7 2003/06/01 19:35:40 tomzo Exp $ */

#include <assert.h>

#include "io.h"

/**
 * @addtogroup Device Device interface
 * @ingroup Raw
 * @brief Access to VBI capture devices.
 */

/**
 * @param capture Initialized vbi_capture context.
 * @param data Store the raw vbi data here. Use vbi_capture_parameters() to
 *   determine the buffer size.
 * @param timestamp On success the capture instant in seconds and fractions
 *   since 1970-01-01 00:00 of the video frame will be stored here.
 * @param timeout Wait timeout, will be read only.
 * 
 * Read a raw vbi frame from the capture device.
 * 
 * @return
 * -1 on error, examine @c errno for details. The function also fails if
 * vbi data is not available in raw format. 0 on timeout, 1 on success.
 */
int
vbi_capture_read_raw(vbi_capture *capture, void *data,
		     double *timestamp, struct timeval *timeout)
{
	vbi_capture_buffer buffer, *bp = &buffer;
	int r;

	assert (capture != NULL);
	assert (timestamp != NULL);
	assert (timeout != NULL);

	buffer.data = data;

	if ((r = capture->read(capture, &bp, NULL, timeout)) > 0)
		*timestamp = buffer.timestamp;

	return r;
}

/**
 * @param capture Initialized vbi capture context.
 * @param data Stores the sliced vbi data here. Use vbi_capture_parameters() to
 *   determine the buffer size.
 * @param lines Stores number of vbi lines decoded and stored in @a data,
 *   which can be zero, here.
 * @param timestamp On success the capture instant in seconds and fractions
 *   since 1970-01-01 00:00 will be stored here.
 * @param timeout Wait timeout, will be read only.
 * 
 * Read a sliced vbi frame, that is an array of vbi_sliced structures,
 * from the capture device. 
 *
 * Note: it's generally more efficient to use vbi_capture_pull_sliced()
 * instead, as that one may avoid having to copy sliced data into the
 * given buffer (e.g. for the VBI proxy)
 * 
 * @return
 * -1 on error, examine @c errno for details. 0 on timeout, 1 on success.
 */
int
vbi_capture_read_sliced(vbi_capture *capture, vbi_sliced *data, int *lines,
			double *timestamp, struct timeval *timeout)
{
	vbi_capture_buffer buffer, *bp = &buffer;
	int r;

	assert (capture != NULL);
	assert (lines != NULL);
	assert (timestamp != NULL);
	assert (timeout != NULL);

	buffer.data = data;

	if ((r = capture->read(capture, NULL, &bp, timeout)) > 0) {
		*lines = ((unsigned int) buffer.size) / sizeof(vbi_sliced);
		*timestamp = buffer.timestamp;
	}

	return r;
}

/**
 * @param capture Initialized vbi capture context.
 * @param raw_data Stores the raw vbi data here. Use vbi_capture_parameters() to
 *   determine the buffer size.
 * @param sliced_data Stores the sliced vbi data here. Use vbi_capture_parameters() to
 *   determine the buffer size.
 * @param lines Stores number of vbi lines decoded and stored in @a data,
 *   which can be zero, here.
 * @param timestamp On success the capture instant in seconds and fractions
 *   since 1970-01-01 00:00 will be stored here.
 * @param timeout Wait timeout, will be read only.
 * 
 * Read a raw vbi frame from the capture device, decode to sliced data
 * and also read the sliced vbi frame, that is an array of vbi_sliced
 * structures, from the capture device.
 *
 * Note: depending on the driver, captured raw data may have to be copied
 * from the capture buffer into the given buffer (e.g. for v4l2 streams which
 * use memory mapped buffers.)  It's generally more efficient to use one of
 * the vbi_capture_pull() interfaces, especially if you don't require access
 * to raw data at all.
 * 
 * @return
 * -1 on error, examine @c errno for details. The function also fails if vbi data
 * is not available in raw format. 0 on timeout, 1 on success.
 */
int
vbi_capture_read(vbi_capture *capture, void *raw_data,
		 vbi_sliced *sliced_data, int *lines,
		 double *timestamp, struct timeval *timeout)
{
	vbi_capture_buffer rbuffer, *rbp = &rbuffer;
	vbi_capture_buffer sbuffer, *sbp = &sbuffer;
	int r;

	assert (capture != NULL);
	assert (lines != NULL);
	assert (timestamp != NULL);
	assert (timeout != NULL);

	rbuffer.data = raw_data;
	sbuffer.data = sliced_data;

	if ((r = capture->read(capture, &rbp, &sbp, timeout)) > 0) {
		*lines = ((unsigned int) sbuffer.size) / sizeof(vbi_sliced);
		*timestamp = sbuffer.timestamp;
	}

	return r;
}

/**
 * @param capture Initialized vbi capture context.
 * @param buffer Store pointer to a vbi_capture_buffer here.
 * @param timeout Wait timeout, will be read only.
 * 
 * Read a raw vbi frame from the capture device, returning a
 * pointer to the image in @a buffer->data, which has @a buffer->size.
 * The data remains valid until the next
 * vbi_capture_pull_raw() call and must be read only.
 * 
 * @return
 * -1 on error, examine @c errno for details. The function also fails
 * if vbi data is not available in raw format. 0 on timeout, 1 on success.
 */
int
vbi_capture_pull_raw(vbi_capture *capture, vbi_capture_buffer **buffer,
		     struct timeval *timeout)
{
	assert (capture != NULL);
	assert (buffer != NULL);
	assert (timeout != NULL);

	*buffer = NULL;

	return capture->read(capture, buffer, NULL, timeout);
}

/**
 * @param capture Initialized vbi capture context.
 * @param buffer Store pointer to a vbi_capture_buffer here.
 * @param timeout Wait timeout, will be read only.
 * 
 * Read a sliced vbi frame, that is an array of vbi_sliced,
 * from the capture device, returning a pointer to the array as @a buffer->data.
 * @a buffer->size is the size of the array, that is the number of lines decoded,
 * which can be zero, <u>times the size of structure vbi_sliced</u>. The data
 * remains valid until the next vbi_capture_pull_sliced() call and must be read only.
 * 
 * @return
 * -1 on error, examine @c errno for details. 0 on timeout, 1 on success.
 */
int
vbi_capture_pull_sliced(vbi_capture *capture, vbi_capture_buffer **buffer,
			struct timeval *timeout)
{
	assert (capture != NULL);
	assert (buffer != NULL);
	assert (timeout != NULL);

	*buffer = NULL;

	return capture->read(capture, NULL, buffer, timeout);
}

/**
 * @param capture Initialized vbi capture context.
 * @param raw_buffer Store pointer to a vbi_capture_buffer here.
 * @param sliced_buffer Store pointer to a vbi_capture_buffer here.
 * @param timeout Wait timeout, will be read only.
 * 
 * Read a raw vbi frame from the capture device and decode to sliced
 * data. Both raw and sliced data is returned, a pointer to the raw image
 * as raw_buffer->data and a pointer to an array of vbi_sliced as
 * sliced_buffer->data. Note sliced_buffer->size is the size of the array
 * in bytes. That is the number of lines decoded, which can be zero,
 * times the size of the vbi_sliced structure.
 *
 * The raw and sliced data remains valid
 * until the next vbi_capture_pull() call and must be read only.
 * 
 * @return
 * -1 on error, examine @c errno for details. The function also fails if vbi data
 * is not available in raw format. 0 on timeout, 1 on success.
 */
int
vbi_capture_pull(vbi_capture *capture, vbi_capture_buffer **raw_buffer,
		 vbi_capture_buffer **sliced_buffer, struct timeval *timeout)
{
	assert (capture != NULL);
	assert (timeout != NULL);

	if (raw_buffer)
		*raw_buffer = NULL;
	if (sliced_buffer)
		*sliced_buffer = NULL;

	return capture->read(capture, raw_buffer, sliced_buffer, timeout);
}

/**
 * @param capture Initialized vbi capture context.
 * 
 * Describe the captured data. Raw vbi frames consist of
 * vbi_raw_decoder.count[0] + vbi_raw_decoder.count[1] lines in
 * vbi_raw_decoder.sampling_format, each vbi_raw_decoder.bytes_per_line.
 * Sliced vbi arrays consist of zero to
 * vbi_raw_decoder.count[0] + vbi_raw_decoder.count[1] vbi_sliced
 * structures.
 * 
 * @return
 * Pointer to a vbi_raw_decoder structure, read only.
 **/
vbi_raw_decoder *
vbi_capture_parameters(vbi_capture *capture)
{
	assert (capture != NULL);

	return capture->parameters(capture);
}

/**
 * @param capture Initialized vbi capture context.
 * @param reset @c TRUE to clear all previous services before adding
 *   new ones (by invoking vbi_raw_decoder_reset() at the appripriate
 *   time.)
 * @param commit @c TRUE to apply all previously added services to
 *   the device; when doing subsequent calls of this function,
 *   commit should be set @c TRUE for the last call.  Reading data
 *   cannot continue before changes were commited (because capturing
 *   has to be suspended to allow resizing the VBI image.)  Note this
 *   flag is ignored when using the VBI proxy.
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
 * Add one or more services to an already initialized capture context.
 * Can be used to dynamically change the set of active services.
 * Internally the function will restart parameter negotiation with the
 * VBI device driver and then call vbi_raw_decoder_add_services().
 * You may call vbi_raw_decoder_reset() before using this function
 * to rebuild your service mask from scratch.  Note that the number of
 * VBI lines may change with this call (even if a negative result is
 * returned) so you have to check the size of your buffers.
 *
 * @return
 * Bitmask of supported services among those requested (not including
 * previously added services.)
 */
unsigned int
vbi_capture_add_services(vbi_capture *capture,
			 vbi_bool reset, vbi_bool commit,
			 unsigned int services, int strict,
			 char ** errorstr)
{
	assert (capture != NULL);

	return capture->add_services(capture, reset, commit, services, strict, errorstr);
}

/**
 * @param capture Initialized vbi capture context, can be @c NULL.
 * 
 * @return
 * The file descriptor used to read from the device. If not
 * applicable (e.g. when using the proxy) or the @a capture context is
 * invalid -1 will be returned.
 */
int
vbi_capture_fd(vbi_capture *capture)
{
	if (capture)
		return capture->get_fd(capture);
	else
		return -1;
}

/**
 * @param capture Initialized vbi capture context, can be @c NULL.
 * 
 * @return
 * A file descriptor which can be used in poll or select system calls
 * to wait for VBI data.  If the device does not support this, value -1
 * will be returned.
 */
int
vbi_capture_get_poll_fd(vbi_capture *capture)
{
	if (capture)
		return capture->get_poll_fd(capture);
	else
		return -1;
}

/**
 * @param capture Initialized vbi capture context.
 * @param flags Set to VBI_CHN_FLUSH_ONLY if the caller has already
 *   sucessfully switched the channel; in this case only the VBI queue
 *   is flushed; if connected to the proxy daemon other clients are
 *   also notified.
 * @param chn_prio Priority level to play nice with other device users.
 * @param p_chn_desc Pointer to description of the channel to switch to.
 *   For analog sources this comprises: Index of TV card input channel
 *   (same as in VIDIOCSCHAN and VIDIOC_ENUMINPUT); TV frequency if the
 *   input channel is a tuner (value format is same as in VIDIOCSFREQ
 *   and VIDIOC_S_FREQUENCY); Index of TV tuner (usually zero, same as
 *   in VIDIOC_S_FREQUENCY, i.e. Hz * 16/1000.); Video image standard.
 * @param p_has_tuner Returns boolean flag: TRUE if the selected channel
 *   supports selecting a TV frequency.
 * @param p_scanning Returns the scan line count for the new video norm.
 * @param errorstr If not @c NULL this function stores a pointer to an error
 *   description here. You must free() this string when no longer needed.
 *
 * @return
 * -1 in case of error, examine @c errno and @c errorstr for details.
 */
int
vbi_capture_channel_change(vbi_capture *capture,
			   int chn_flags, int chn_prio,
			   vbi_channel_desc * p_chn_desc,
			   vbi_bool * p_has_tuner, int * p_scanning,
			   char ** errorstr)
{
	assert (capture != NULL);

	if (capture->channel_change != NULL)
		return capture->channel_change(capture, chn_flags, chn_prio,
				       	       p_chn_desc, p_has_tuner,
					       p_scanning, errorstr);
	else
		return -1;
}

/**
 * @param capture Initialized vbi capture context, can be @c NULL.
 * 
 * Free all resources associated with the @a capture context.
 */
void
vbi_capture_delete(vbi_capture *capture)
{
	if (capture)
		capture->_delete(capture);
}
