#include <math.h>

#define AMP 110
#define DC 60

static vbi_raw_decoder sim;
static double sim_time;

static inline double
shape(double ph)
{
	double x = sin(ph);

	return x * x;
}

/*
 *  Closed Caption Signal Simulator
 */

static inline double
cc_sim(double t, double F, unsigned char b1, unsigned char b2)
{
	int bits = (b2 << 10) + (b1 << 2) + 2; /* start bit 0 -> 1 */
	double t1 = 10.5e-6 - .25 / F;
	double t2 = t1 + 7 / F;		/* CRI 7 cycles */
	double t3 = t2 + 1.5 / F;
	double t4 = t3 + 18 / F;	/* 17 bits + raise and fall time */
	double ph;

	if (t < t1) {
		return 0.0;
	} else if (t < t2) {
		t -= t2;
		ph = M_PI * 2 * t * F - (M_PI * .5);
		return sin(ph) / 2 + .5;
	} else if (t < t3) {
		return 0.0;
	} else if (t < t4) {
		int i, n;

		t -= t3;
		i = (t * F - .0);
		n = (bits >> i) & 3; /* low = 0, up, down, high */
		if (n == 0)
			return 0.0;
		else if (n == 3)
			return 1.0;

		if ((n ^ i) & 1) /* down */
			ph = M_PI * 2 * (t - 1 / F) * F / 4;
		else /* up */
			ph = M_PI * 2 * (t - 0 / F) * F / 4;

		return shape(ph);
	} else {
		return 0.0;
	}
}

/*
 *  Wide Screen Signalling Simulator
 */

static inline double
wss625_sim(double t, double F, unsigned int bits)
{
	static int twobit[] = { 0xE38, 0xE07, 0x1F8, 0x1C7 };
	static char frame[] =
		"\0"
		"\1\1\1\1\1\0\0\0\1\1\1\0\0\0"
		"\1\1\1\0\0\0\1\1\1\0\0\0\1\1\1"
		"\0\0\0\1\1\1\1\0\0\0\1\1\1\1\0\0\0\0\0\1\1\1\1\1"
		"x";
	double t1 = 11.0e-6 - .5 / F;
	double t4 = t1 + (29 + 24 + 84) / F;
	double ph;
	int i, j, n;

	frame[1 + 29 + 24] = bits & 1;

	if (t < t1) {
		return 0.0;
	} else if (t < t4) {
		t -= t1;
		i = (t * F - .0);
		if (i < 29 + 24) {
			n = frame[i] + 2 * frame[i + 1];
		} else {
			j = i - 29 - 24;
			n = twobit[(bits >> (j / 6)) & 3];
			n = (n >> (j % 6)) & 3;
		}

		/* low = 0, down, up, high */
		if (n == 0)
			return 0.0;
		else if (n == 3)
			return 1.0;
		if ((n ^ i) & 1) 
			ph = M_PI * 2 * (t - 1 / F) * F / 4;
		else /* down */
			ph = M_PI * 2 * (t - 0 / F) * F / 4;

		return shape(ph);
	} else {
		return 0.0;
	}
}

static inline double
wss525_sim(double t, double F, unsigned int bits)
{
	double t1 = 11.2e-6 - .5 / F;
	double t4 = t1 + (2 + 14 + 6 + 1) / F;
	double ph;
	int i, n;

	bits = bits * 2 + (2 << 21); /* start bits 10, stop 0 */

	if (t < t1) {
		return 0.0;
	} else if (t < t4) {
		t -= t1;
		i = (t * F - .0);
		n = (bits >> (22 - i)) & 3; /* low = 0, up, down, high */

		if (n == 0)
			return 0.0;
		else if (n == 3)
			return 1.0;
		if ((n ^ i) & 1) 
			ph = M_PI * 2 * (t - 0 / F) * F / 4;
		else /* down */
			ph = M_PI * 2 * (t - 1 / F) * F / 4;

		return shape(ph);
	} else
		return 0.0;
}

/*
 *  Teletext Signal Simulator
 */

