/*
 *  libzvbi - Raw vbi decoder
 *
 *  Copyright (C) 2000, 2001, 2002 Michael H. Schimek
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

/* $Id: decoder.h,v 1.1 2002/01/12 16:18:36 mschimek Exp $ */

#ifndef DECODER_H
#define DECODER_H

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#include "bcd.h"
#include "sliced.h"

/* Public */

#include <pthread.h>

/* Bit slicer */

/**
 * vbi_pixfmt:
 * 
 * Raw vbi data sample format, this enumeration corresponds to
 * librte rte_pixfmt.
 * 
 * <table frame=all><title>Sample formats</title><tgroup cols=5 align=center>
 * <colspec colname=c1><colspec colname=c2><colspec colname=c3><colspec colname=c4>
 * <colspec colname=c5>
 * <spanspec spanname=desc1 namest=c1 nameend=c5 align=left>
 * <spanspec spanname=desc2 namest=c1 nameend=c5 align=left>
 * <spanspec spanname=desc3 namest=c1 nameend=c5 align=left>
 * <spanspec spanname=desc4 namest=c1 nameend=c5 align=left>
 * <spanspec spanname=desc5 namest=c1 nameend=c5 align=left>
 * <spanspec spanname=desc6 namest=c1 nameend=c5 align=left>
 * <thead>
 * <row><entry>Symbol</><entry>Byte&nbsp;0</><entry>Byte&nbsp;1</>
 * <entry>Byte&nbsp;2</><entry>Byte&nbsp;3</></row>
 * </thead><tbody>
 * <row><entry spanname=desc1>Planar YUV 4:2:0 data. Only the luminance
 *   bytes are evaluated. This is the format in which raw vbi
 *   data is usually captured by hardware.</></row>
 * <row><entry>@VBI_PIXFMT_YUV420</><entry>Y0</><entry>Y1</><entry>Y2</><entry>Y3</></row>
 * <row><entry spanname=desc1>Packed YUV 4:2:2 data. Only the luminance bytes are evaluated.</></row>
 * <row><entry>@VBI_PIXFMT_YUYV</><entry>Y0</><entry>Cb</><entry>Y1</><entry>Cr</></row>
 * <row><entry>@VBI_PIXFMT_YVYU</><entry>Y0</><entry>Cr</><entry>Y1</><entry>Cb</></row>
 * <row><entry>@VBI_PIXFMT_UYVY</><entry>Cb</><entry>Y0</><entry>Cr</><entry>Y1</></row>
 * <row><entry>@VBI_PIXFMT_VYUY</><entry>Cr</><entry>Y0</><entry>Cb</><entry>Y1</></row>
 * <row><entry spanname=desc1>Packed 32 bit RGB data. Only the g bits are evaluated.</></row>
 * <row><entry>@VBI_PIXFMT_RGBA32_LE @VBI_PIXFMT_ARGB32_BE</>
 * <entry>r7&nbsp;...&nbsp;r0</><entry>g7&nbsp;...&nbsp;g0</>
 * <entry>b7&nbsp;...&nbsp;b0</><entry>a7&nbsp;...&nbsp;a0</></row>
 * <row><entry>@VBI_PIXFMT_BGRA32_LE @VBI_PIXFMT_ARGB32_BE</>
 * <entry>b7&nbsp;...&nbsp;b0</><entry>g7&nbsp;...&nbsp;g0</>
 * <entry>r7&nbsp;...&nbsp;r0</><entry>a7&nbsp;...&nbsp;a0</></row>
 * <row><entry>@VBI_PIXFMT_ARGB32_LE @VBI_PIXFMT_BGRA32_BE</>
 * <entry>a7&nbsp;...&nbsp;a0</><entry>r7&nbsp;...&nbsp;r0</>
 * <entry>g7&nbsp;...&nbsp;g0</><entry>b7&nbsp;...&nbsp;b0</></row>
 * <row><entry>@VBI_PIXFMT_ABGR32_LE @VBI_PIXFMT_RGBA32_BE</>
 * <entry>a7&nbsp;...&nbsp;a0</><entry>b7&nbsp;...&nbsp;b0</>
 * <entry>g7&nbsp;...&nbsp;g0</><entry>r7&nbsp;...&nbsp;r0</></row>
 * <row><entry spanname=desc4>Packed 24 bit RGB data. Only the g bits are evaluated.</></row>
 * <row><entry>@VBI_PIXFMT_RGBA24</>
 * <entry>r7&nbsp;...&nbsp;r0</><entry>g7&nbsp;...&nbsp;g0</>
 * <entry>b7&nbsp;...&nbsp;b0</><entry></></row>
 * <row><entry>@VBI_PIXFMT_BGRA24</>
 * <entry>b7&nbsp;...&nbsp;b0</><entry>g7&nbsp;...&nbsp;g0</>
 * <entry>r7&nbsp;...&nbsp;r0</><entry></></row>
 * <row><entry spanname=desc5>Packed 16 bit RGB data. Only the g bits are evaluated.</></row>
 * <row><entry>@VBI_PIXFMT_RGB16_LE</>
 * <entry>g2&nbsp;g1&nbsp;g0&nbsp;r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0</>
 * <entry>b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0&nbsp;g5&nbsp;g4&nbsp;g3</>
 * <entry></><entry></></row><row><entry>@VBI_PIXFMT_BGR16_LE</>
 * <entry>g2&nbsp;g1&nbsp;g0&nbsp;b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0</>
 * <entry>r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0&nbsp;g5&nbsp;g4&nbsp;g3</>
 * <entry></><entry></></row><row><entry>@VBI_PIXFMT_RGB16_BE</>
 * <entry>b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0&nbsp;g5&nbsp;g4&nbsp;g3</>
 * <entry>g2&nbsp;g1&nbsp;g0&nbsp;r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0</>
 * <entry></><entry></></row><row><entry>@VBI_PIXFMT_BGR16_BE</>
 * <entry>r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0&nbsp;g5&nbsp;g4&nbsp;g3</>
 * <entry>g2&nbsp;g1&nbsp;g0&nbsp;b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0</>
 * <entry></><entry></></row>
 * <row><entry spanname=desc6>Packed 15 bit RGB data. Only the g bits are evaluated.</></row>
 * <row><entry>@VBI_PIXFMT_RGBA15_LE</>
 * <entry>g2&nbsp;g1&nbsp;g0&nbsp;r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0</>
 * <entry>a0&nbsp;b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0&nbsp;g4&nbsp;g3</>
 * <entry></><entry></></row><row><entry>@VBI_PIXFMT_BGRA15_LE</>
 * <entry>g2&nbsp;g1&nbsp;g0&nbsp;b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0</>
 * <entry>a0&nbsp;r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0&nbsp;g4&nbsp;g3</>
 * <entry></><entry></></row><row><entry>@VBI_PIXFMT_ARGB15_LE</>
 * <entry>g1&nbsp;g0&nbsp;r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0&nbsp;a0</>
 * <entry>b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0&nbsp;g4&nbsp;g3&nbsp;g2</>
 * <entry></><entry></></row><row><entry>@VBI_PIXFMT_ABGR15_LE</>
 * <entry>g1&nbsp;g0&nbsp;b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0&nbsp;a0</>
 * <entry>r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0&nbsp;g4&nbsp;g3&nbsp;g2</>
 * <entry></><entry></></row><row><entry>@VBI_PIXFMT_RGBA15_BE</>
 * <entry>a0&nbsp;b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0&nbsp;g4&nbsp;g3</>
 * <entry>g2&nbsp;g1&nbsp;g0&nbsp;r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0</>
 * <entry></><entry></></row><row><entry>@VBI_PIXFMT_BGRA15_BE</>
 * <entry>a0&nbsp;r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0&nbsp;g4&nbsp;g3</>
 * <entry>g2&nbsp;g1&nbsp;g0&nbsp;b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0</>
 * <entry></><entry></></row><row><entry>@VBI_PIXFMT_ARGB15_BE</>
 * <entry>b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0&nbsp;g4&nbsp;g3&nbsp;g2</>
 * <entry>g1&nbsp;g0&nbsp;r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0&nbsp;a0</>
 * <entry></><entry></></row><row><entry>@VBI_PIXFMT_ABGR15_BE</>
 * <entry>r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0&nbsp;g4&nbsp;g3&nbsp;g2</>
 * <entry>g1&nbsp;g0&nbsp;b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0&nbsp;a0</>
 * <entry></><entry></></row>
 * </tbody></tgroup></table>
 **/
