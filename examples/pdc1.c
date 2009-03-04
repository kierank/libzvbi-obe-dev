/*
 *  libzvbi VPS/PDC example 1
 *
 *  Copyright (C) 2008, 2009 Michael H. Schimek
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* $Id: pdc1.c,v 1.1 2009/03/04 21:47:53 mschimek Exp $ */

/* This example shows how to receive and decode VPS/PDC Program IDs
   with libzvbi. For simplicity channel change functions have been
   omitted and not all PDC features are supported. See example 2
   for a more complete implementation.

   gcc -o pdc1 pdc1.c `pkg-config zvbi-0.2 --cflags --libs`

   The program expects the starting date and time, ending time
   and VPS/PDC time of a TV program to record as arguments:
   ./pdc1 YYYY-MM-DD HH:MM  HH:MM  HH:MM

   It opens /dev/vbi and scans the currently tuned in network for a
   matching VPS/PDC label, logging the progress on standard output,
   without actually recording anything.
*/

#define _GNU_SOURCE 1

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include <time.h>
#include <locale.h>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>

#if 1 /* to compile this program in the libzvbi source tree */
#  include "src/libzvbi.h"
#else /* to compile this program against the installed library */
#  include <libzvbi.h>
#endif

#ifndef N_ELEMENTS
#  define N_ELEMENTS(array) (sizeof (array) / sizeof (*(array)))
#endif
#ifndef MIN
#  define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

static vbi_capture *		cap;
static vbi_decoder *		dec;

static const char *		dev_name = "/dev/vbi";

/* The current time of the intended audience of the tuned in network
   according to the network (see VBI_EVENT_LOCAL_TIME). It may differ
   from system time if the system is not in sync with UTC or if we
   receive the TV signal with a delay. We will not determine the
   offset in this example but assume system time is ok. */
static time_t			audience_time;

/* The system time when the most recent PDC signal was received. */
static double			timestamp;

/* PDC Label Channel state. */
struct lc_state {
	/* The PIL most recently received on this LC, zero if none. */
	vbi_pil				pil;

	/* The system time in seconds when pil was first and most
	   recently received. */
	double				since;
	double				last;
};

/* The most recently received PILs. */
static struct lc_state		lc_state[VBI_MAX_PID_CHANNELS];

/* Video recorder states. */
enum vcr_state {
	/* All capturing stopped. */
	VCR_STATE_STBY,

	/* Searching for a PDC signal. */
	VCR_STATE_SCAN,

	/* Preparing to record. */
	VCR_STATE_PTR,

	/* Recording a program. */
	VCR_STATE_REC
};

/* The current video recorder state. */
static enum vcr_state		vcr_state;

/* The system time in seconds at the last change of vcr_state. */
static double			vcr_state_since;

/* In timer control mode we start and stop recording at the scheduled
   times. Timer control mode is enabled when the network does not
   transmit program IDs or we lost all PDC signals. */
static vbi_bool			timer_control_mode;

/* In VCR_STATE_REC this variable stops recording with a 30 second
   delay as required by EN 300 231. This is a system time in
   seconds, or DBL_MAX if no stop is planned. */
static double			delayed_stop_at;

/* In VCR_REC_STATE if delayed_stop_at < DBL_MAX, delayed_stop_pid
   contains a copy of the program ID which caused the delayed stop.

   If delayed_stop_pid.luf == 1 the program will continue on the
   channel with delayed_stop_pid.cni, accompanied by
   delayed_stop_pid.pil (which may also provide a new start date and
   time).

   Otherwise delayed_stop_pid.pil can be a valid PIL, a RI/T or INT
   service code, or zero if a loss of the PDC signal or service caused
   the delayed stop. */
static vbi_program_id		delayed_stop_pid;

/* A program to be recorded. */
struct program {
	struct program *	next;

	/* The most recently announced start and end time of the
	   program ("AT-1" in EN 300 231 in parlance), in case we do
	   not receive a PDC signal. When the duration of the program
	   is unknown start_time == end_time. end_time is
	   exclusive. */
	time_t			start_time;
	time_t			end_time;

	/* The expected Program Identification Label. Usually this is
	   the originally announced start date and time of the program
	   ("AT-2" in EN 300 231), relative to the time zone of the
	   intended audience. */
	vbi_pil			pil;

	/* The validity window of pil, that is the time when the
	   network can be expected to transmit the PIL. Usually from
	   00:00 on the same day to 04:00 on the next
	   day. pil_valid_end is exclusive. */
	time_t			pil_valid_start;
	time_t			pil_valid_end;
};

/* The recording schedule, a singly-linked list of program
   structures. */
static struct program *		schedule;

/* In VCR_STATE_PTR and VCR_STATE_REC the program we (are about to)
   record, a pointer into the schedule list, or NULL. */