static double
ttx_sim(double t, double F, const uint8_t *text)
{
	double t1 = 10.3e-6 - .5 / F;
	double t2 = t1 + (45 * 8 + 1) / F; /* 45 bytes + raise and fall time */
	double ph;

	if (t < t1) {
		return 0.0;
	} else if (t < t2) {
		int i, j, n;

		t -= t1;
		i = (t * F);
		j = i >> 3;
		i &= 7;

		if (j == 0)
			n = ((text[0] * 2) >> i) & 3;
		else
			n = (((text[j - 1] >> 7) + text[j] * 2) >> i) & 3;

		if (n == 0) {
			return 0.0;
		} else if (n == 3) {
			return 1.0;
		} else if ((n ^ i) & 1) {
			ph = M_PI * 2 * (t - 1 / F) * F / 4;
			return shape(ph);
		} else { /* up */
			ph = M_PI * 2 * (t - 0 / F) * F / 4;
			return shape(ph);
		}
	} else {
		return 0.0;
	}

	if (t < t1) {
		return 0.0;
	} else if (t < t2) {
		int i, j, n;

		t -= t1;
		i = (t * F - .0);
		j = i >> 3;
		if (j < 44)
			n = ((text[j + 1] * 256 + text[j]) >> i) & 3;
		else
			n = (text[i] >> i) & 3;

		return shape(ph);
	}
}

static int caption_i = 0;
static const uint8_t caption_text[] = {
	0x14, 0x25, 0x14, 0x25, 'L', 'I', 'B', 'Z',
	'V', 'B', 'I', ' ', 'C', 'A', 'P', 'T',
	'I', 'O', 'N', ' ', 'S', 'I', 'M', 'U',
	'L', 'A', 'T', 'I', 'O', 'N', 0x14, 0x2D,
	0x14, 0x2D /* even size please, add 0 if neccessary */
};

static inline int
odd(int c)
{
	int n;

	n = c ^ (c >> 4);
	n = n ^ (n >> 2);
	n = n ^ (n >> 1);

	if (!(n & 1))
		c |= 0x80;

	return c;
}