/* Attn: keep this in sync with rte, don't change order */
typedef enum {
  VBI_PIXFMT_YUV420 = 1,
  VBI_PIXFMT_YUYV,
  VBI_PIXFMT_YVYU,
  VBI_PIXFMT_UYVY,
  VBI_PIXFMT_VYUY,
  VBI_PIXFMT_RGBA32_LE = 32,
  VBI_PIXFMT_RGBA32_BE,
  VBI_PIXFMT_BGRA32_LE,
  VBI_PIXFMT_BGRA32_BE,
  VBI_PIXFMT_RGB24,
  VBI_PIXFMT_BGR24,
  VBI_PIXFMT_RGB16_LE,
  VBI_PIXFMT_RGB16_BE,
  VBI_PIXFMT_BGR16_LE,
  VBI_PIXFMT_BGR16_BE,
  VBI_PIXFMT_RGBA15_LE,
  VBI_PIXFMT_RGBA15_BE,
  VBI_PIXFMT_BGRA15_LE,
  VBI_PIXFMT_BGRA15_BE,
  VBI_PIXFMT_ARGB15_LE,
  VBI_PIXFMT_ARGB15_BE,
  VBI_PIXFMT_ABGR15_LE,
  VBI_PIXFMT_ABGR15_BE
} vbi_pixfmt;