static struct program *		curr_program;

/* If curr_program != NULL the program ID which put us into PTR or REC
   state. If recording was started by the timer curr_pid.pil is
   zero. */
static vbi_program_id		curr_pid;

static const double
signal_timeout [VBI_MAX_PID_CHANNELS] = {
	[VBI_PID_CHANNEL_LCI_0] = 2,
	[VBI_PID_CHANNEL_LCI_1] = 2,
	[VBI_PID_CHANNEL_LCI_2] = 2,
	[VBI_PID_CHANNEL_LCI_3] = 2,

	/* VPS signals have no error protection. When the payload
	   changes, libzvbi will wait for one repetition to confirm
	   correct reception. */
	[VBI_PID_CHANNEL_VPS] = 3 / 25.0,

	/* Other channels not implemented yet. */
};

static const double
signal_period [VBI_MAX_PID_CHANNELS] = {
	/* EN 300 231 Section 8.3: "In the case of the packet 8/30
	   version (Method B) the repetition rate of labels in any
	   label data channel is once per second." Section E.2: "Where
	   more than one label channel is in use the signalling rate
	   is normally one line per label channel per second." */
	[VBI_PID_CHANNEL_LCI_0] = 1,
	[VBI_PID_CHANNEL_LCI_1] = 1,
	[VBI_PID_CHANNEL_LCI_2] = 1,
	[VBI_PID_CHANNEL_LCI_3] = 1,

	[VBI_PID_CHANNEL_VPS] = 1 / 25.0,

	/* Other channels not implemented yet. */
};

/* For debugging. */
#define D printf ("%s:%u\n", __FUNCTION__, __LINE__)

static void
print_time			(time_t			time)
  __attribute__ ((unused));

/* For debugging. */
static void
print_time			(time_t			time)
{
	char buffer[80];
	struct tm tm;

	memset (&tm, 0, sizeof (tm));
	localtime_r (&time, &tm);
	strftime (buffer, sizeof (buffer),
		  "%Y-%m-%d %H:%M:%S %Z = ", &tm);
	fputs (buffer, stdout);

	memset (&tm, 0, sizeof (tm));
	gmtime_r (&time, &tm);
	strftime (buffer, sizeof (buffer),
		  "%Y-%m-%d %H:%M:%S UTC", &tm);
	puts (buffer);
}

static void
no_mem_exit			(void)
{
	fputs ("Out of memory.\n", stderr);
	exit (EXIT_FAILURE);
}

static const char *
pil_str				(vbi_pil		pil)
{
	static char buffer[32];

	switch (pil) {
	case VBI_PIL_TIMER_CONTROL:	return "TC";
	case VBI_PIL_INHIBIT_TERMINATE:	return "RI/T";
	case VBI_PIL_INTERRUPTION:	return "INT";
	case VBI_PIL_CONTINUE:		return "CONT";
	case VBI_PIL_NSPV:		return "NSPV/END";

	default:
		/* Attention! This returns a static string. */
		snprintf (buffer, sizeof (buffer),
			  "%02u%02uT%02u%02u",
			  VBI_PIL_MONTH (pil),
			  VBI_PIL_DAY (pil),
			  VBI_PIL_HOUR (pil),
			  VBI_PIL_MINUTE (pil));
		return buffer;
	}
}

static void
remove_program_from_schedule	(struct program *	p)
{
	struct program **pp;

	if (p == curr_program) {
		assert (VCR_STATE_STBY == vcr_state
			|| VCR_STATE_SCAN == vcr_state);

		curr_program = NULL;
	}

	for (pp = &schedule; NULL != *pp; pp = &(*pp)->next) {
		if (*pp == p) {
			*pp = p->next;
			memset (p, 0, sizeof (p));
			free (p);
			break;
		}
	}
}

static void
remove_stale_programs_from_schedule (void)
{
	struct program *p;
	struct program *p_next;

	for (p = schedule; NULL != p; p = p_next) {
		p_next = p->next;

		if (audience_time >= p->end_time
		    && audience_time >= p->pil_valid_end) {
			printf ("PIL %s no longer valid, "
				"removing program from schedule.\n",
				pil_str (p->pil));
			remove_program_from_schedule (p);
		}
	}
}

static struct program *
find_program_by_pil		(vbi_pil		pil)
{
	struct program *p;

	for (p = schedule; NULL != p; p = p->next) {
		if (pil == p->pil)
			return p;
	}

	return NULL;
}

static const char *
vcr_state_name			(enum vcr_state		state)
{
	switch (state) {
#define CASE(x) case VCR_STATE_ ## x: return #x;
	CASE (STBY)
	CASE (SCAN)
	CASE (PTR)
	CASE (REC)
	}

	assert (0);
}

