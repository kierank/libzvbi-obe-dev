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

/* $Id: io.c,v 1.2 2002/01/21 07:34:59 mschimek Exp $ */

#include <assert.h>

#include "io.h"

/**
 * vbi_capture_read_raw:
 * @capture: Initialized vbi capture context.
 * @data: Store the raw vbi data here. Use vbi_capture_parameters() to
 *   determine the buffer size.
 * @timestamp: On success the capture instant in seconds and fractions
 *   since 1970-01-01 00:00 will be stored here.
 * @timeout: Wait timeout, will be read only.
 * 
 * Read a raw vbi frame from the capture device.
 * 
 * Return value:
 * -1 on error, examine errno for details. The function also fails if vbi data
 * is not available in raw format. 0 on timeout, 1 on success.
 **/
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
 * vbi_capture_read_sliced:
 * @capture: Initialized vbi capture context.
 * @data: Store the sliced vbi data here. Use vbi_capture_parameters() to
 *   determine the buffer size.
 * @lines: Store number of vbi lines decoded and stored in @data,
 *   which can be zero, here.
 * @timestamp: On success the capture instant in seconds and fractions
 *   since 1970-01-01 00:00 will be stored here.
 * @timeout: Wait timeout, will be read only.
 * 
 * Read a sliced vbi frame, that is an array of #vbi_sliced,
 * from the capture device. 
 * 
 * Return value: 
 * -1 on error, examine errno for details. 0 on timeout, 1 on success.
 **/
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
 * vbi_capture_read:
 * @capture: Initialized vbi capture context.
 * @raw_data: Store the raw vbi data here. Use vbi_capture_parameters() to
 *   determine the buffer size.
 * @sliced_data: Store the sliced vbi data here. Use vbi_capture_parameters() to
 *   determine the buffer size.
 * @lines: Store number of vbi lines decoded and stored in @data,
 *   which can be zero, here.
 * @timestamp: On success the capture instant in seconds and fractions
 *   since 1970-01-01 00:00 will be stored here.
 * @timeout: Wait timeout, will be read only.
 * 
 * Read a raw vbi frame from the capture device, decode to sliced data
 * and also read the sliced vbi frame, that is an array of #vbi_sliced,
 * from the capture device.
 * 
 * Return value:
 * -1 on error, examine errno for details. The function also fails if vbi data
 * is not available in raw format. 0 on timeout, 1 on success.
 **/
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
 * vbi_capture_pull_raw:
 * @capture: Initialized vbi capture context.
 * @buffer: Store pointer to a #vbi_capture_buffer here.
 * @timeout: Wait timeout, will be read only.
 * 
 * Read a raw vbi frame from the capture device, returning a
 * pointer to the image. This data remains valid until the next
 * vbi_capture_pull_raw() call and must be read only.
 * 
 * Return value: 
 * -1 on error, examine errno for details. The function also fails if vbi data
 * is not available in raw format. 0 on timeout, 1 on success.
 **/
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
 * vbi_capture_pull_sliced:
 * @capture: Initialized vbi capture context.
 * @buffer: Store pointer to a #vbi_capture_buffer here.
 * @timeout: Wait timeout, will be read only.
 * 
 * Read a sliced vbi frame, that is an array of #vbi_sliced,
 * from the capture device, returning a pointer to the array buffer->data.
 * Note buffer->size is lines decoded, which can be zero, times the size of
 * #vbi_sliced. This data remains valid until the next vbi_capture_pull_sliced()
 * call and must be read only.
 * 
 * Return value: 
 * -1 on error, examine errno for details. 0 on timeout, 1 on success.
 **/
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
 * vbi_capture_pull:
 * @capture: Initialized vbi capture context.
 * @raw_buffer: Store pointer to a #vbi_capture_buffer here.
 * @sliced_buffer: Store pointer to a #vbi_capture_buffer here.
 * @timeout: Wait timeout, will be read only.
 * 
 * Read a raw vbi frame from the capture device, decode to sliced data
 * and also read the sliced vbi frame, that is an array of #vbi_sliced,
 * from the capture device, returning pointers to the image raw_buffer->data
 * and array sliced_buffer->data. Note sliced_buffer->size is lines decoded,
 * which can be zero, times the size of #vbi_sliced. This data remains valid
 * until the next vbi_capture_pull_raw() call and must be read only.
 * 
 * Return value: 
 * -1 on error, examine errno for details. The function also fails if vbi data
 * is not available in raw format. 0 on timeout, 1 on success.
 **/
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
 * vbi_capture_parameters:
 * @capture: Initialized vbi capture context.
 * 
 * Describe the captured data. Raw vbi frames consist of
 * vbi_raw_decoder.count[0] + vbi_raw_decoder.count[1] lines in
 * vbi_raw_decoder.sampling_format, each vbi_raw_decoder.bytes_per_line.
 * Sliced vbi arrays consist of at most
 * vbi_raw_decoder.count[0] + vbi_raw_decoder.count[1] #vbi_sliced
 * structures.
 * 
 * Return value: 
 * Pointer to a #vbi_raw_decoder structure, read only.
 **/
vbi_raw_decoder *
vbi_capture_parameters(vbi_capture *capture)
{
	assert (capture != NULL);

	return capture->parameters(capture);
}

/**
 * vbi_capture_delete:
 * @capture: Initialized vbi capture context, can be %NULL.
 * 
 * Free all resources associated with the capture context.
 **/
void
vbi_capture_delete(vbi_capture *capture)
{
	if (capture)
		capture->delete(capture);
}