#define VBI_PIXFMT_ABGR32_BE VBI_PIXFMT_RGBA32_LE
#define VBI_PIXFMT_ARGB32_BE VBI_PIXFMT_BGRA32_LE
#define VBI_PIXFMT_ABGR32_LE VBI_PIXFMT_RGBA32_BE
#define VBI_PIXFMT_ARGB32_LE VBI_PIXFMT_BGRA32_BE

/* Private */

#define VBI_PIXFMT_BPP(fmt)						\
	(((fmt) == VBI_PIXFMT_YUV420) ? 1 :				\
	 (((fmt) >= VBI_PIXFMT_RGBA32_LE				\
	   && (fmt) <= VBI_PIXFMT_BGRA32_BE) ? 4 :			\
	  (((fmt) == VBI_PIXFMT_RGB24					\
	    || (fmt) == VBI_PIXFMT_BGR24) ? 3 : 2)))

/* Public */

/**
 * vbi_modulation:
 * @VBI_MODULATION_NRZ_LSB:
 * @VBI_MODULATION_NRZ_MSB:
 *   The data is 'non-return to zero' coded, logical '1' bits
 *   are described by high sample values, logical '0' bits by
 *   low values. The data is either last or most significant
 *   bit first transmitted.
 * @VBI_MODULATION_BIPHASE_LSB:
 * @VBI_MODULATION_BIPHASE_MSB:
 *   The data is 'bi-phase' coded. Each data bit is described
 *   by two complementary signalling elements, a logical '1'
 *   by a sequence of '10' elements, a logical '0' by a '01'
 *   sequence. The data is either last or most significant
 *   bit first transmitted.
 *
 * Modulation of the vbi data.
 **/
typedef enum {
	VBI_MODULATION_NRZ_LSB,
	VBI_MODULATION_NRZ_MSB,
	VBI_MODULATION_BIPHASE_LSB,
	VBI_MODULATION_BIPHASE_MSB
} vbi_modulation;

/**
 * vbi_bit_slicer:
 *
 * Bit slicer context. The contents of the vbi_bit_slicer structure
 * are private, use vbi_bit_slicer_init() to initialize.
 **/
typedef struct vbi_bit_slicer {
	/*< private >*/
	vbi_bool	(* func)(struct vbi_bit_slicer *slicer,
				 uint8_t *raw, uint8_t *buf);
	unsigned int	cri;
	unsigned int	cri_mask;
	int		thresh;
	int		cri_bytes;
	int		cri_rate;
	int		oversampling_rate;
	int		phase_shift;
	int		step;
	unsigned int	frc;
	int		frc_bits;
	int		payload;
	int		endian;
	int		skip;
} vbi_bit_slicer;

extern void		vbi_bit_slicer_init(vbi_bit_slicer *slicer,
					    int raw_samples, int sampling_rate,
					    int cri_rate, int bit_rate,
					    unsigned int cri_frc, unsigned int cri_mask,
					    int cri_bits, int frc_bits, int payload,
					    vbi_modulation modulation, vbi_pixfmt fmt);