static uint8_t *
ttx_next(void)
{
	static uint8_t s1[2][10] = {
		{ 0x02, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15 },
		{ 0x02, 0x15, 0x02, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15 }
	};
	static uint8_t s2[32] = "100\2LIBZVBI\7            00:00:00";
	static uint8_t s3[40] = "  LIBZVBI TELETEXT SIMULATION           ";
	static uint8_t s4[40] = "  Page 100                              ";
	static uint8_t s5[10][42] = {
		{ 0x02, 0x2f, 0x97, 0x20, 0x37, 0x23, 0x23, 0x23, 0x23, 0x23,
		  0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23,
		  0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23,
		  0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23,
		  0xb5, 0x20 },
		{ 0xc7, 0x2f, 0x97, 0x0d, 0xb5, 0x04, 0x20, 0x9d, 0x83, 0x8c,
		  0x08, 0x2a, 0x2a, 0x2a, 0x89, 0x20, 0x20, 0x0d, 0x54, 0x45,
		  0xd3, 0x54, 0x20, 0xd0, 0xc1, 0xc7, 0x45, 0x8c, 0x20, 0x20,
		  0x08, 0x2a, 0x2a, 0x2a, 0x89, 0x0d, 0x20, 0x20, 0x1c, 0x97,
		  0xb5, 0x20 },
		{ 0x02, 0xd0, 0x97, 0x20, 0xb5, 0x20, 0x20, 0x20, 0x20, 0x20,
		  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
		  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
		  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
		  0xea, 0x20 },
		{ 0xc7, 0xd0, 0x97, 0x20, 0xb5, 0x20, 0x20, 0x20, 0x20, 0x20,
		  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
		  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
		  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
		  0xb5, 0x20 },
		{ 0x02, 0xc7, 0x97, 0x20, 0xb5, 0x20, 0x20, 0x20, 0x20, 0x20,
		  0x20, 0x15, 0x1a, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c,
		  0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c,
		  0x2c, 0x2c, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x97, 0x19,
		  0xb5, 0x20 },
		{ 0xc7, 0xc7, 0x97, 0x20, 0xb5, 0x20, 0x20, 0x20, 0x20, 0x20,
		  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
		  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
		  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
		  0xb5, 0x20 },
		{ 0x02, 0x8c, 0x97, 0x9e, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x13,
		  0x7f, 0x7f, 0x7f, 0x7f, 0x16, 0x7f, 0x7f, 0x7f, 0x7f, 0x92,
		  0x7f, 0x92, 0x7f, 0x7f, 0x15, 0x7f, 0x7f, 0x15, 0x7f, 0x91,
		  0x91, 0x7f, 0x7f, 0x91, 0x94, 0x7f, 0x94, 0x7f, 0x94, 0x97,
		  0xb5, 0x20 },
		{ 0xc7, 0x8c, 0x97, 0x9e, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x13,
		  0x7f, 0x7f, 0x7f, 0x7f, 0x16, 0x7f, 0x7f, 0x7f, 0x7f, 0x92,
		  0x7f, 0x7f, 0x7f, 0x7f, 0x15, 0x7f, 0x7f, 0x7f, 0x7f, 0x91,
		  0x7f, 0x7f, 0x7f, 0x7f, 0x94, 0x7f, 0x7f, 0x7f, 0x7f, 0x97,
		  0xb5, 0x20 },
		{ 0x02, 0x9b, 0x97, 0x9e, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x13,
		  0x7f, 0x7f, 0x7f, 0x7f, 0x16, 0x7f, 0x7f, 0x7f, 0x7f, 0x92,
		  0x7f, 0x7f, 0x7f, 0x7f, 0x15, 0x7f, 0x7f, 0x7f, 0x7f, 0x91,
		  0x7f, 0x7f, 0x7f, 0x7f, 0x94, 0x7f, 0x7f, 0x7f, 0x7f, 0x97,
		  0xb5, 0x20 },
		{ 0xc7, 0x9b, 0x97, 0x20, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23,
		  0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23,
		  0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23,
		  0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23,
		  0xa1, 0x20 }
	};
	static uint8_t buf[45];
	static int row = 0, page = 0;
	int i;

	buf[0] = 0x55;
	buf[1] = 0x55;
	buf[2] = 0x27;

	if (row == 0) {
		memcpy(buf + 3, s1[page], 10);
		page ^= 1;
		for (i = 0; i < 32; i++)
			buf[13 + i] = odd(s2[i]);
	} else if (row == 1) {
		buf[3] = 0x02; buf[4] = 0x02;
		for (i = 0; i < 40; i++)
			buf[5 + i] = odd(s3[i]);
	} else if (row == 2) {
		buf[3] = 0x02; buf[4] = 0x49;
		for (i = 0; i < 40; i++)
			buf[5 + i] = odd(s4[i]);
	} else {
		memcpy(buf + 3, s5[row - 3], 42);
	}

	if (++row >= 13) row = 0;

	return buf;
}