static void
change_vcr_state		(enum vcr_state		new_state)
{
	if (new_state == vcr_state)
		return;

	printf ("VCR state %s -> %s.\n",
		vcr_state_name (vcr_state),
		vcr_state_name (new_state));

	vcr_state = new_state;
	vcr_state_since = timestamp;
}

static vbi_bool
teletext_8302_available		(void)
{
	return (0 != (lc_state[VBI_PID_CHANNEL_LCI_0].pil |
		      lc_state[VBI_PID_CHANNEL_LCI_1].pil |
		      lc_state[VBI_PID_CHANNEL_LCI_2].pil |
		      lc_state[VBI_PID_CHANNEL_LCI_3].pil));
}

static void
disable_timer_control		(void)
{
	if (!timer_control_mode)
		return;
	puts ("Leave timer control mode.");
	timer_control_mode = FALSE;
}

static void
enable_timer_control		(void)
{
	if (timer_control_mode)
		return;
	puts ("Enter timer control mode.");
	timer_control_mode = TRUE;
}

static void
stop_recording_now		(void)
{
	assert (VCR_STATE_REC == vcr_state);

	printf ("Program ended according to %s%s.\n",
		timer_control_mode ? "schedule" : "VPS/PDC signal",
		(delayed_stop_at < DBL_MAX) ? " with delay" : "");

	change_vcr_state (VCR_STATE_SCAN);

	delayed_stop_at = DBL_MAX;
}

static void
stop_recording_in_30s		(const vbi_program_id *	pid)
{
	assert (VCR_STATE_REC == vcr_state);

	/* What triggered the stop. */
	if (NULL == pid) {
		/* Signal lost. */
		memset (&delayed_stop_pid, 0,
			sizeof (delayed_stop_pid));
	} else {
		delayed_stop_pid = *pid;
	}

	/* If we stop because the PIL is no longer transmitted we may
	   need one second to realize (e.g. receiving LCI 0 at t+0,
	   LCI 1 at t+0.2, then LCI 0 at t+1, and again LCI 0 at t+2
	   seconds) so we start counting 30 seconds not from the
	   current time (t+2) but the first time the label was missing
	   (t+1). */
	if (0 == curr_pid.pil) {
		delayed_stop_at = timestamp + 30;
	} else {
		delayed_stop_at = lc_state[curr_pid.channel].last + 30;
	}

	printf ("Will stop recording program in %d seconds.\n",
		(int)(delayed_stop_at - timestamp));
}

static void
start_recording_by_pil		(struct program *	p,
				 const vbi_program_id *	pid)
{
	assert (!timer_control_mode);
	assert (VCR_STATE_SCAN == vcr_state
		|| VCR_STATE_PTR == vcr_state);

	puts ("Recording program using VPS/PDC signal.");

	/* EN 300 231 Section 9.4.1: "[When] labels are not received
	   correctly during a recording, the recording will be
	   continued for the computed duration following the actual
	   start time;" */
	/* XXX overflow possible. */
	p->end_time += audience_time - p->start_time;
	p->start_time = audience_time;

	change_vcr_state (VCR_STATE_REC);

	curr_program = p;
	curr_pid = *pid;
}

static void
prepare_to_record_by_pil	(struct program *	p,
				 const vbi_program_id *	pid)
{
	assert (!timer_control_mode);
	assert (VCR_STATE_SCAN == vcr_state);

	change_vcr_state (VCR_STATE_PTR);

	curr_program = p;
	curr_pid = *pid;
}

static void
start_recording_by_timer	(struct program *	p)
{
	assert (timer_control_mode);
	assert (VCR_STATE_SCAN == vcr_state);

	puts ("Recording program using timer.");

	change_vcr_state (VCR_STATE_REC);

	curr_program = p;
	memset (&curr_pid, 0, sizeof (curr_pid));
}

static void
execute_delayed_stop		(void)
{
	stop_recording_now ();

	if (delayed_stop_pid.luf) {
		/* The program has been scheduled to another date,
		   we ignore this. */
		delayed_stop_pid.luf = FALSE;
	} else if (VBI_PIL_INTERRUPTION == delayed_stop_pid.pil) {
		/* The program pauses, will not be removed from the
		   schedule. */
		return;
	} else if (timer_control_mode) {
		/* We may be wrong about the program ending now, so we
		   keep it scheduled until pil_valid_end in case we
		   receive its PIL after all. */
		return;
	}

	/* The program has ended. */
	remove_program_from_schedule (curr_program);
}