/**
 * vbi_bit_slice:
 * @slicer: Pointer to initialized bit slicer object.
 * @raw: Input data. At least the number of pixels or samples
 *  given as 'raw_samples' to vbi_bit_slicer_init().
 * @buf: Output data. This must be large enough to store
 *   the number of bits given as 'payload' to vbi_bit_slicer_init().
 * 
 * Decode one scan line of raw vbi data. Note the bit slicer tries
 * to adapt to the average signal amplitude, you should avoid
 * using the same #vbi_bit_slicer object for data from different
 * devices.
 *
 * <important>This is one of the few not reentrant libzvbi functions.
 * When you want to share one vbi_bit_slicer object between
 * multiple threads you must implement your own locking mechanism.
 * </important>
 * 
 * Return value:
 * FALSE if the raw data does not contain the expected
 * information, in particular the CRI/FRC has not been found.
 * This may also be the result of a too weak or noisy signal.
 **/
static inline vbi_bool
vbi_bit_slice(vbi_bit_slicer *slicer, uint8_t *raw, uint8_t *buf)
{
	return slicer->func(slicer, raw, buf);
}

extern char *			vbi_sliced_name(unsigned int service);

/* Raw vbi decoder */

/**
 * vbi_raw_decoder:
 *
 * Raw vbi decoder object. Only the sampling parameters are public, see
 * vbi_raw_decoder_parameters() and vbi_raw_decoder_add_services() for
 * usage instructions.
 *
 * @scanning: Either 525 (NTSC) or 625 (PAL, SECAM), describing the
 *   scan line system all line numbers refer to.
 *
 * @sampling_format: See #vbi_pixfmt.
 *
 * @sampling_rate: Sampling rate in Hz, the number of samples or pixels
 *   captured per second.
 *
 * @bytes_per_line: Number of samples or pixels captured per scan line,
 *   in bytes. This determines the raw vbi image width and has to be
 *   large enough to cover all data transmitted in the line (with
 *   headroom).
 *
 * @offset: The distance from 0H (leading edge hsync, half amplitude point)
 *   to the first sample/pixel captured, in samples/pixels. This has
 *   to be small enough to pick up the data header.
 *
 * @start: First scan line to be captured, first and second field
 *   respectively, according to the ITU-R line numbering scheme
 *   (see #vbi_sliced). Set to zero if the exact line number isn't
 *   known.
 *
 * @count: Number of scan lines captured, first and second
 *   field respectively. This can be zero if only data from one
 *   field is required. The sum @count[0] + @count[1] determines the
 *   raw vbi image height.
 *
 * @interlaced: In the raw vbi image, normally all lines of the second
 *   field are supposed to follow all lines of the first field. When
 *   this flag is set, the scan lines of first and second field
 *   will be interleaved in memory. This implies @count[0] and @count[1]
 *   are equal.
 *
 * @synchronous: Fields must be stored in temporal order, i. e. as the
 *   lines have been captured. It is assumed that the first field is
 *   also stored first in memory, however if the hardware cannot reliable
 *   distinguish fields this flag shall be cleared, which disables
 *   decoding of data services depending on the field number.
 **/
typedef struct vbi_raw_decoder {
	/*< public >*/

	/* Sampling parameters */

	int			scanning;
	vbi_pixfmt		sampling_format;
	int			sampling_rate;		/* Hz */
	int			bytes_per_line;
	int			offset;			/* 0H, samples */
	int			start[2];		/* ITU-R numbering */
	int			count[2];		/* field lines */
	vbi_bool		interlaced;
	vbi_bool		synchronous;

	/*< private >*/

	pthread_mutex_t		mutex;

	unsigned int		services;
	int			num_jobs;

	int8_t *		pattern;
	struct _vbi_raw_decoder_job {
		unsigned int		id;
		int			offset;
		vbi_bit_slicer		slicer;
	}			jobs[8];
} vbi_raw_decoder;

extern void		vbi_raw_decoder_init(vbi_raw_decoder *rd);
extern void		vbi_raw_decoder_reset(vbi_raw_decoder *rd);
extern void		vbi_raw_decoder_destroy(vbi_raw_decoder *rd);
extern unsigned int	vbi_raw_decoder_add_services(vbi_raw_decoder *rd,
						     unsigned int services,
						     int strict);
extern unsigned int	vbi_raw_decoder_remove_services(vbi_raw_decoder *rd,
							unsigned int services);
extern unsigned int	vbi_raw_decoder_parameters(vbi_raw_decoder *rd, unsigned int services,
						   int scanning, int *max_rate);
extern int		vbi_raw_decode(vbi_raw_decoder *rd, uint8_t *raw,
				       vbi_sliced *out);

/* Private */

#endif /* DECODER_H */