static void
read_sim(uint8_t *raw_data, vbi_sliced *sliced_data,
	 int *lines, double *timestamp)
{
	uint8_t *buf;
	double start, inc;
	int i;

	memset(raw_data, 0, (sim.count[0] + sim.count[1])
			    * sim.bytes_per_line);

	*timestamp = sim_time;

	if (sim.scanning == 525)
		sim_time += 1001 / 30000.0;
	else
		sim_time += 1 / 25.0;

	start = sim.offset / (double) sim.sampling_rate;
	inc = 1 / (double) sim.sampling_rate;

	if (sim.scanning == 525) {
		/* Closed Caption */
		{
			buf = raw_data + (21 - sim.start[0])
				* sim.bytes_per_line;

			for (i = 0; i < sim.bytes_per_line; i++)
				buf[i] = cc_sim(start + i * inc, 15734 * 32,
						odd(caption_text[caption_i]),
						odd(caption_text[caption_i + 1]))
					* AMP + DC;

			if ((caption_i += 2) > (int) sizeof(caption_text))
				caption_i = 0;
		}

		/* WSS NTSC-Japan */
		{
			const int poly = (1 << 6) + (1 << 1) + 1;
			int b0 = 1, b1 = 1;
			int bits = (b0 << 13) + (b1 << 12);
			int crc, j;

			crc = (((1 << 6) - 1) << (14 + 6)) + (bits << 6);

			for (j = 14 + 6 - 1; j >= 0; j--) {
				if (crc & ((1 << 6) << j))
					crc ^= poly << j;
			}

			bits <<= 6;
			bits |= crc;

			/* fprintf(stderr, "WSS CPR << %08x\n", bits); */

			buf = raw_data + (20 - sim.start[0])
				* sim.bytes_per_line;

			for (i = 0; i < sim.bytes_per_line; i++)
				buf[i] = wss525_sim(start + i * inc,
						    447443, bits)
					* AMP + DC;
		}
	} else {
		/* Closed Caption */
		{
			buf = raw_data + (22 - sim.start[0])
				* sim.bytes_per_line;

			for (i = 0; i < sim.bytes_per_line; i++)
				buf[i] = cc_sim(start + i * inc, 15625 * 32,
						odd(caption_text[caption_i]),
						odd(caption_text[caption_i + 1]))
					* AMP + DC;

			if ((caption_i += 2) > (int) sizeof(caption_text))
				caption_i = 0;
		}

		/* WSS PAL */
		{
			int g0 = 1, g1 = 2, g2 = 3, g3 = 4;
			int bits = (g3 << 11) + (g2 << 8) + (g1 << 4) + g0;

			buf = raw_data + (23 - sim.start[0])
				* sim.bytes_per_line;

			for (i = 0; i < sim.bytes_per_line; i++)
				buf[i] = wss625_sim(start + i * inc, 15625 * 320,
						    bits) * AMP + DC;
		}

		/* Teletext */
		{
			int line, count;
			uint8_t *text;

			buf = raw_data;

			for (line = sim.start[0], count = sim.count[0];
			     count > 0; line++, count--, buf += sim.bytes_per_line)
				if ((line >= 7 && line <= 15)
				    || (line >= 19 && line <= 21)) {
					text = ttx_next();
					for (i = 0; i < sim.bytes_per_line; i++) {
						buf[i] = ttx_sim(start + i * inc,
								 15625 * 444,
								 text) * AMP + DC;
					}
				}
			for (line = sim.start[1], count = sim.count[1];
			     count > 0; line++, count--, buf += sim.bytes_per_line)
				if ((line >= 320 && line <= 328)
				    || (line >= 332 && line <= 335)) {
					text = ttx_next();
					for (i = 0; i < sim.bytes_per_line; i++) {
						buf[i] = ttx_sim(start + i * inc,
								 15625 * 444,
								 text) * AMP + DC;
					}
				}
		}
	}

	*lines = vbi_raw_decode(&sim, raw_data, sliced_data);
}

static vbi_raw_decoder *
init_sim(int scanning, unsigned int services)
{
	vbi_raw_decoder_init(&sim);

	sim.scanning = scanning;
	sim.sampling_format = VBI_PIXFMT_YUV420;
	sim.sampling_rate = 2 * 13500000;
	sim.bytes_per_line = 1440;
	sim.offset = 9.7e-6 * sim.sampling_rate;
	sim.interlaced = FALSE;
	sim.synchronous = TRUE;

	if (scanning == 525) {
		sim.start[0] = 10;
		sim.count[0] = 21 - 10 + 1;
		sim.start[1] = 272;
		sim.count[1] = 285 - 272 + 1;
	} else if (scanning == 625) {
		sim.start[0] = 6;
		sim.count[0] = 23 - 6 + 1;
		sim.start[1] = 318;
		sim.count[1] = 335 - 318 + 1;
	} else
		assert(!"invalid scanning value");

	sim_time = 0.0;

	vbi_raw_decoder_add_services(&sim, services, 0);

	return &sim;
}