static void
signal_or_service_lost		(void)
{
	struct program *p;

	if (timer_control_mode)
		return;

	enable_timer_control ();

	switch (vcr_state) {
	case VCR_STATE_STBY:
		assert (0);

	case VCR_STATE_SCAN:
		break;

	case VCR_STATE_PTR:
		/* According to EN 300 231 Section E.1 and Section E.3
		   Example 12 the program should begin within one
		   minute when PRF=1, so we start recording now. We
		   will stop by PIL if we pick up a VPS or Teletext
		   signal again before curr_program->end_time, but we
		   will not return to VCR_STATE_PTR if PRF is still
		   1. */
		puts ("Recording program using lost "
		      "PDC signal with PRF=1.");

		p = curr_program;

		/* Record for the scheduled duration ... */
		/* XXX overflow possible. */
		p->end_time = p->end_time - p->start_time + audience_time;
		/* ... plus one minute since PRF=1. */
		p->end_time += 60 - MIN (vcr_state_since - timestamp, 60.0);
		p->start_time = audience_time;

		change_vcr_state (VCR_STATE_REC);

		memset (&curr_pid, 0, sizeof (curr_pid));

		break;

	case VCR_STATE_REC:
		if (timestamp >= delayed_stop_at) {
			execute_delayed_stop ();
			/* Now in VCR_STATE_SCAN. */
		} else if (delayed_stop_at < DBL_MAX) {
			printf ("PDC signal lost; already stopping in "
				"%d seconds.\n",
				(int)(delayed_stop_at - timestamp));
		} else if (curr_program->start_time
			   == curr_program->end_time) {
			/* Since we don't know the program duration,
			   we cannot record under timer control. We
			   stop recording in 30 seconds as shown in EN
			   300 231 Annex E.3, Example 11, 16:20:10,
			   but with an extra twist: If we receive
			   curr_program->pil again within those 30
			   seconds the stop will be canceled. */
			stop_recording_in_30s (/* pid */ NULL);
		}

		break;
	}
}

/* Any new PIL on the same label channel terminates the recording (EN
   200 321 Annex E.3 Example 1, 3, 5, 6, 10, 11). On a different
   channel it does only if curr_program->pil is no longer transmitted
   (Example 2, 4, 8, 9, 11, 12). */
static vbi_bool
stop_if_new_pil			(const vbi_program_id *	pid)
{
	vbi_bool mi;

	if (VCR_STATE_REC == vcr_state) {
		if (delayed_stop_at < DBL_MAX) {
			printf ("Already stopping in %d seconds.\n",
				(int)(delayed_stop_at - timestamp));
			return FALSE;
		}

		if (0 == curr_pid.pil) {
			/* Recording was started by timer,
			   will be stopped by timer. */
			return FALSE;
		}
	}

	if (0 != curr_pid.pil
	    && pid->channel == curr_pid.channel) {
		if (VCR_STATE_PTR == vcr_state) {
			/* pid->pil is either INT or RI/T or not
			   curr_program->pil. */
			change_vcr_state (VCR_STATE_SCAN);
			return TRUE;
		}

		/* EN 300 231 Section 6.2 p): "When set to "1" in a
		   particular programme label or Service Code, [MI]
		   indicates that the end of transmission of the
		   programme label coincides exactly with the end of
		   transmission of the programme or that the Service
		   Code takes immediate effect. When set to "0", it
		   indicates that recording should continue for 30 s
		   after the programme label is no longer transmitted
		   (and is replaced by another valid label), or that
		   the effect of service codes is delayed by 30 s."
		   Only Teletext packet 8/30 format 2 contains a MI
		   flag but libzvbi correctly sets the pid->mi field
		   for other label channels. */
		mi = pid->mi;
	} else {
		/* Only Teletext packet 8/30 format 2 supports
		   label channels. */
		if (pid->channel > VBI_PID_CHANNEL_LCI_3
		    || curr_pid.channel > VBI_PID_CHANNEL_LCI_3) {
			printf ("Ignore %s/%02X from different source.\n",
				pil_str (pid->pil), pid->pty);
			return FALSE;
		}

		if (curr_program->pil == lc_state[curr_pid.channel].pil) {
			printf ("Ignore %s/%02X with different LCI.\n",
				pil_str (pid->pil), pid->pty);
			return FALSE;
		}

		printf ("PIL %s is no longer present on LC %u.\n",
			pil_str (curr_program->pil),
			curr_pid.channel);

		if (VCR_STATE_PTR == vcr_state) {
			change_vcr_state (VCR_STATE_SCAN);
			remove_program_from_schedule (curr_program);
			return TRUE;
		}

		/* There is no example in EN 300 231 of a new label
		   with MI=0 replacing a label with MI=1 or vice
		   versa. My interpretation of Section 6.2 p) and
		   Annex E.3 Example 1 to 7 is that only the MI flag
		   of the old label determines when the program
		   stops. */
		if (0 == curr_pid.pil)
			mi = TRUE;
		else
			mi = curr_pid.mi;
	}

	if (mi) {
		stop_recording_now ();
		return TRUE;
	} else {
		stop_recording_in_30s (pid);
		return FALSE;
	}
}

/* Interruption or Recording Inhibit/Terminate service code. */
static void
received_int_rit		(const vbi_program_id *	pid)
{
	switch (vcr_state) {
	case VCR_STATE_STBY:
		assert (0);

	case VCR_STATE_SCAN:
		disable_timer_control ();
		break;

	case VCR_STATE_PTR:
		assert (!timer_control_mode);

		/* fall through */

	case VCR_STATE_REC:
		if (timer_control_mode) {
			/* Since we don't record by curr_program->pil
			   this PIL may or may not interrupt or
			   terminate curr_program, so we ignore it. */
			printf ("In timer control mode; "
				"%s/%02X ignored.\n",
				pil_str (pid->pil), pid->pty);
			return;
		} else if (VCR_STATE_REC == vcr_state
			   && timestamp >= delayed_stop_at) {
			disable_timer_control ();
			execute_delayed_stop ();
			/* Now in VCR_STATE_SCAN. */
		} else if (!stop_if_new_pil (pid)) {
			return;
		}

		break;
	}
}

static void
received_same_pil		(const vbi_program_id *	pid)
{
	if (VCR_STATE_REC == vcr_state) {
		if (delayed_stop_at < DBL_MAX) {
			/* We lost all PDC signals and timer_control()
			   arranged for a delayed stop. Or we received
			   an INT or RI/T code or a different PIL than
			   curr_program->pil with MI=0. But now we
			   receive curr_program->pil again. */
			delayed_stop_at = DBL_MAX;
			puts ("Delayed stop canceled.");
		} else {
			/* We lost all PDC signals and timer_control()
			   started recording out of SCAN or PTR state,
			   but now we receive curr_program->pil
			   (again). Or this is just a retransmission
			   of the PIL which started recording. Either
			   way, we do not return to VCR_STATE_PTR if
			   PRF is (still or again) 1. */
			puts ("Already recording.");
		}

		return;
	}

	assert (VCR_STATE_PTR == vcr_state);

	if (pid->prf) {
		if (timestamp >= vcr_state_since + 60) {
			/* EN 300 231 Section E.1,
			   Section E.3 Example 12. */
			puts ("Overriding stuck PRF flag.");
		} else {
			puts ("Already prepared to record.");
			return;
		}
	}

	/* PRF 1 -> 0, program starts now. */

	start_recording_by_pil (curr_program, pid);
}

static void
received_pil			(const vbi_program_id *	pid)
{
	struct program *p;

	if (pid->luf) {
		/* VCR reprogramming omitted. */
		return;
	}

	switch (vcr_state) {
	case VCR_STATE_STBY:
		assert (0);

	case VCR_STATE_SCAN:
		disable_timer_control ();
		p = find_program_by_pil (pid->pil);
		break;

	case VCR_STATE_PTR:
		assert (!timer_control_mode);

		/* fall through */

	case VCR_STATE_REC:
		if (timer_control_mode) {
			disable_timer_control ();

			p = find_program_by_pil (pid->pil);
			if (p == curr_program) {
				puts ("Continue recording using "
				      "VPS/PDC signal.");

				curr_pid = *pid;

				/* Cancel a delayed stop because the
				   program is still running. */
				delayed_stop_at = DBL_MAX;

				return;
			}

			/* The running program is not scheduled for
			   recording. */
			stop_recording_now ();
		} else if (VCR_STATE_REC == vcr_state
			   && timestamp >= delayed_stop_at) {
			disable_timer_control ();
			execute_delayed_stop ();
			p = find_program_by_pil (pid->pil);
		} else {
			if (pid->pil == curr_program->pil) {
				received_same_pil (pid);
				return;
			}

			if (!stop_if_new_pil (pid))
				return;

			p = find_program_by_pil (pid->pil);
		}

		break;
	}

	assert (VCR_STATE_SCAN == vcr_state);

	if (NULL == p)
		return;

	if (pid->prf) {
		prepare_to_record_by_pil (p, pid);
	} else {
		start_recording_by_pil (p, pid);
	}
}

static void
event_handler			(vbi_event *		ev,
				 void *			user_data)
{
	const vbi_program_id *pid;
	vbi_pid_channel lci;

	user_data = user_data; /* unused, no warning please */

	assert (VCR_STATE_STBY != vcr_state);

	pid = ev->ev.prog_id;
	lci = pid->channel;

	switch (lci) {
		unsigned int i;

	case VBI_PID_CHANNEL_LCI_0:
	case VBI_PID_CHANNEL_LCI_1:
	case VBI_PID_CHANNEL_LCI_2:
	case VBI_PID_CHANNEL_LCI_3:
		/* EN 300 231 Section 8.3: "In the case of the packet
		   8/30 version (Method B) the repetition rate of
		   labels in any label data channel is once per
		   second." Section E.2: "Where more than one label
		   channel is in use the signalling rate is normally
		   one line per label channel per second." It follows
		   when the PIL on one channel is more than one second
		   older than a PIL on any other channel, the former
		   PIL is no longer transmitted. */
		for (i = VBI_PID_CHANNEL_LCI_0;
		     i <= VBI_PID_CHANNEL_LCI_3; ++i) {
			if (i == lci)
				continue;
			if (0 == lc_state[i].pil)
				continue;
			/* We allow two seconds to be sure. */
			if (timestamp >= lc_state[i].last + 2) {
				double t = lc_state[i].last + 1;

				lc_state[i].pil = 0;
				lc_state[i].since = t;
				lc_state[i].last = t;
			}
		}

		break;

	case VBI_PID_CHANNEL_VPS:
		/* EN 300 231 Section 9.4.1: "When both line 16 (VPS)
		   and Teletext-delivered labels are available
		   simultaneously, decoders should default to the
		   Teletext-delivered service;" */
		if (teletext_8302_available ())
			goto finish;
		break;

	default:
		/* Support for other sources not implemented yet. */
		return;
	}

	printf ("Received PIL %s/%02X on LC %u.\n",
		pil_str (pid->pil), pid->pty, lci);

	switch (pid->pil) {
	case VBI_PIL_TIMER_CONTROL:
	case VBI_PIL_CONTINUE:
		signal_or_service_lost ();
		break;

	case VBI_PIL_INTERRUPTION:
	case VBI_PIL_INHIBIT_TERMINATE:
		received_int_rit (pid);
		break;

	default:
		received_pil (pid);
		break;
	}

 finish:
	if (lc_state[lci].pil != pid->pil) {
		lc_state[lci].pil = pid->pil;
		lc_state[lci].since = timestamp;
	}

	lc_state[lci].last = timestamp;
}

static vbi_bool
in_pil_validity_window		(void)
{
	struct program *p;

	for (p = schedule; NULL != p; p = p->next) {
		/* The announced start and end time should fall within
		   the PIL validity window, but just in case. */
		if ((audience_time >= p->start_time
		     && audience_time < p->end_time)
		    || (audience_time >= p->pil_valid_start
			&& audience_time < p->pil_valid_end))
			return TRUE;
	}

	return FALSE;
}

static void
timer_control			(void)
{
	struct program *p;

	assert (timer_control_mode);

	switch (vcr_state) {
	case VCR_STATE_STBY:
	case VCR_STATE_PTR:
		assert (0);

	case VCR_STATE_SCAN:
		break;

	case VCR_STATE_REC:
		if (timestamp >= delayed_stop_at) {
			execute_delayed_stop ();
		} else if (delayed_stop_at < DBL_MAX) {
			/* Will stop later. */
			return;
		} else if (audience_time >= curr_program->end_time) {
			stop_recording_now ();
		} else {
			/* Still running. */
			return;
		}

		assert (VCR_STATE_SCAN == vcr_state);

		/* We remove the program from the schedule as shown in
		   EN 300 231 Annex E.3, Example 11, 01:58:00. However
		   as the example itself demonstrates this is not in
		   the best interest of the user. A better idea may be
		   to keep the program scheduled until
		   curr_program->pil_valid_end, in case the program is
		   late or overrunning and we receive its PIL after
		   all. */
		remove_program_from_schedule (curr_program);

		break;
	}

	for (p = schedule; NULL != p; p = p->next) {
		/* Note if no program length has been specified
		   (start_time == end_time) this function will not
		   record the program. */
		/* We must also compare against p->end_time because we
		   will not remove the program from the schedule at
		   that time. See execute_delayed_stop(). */
		if (audience_time >= p->start_time
		    && audience_time < p->end_time) {
			start_recording_by_timer (p);
			return;
		}
	}
}

static void
pdc_signal_check		(void)
{
	static const unsigned int ttx_chs =
		((1 << VBI_PID_CHANNEL_LCI_0) |
		 (1 << VBI_PID_CHANNEL_LCI_0) |
		 (1 << VBI_PID_CHANNEL_LCI_0) |
		 (1 << VBI_PID_CHANNEL_LCI_0));
	static const unsigned int vps_ch =
		(1 << VBI_PID_CHANNEL_VPS);
	unsigned int active_chs;
	unsigned int lost_chs;
	vbi_pid_channel i;

	if (timer_control_mode)
		return;

	/* Determine if we lost signals. For now only Teletext and VPS
	   delivery is supported, so we don't check other channels. */

	active_chs = 0;
	lost_chs = 0;

	for (i = 0; i < VBI_MAX_PID_CHANNELS; ++i) {
		double timeout_at;

		if (0 == lc_state[i].pil)
			continue;

		timeout_at = lc_state[i].last + signal_timeout[i];
		if (timestamp >= timeout_at) {
			double lost_at;

			lc_state[i].pil = 0;
			lost_at = lc_state[i].last + signal_period[i];
			lc_state[i].since = lost_at;
			lc_state[i].last = lost_at;
			lost_chs |= 1 << i;
		} else {
			active_chs |= 1 << i;
		}
	}

	if (0 == (active_chs & ttx_chs)) {
		if (0 == (active_chs & vps_ch)) {
			if (0 != lost_chs) {
				puts ("Teletext and/or VPS PDC "
				      "signal lost, will fall back "
				      "to timer control.");
			}
		} else {
			if (0 != (lost_chs & ttx_chs)) {
				puts ("Teletext PDC signal lost, "
				      "will fall back to VPS.");
			}
		}
	}

	if (0 == active_chs)
		signal_or_service_lost ();
}

static void
capture_loop			(void)
{
	vbi_service_set services;
	char *errstr;
	struct timeval timeout;
	vbi_capture_buffer *sliced_buffer;
	double last_timestamp;

	assert (VCR_STATE_STBY == vcr_state);

	/* Open VBI device. */

	services = (VBI_SLICED_TELETEXT_B |
		    VBI_SLICED_VPS);

	cap = vbi_capture_v4l2k_new (dev_name,
				     /* video_fd */ -1,
				     /* buffers */ 5,
				     &services,
				     /* strict */ 0,
				     &errstr,
				     /* verbose */ FALSE);
	if (NULL == cap) {
		fprintf (stderr,
			 "Cannot open \"%s\": %s.\n",
			 dev_name, errstr);
		free (errstr);
		exit (EXIT_FAILURE);
	}

	vbi_channel_switched (dec, 0);

	change_vcr_state (VCR_STATE_SCAN);

	/* Don't wait more than two seconds for the driver
	   to return data. */
	timeout.tv_sec = 2;
	timeout.tv_usec = 0;

	last_timestamp = 0;

	for (;;) {
		unsigned int n_lines;
		int r;

		r = vbi_capture_pull (cap,
				      /* raw_buffer */ NULL,
				      &sliced_buffer,
				      &timeout);
		switch (r) {
		case -1:
			fprintf (stderr,
				 "VBI read error: %s.\n",
				 strerror (errno));
			/* Could be ignored, esp. EIO from some
			   drivers. */
			exit (EXIT_FAILURE);

		case 0: 
			fprintf (stderr, "VBI read timeout\n");
			exit (EXIT_FAILURE);

		case 1: /* success */
			break;

		default:
			assert (0);
		}

		timestamp = sliced_buffer->timestamp;
		n_lines = sliced_buffer->size / sizeof (vbi_sliced);

		/* See mainloop(). */
		audience_time = (time_t) timestamp;

		vbi_decode (dec,
			    (vbi_sliced *) sliced_buffer->data,
			    n_lines, timestamp);

		/* Once per second is enough. */
		if ((long) last_timestamp != (long) timestamp) {
			if (!timer_control_mode) {
				/* May enable timer control mode. */
				pdc_signal_check ();
			}

			if (timer_control_mode)
				timer_control ();
		}

		if (VCR_STATE_SCAN == vcr_state
		    && !in_pil_validity_window ())
			break;

		last_timestamp = timestamp;
	}

	change_vcr_state (VCR_STATE_STBY);

	/* Close VBI device. */

	vbi_capture_delete (cap);
	cap = NULL;
}

static void
mainloop			(void)
{
	for (;;) {
		struct program *p;
		time_t first_scan;

		assert (VCR_STATE_STBY == vcr_state);

		/* The current time of the intended audience of the
		   tuned in network according to the network. It may
		   differ from system time if the system is not in
		   sync with UTC or if we receive the TV signal with a
		   delay. We will not determine the offset in this
		   example but assume system time is accurate. */
 		audience_time = time (NULL);

		remove_stale_programs_from_schedule ();
		if (NULL == schedule)
			break;

		first_scan = schedule->start_time;
		for (p = schedule; NULL != p; p = p->next) {
			if (p->start_time < first_scan)
				first_scan = p->start_time;
			if (p->pil_valid_start < first_scan)
				first_scan = p->pil_valid_start;
		}

		while (first_scan > audience_time) {
			printf ("Sleeping until %s.\n",
				ctime (&first_scan));

			/* May abort earlier. */
			sleep (first_scan - audience_time);

			audience_time = time (NULL);
		}

		capture_loop ();
	}

	puts ("Recording schedule is empty.");
}

static void
reset_state			(void)
{
	unsigned int i;

	audience_time = 0.0;
	timestamp = 0.0;

	for (i = 0; i < VBI_MAX_PID_CHANNELS; ++i) {
		lc_state[i].pil = 0; /* none received */
		lc_state[i].since = 0.0;
		lc_state[i].last = 0.0;
	}

	vcr_state = VCR_STATE_STBY;
	vcr_state_since = 0.0;

	timer_control_mode = FALSE;

	delayed_stop_at = DBL_MAX;
}

static void
add_program_to_schedule		(const struct tm *	start_tm,
				 const struct tm *	end_tm,
				 const struct tm *	pdc_tm)
{
	struct program *p;
	struct tm tm;
	time_t t;

	/* Note PILs represent the originally announced start date of
	   the program in the time zone of the intended audience. When
	   we convert pdc_tm to a PIL we assume that zone is the same
	   as the system time zone (TZ environment variable), and
	   start_tm, end_tm and pdc_tm are also given relative to this
	   time zone. We do not consider the case where a program
	   straddles a daylight saving time discontinuity, e.g. starts
	   in the CET zone and ends in the CEST zone or vice versa. */

	p = calloc (1, sizeof (*p));
	assert (NULL != p);

	tm = *start_tm;
	tm.tm_isdst = -1; /* unknown */
	p->start_time = mktime (&tm);
	if ((time_t) -1 == p->start_time) {
		fprintf (stderr, "Invalid start time.\n");
		exit (EXIT_FAILURE);
	}

	tm = *start_tm;
	tm.tm_isdst = -1; /* unknown */
	tm.tm_hour = end_tm->tm_hour;
	tm.tm_min = end_tm->tm_min;
	if (end_tm->tm_hour < start_tm->tm_hour) {
		/* mktime() should handle a 32nd. */
		++tm.tm_mday;
	}
	p->end_time = mktime (&tm);
	if ((time_t) -1 == p->end_time) {
		fprintf (stderr, "Invalid end time.\n");
		exit (EXIT_FAILURE);
	}

	tm = *start_tm;
	tm.tm_hour = pdc_tm->tm_hour;
	tm.tm_min = pdc_tm->tm_min;
	if (pdc_tm->tm_hour >= start_tm->tm_hour + 12) {
		/* mktime() should handle a 0th. */
		--tm.tm_mday;
	} else if (pdc_tm->tm_hour + 12 < start_tm->tm_hour) {
		++tm.tm_mday;
	}

	/* Normalize. */
	t = mktime (&tm);
	if ((time_t) -1 == t || NULL == localtime_r (&t, &tm)) {
		fprintf (stderr, "Cannot determine PIL month/day.\n");
		exit (EXIT_FAILURE);
	}

	p->pil = VBI_PIL (tm.tm_mon + 1, /* -> 1 ... 12 */
			  tm.tm_mday,
			  tm.tm_hour,
			  tm.tm_min);

	if (!vbi_pil_validity_window (&p->pil_valid_start,
				      &p->pil_valid_end,
				      p->pil,
				      p->start_time,
				      NULL /* system tz */)) {
		fprintf (stderr, "Cannot determine PIL validity.\n");
		exit (EXIT_FAILURE);
	}

	p->next = schedule;
	schedule = p;
}

static void
parse_args			(int			argc,
				 char **		argv)
{
	struct tm start_tm;
	struct tm end_tm;
	struct tm pdc_tm;

	if (5 != argc)
		goto invalid;

	memset (&start_tm, 0, sizeof (struct tm));
	if (NULL == strptime (argv[1], "%Y-%m-%d", &start_tm))
		goto invalid;
	if (NULL == strptime (argv[2], "%H:%M", &start_tm))
		goto invalid;

	memset (&end_tm, 0, sizeof (struct tm));
	if (NULL == strptime (argv[3], "%H:%M", &end_tm))
		goto invalid;

	memset (&pdc_tm, 0, sizeof (struct tm));
	if (NULL == strptime (argv[4], "%H:%M", &pdc_tm))
		goto invalid;

	add_program_to_schedule (&start_tm, &end_tm, &pdc_tm);

	return;

 invalid:
	fprintf (stderr,
"Please specify the start time of a program in the format\n"
"YYYY-MM-DD HH:MM, the end time HH:MM and a VPS/PDC time HH:MM.\n");

	exit (EXIT_FAILURE);
}

int
main				(int			argc,
				 char **		argv)
{
	vbi_bool success;

	setlocale (LC_ALL, "");

	parse_args (argc, argv);

	dec = vbi_decoder_new ();
	if (NULL == dec)
		no_mem_exit ();

	success = vbi_event_handler_register (dec, VBI_EVENT_PROG_ID,
					      event_handler,
					      /* user_data */ NULL);
	if (!success)
		no_mem_exit ();

	reset_state ();

	mainloop ();

	vbi_decoder_delete (dec);

	while (NULL != schedule)
		remove_program_from_schedule (schedule);

	exit (EXIT_SUCCESS);
}
