/*
 *  VBI proxy daemon
 *
 *  Copyright (C) 2002, 2003 Tom Zoerner (and others)
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
 *
 *
 *  Description:
 *
 *    This is the main module of the VBI proxy daemon.  Please refer to
 *    the README file for information on the daemon's general purpose.
 *
 *    When started, the daemon will at first only create a named socket in
 *    /tmp for the devices given on the command line and wait for client
 *    connections.  When a client connects the VBI device is opened and
 *    configured for the requested services.  If more clients connect, the
 *    daemon will reset service parameters and add them newly to the slicer
 *    in order of connection times, adapting VBI device parameters as
 *    required and possible (e.g. enlarging VBI window)
 *
 *    Client handling was originally derived from alevtd by Gerd Knorr, then
 *    adapted/extended for nxtvepg and again adapted/reduced for the VBI proxy
 *    by Tom Zoerner.
 *
 *
 *  $Log: proxyd.c,v $
 *  Revision 1.6  2003/06/01 19:33:51  tomzo
 *  Implemented server-side TV channel switching
 *  - implemented messages MSG_TYPE_CHN_CHANGE_REQ/CNF/REJ
 *  - use new function vbi_proxy_take_channel_req(): flush & CHANGE_IND still TODO
 *  - added struct VBIPROXY_CHN_PROFILE to client state struct
 *  Also: added VBI API identifier and device path to CONNECT_CNF (for future use)
 *  Also: adapted message I/O for optimization in proxy-msg.c:
 *  - use static buffer to read messages into, instead of malloc()ed ones
 *
 *  Revision 1.5  2003/05/24 12:16:07  tomzo
 *  - allow multiple -dev arguments on the command line and serve all the given
 *    devices through multiple sockets in /tmp --> split off array of structs
 *    PROXY_DEV from main state struct; clients maintain index into the array
 *  - added support for v4l drivers without select() by using threads to block
 *    in read() --> added mutexes for modifications of device queue and client list
 *  - handle SERVICE_REQ messages from proxy clients to support add_service()
 *    capture interface in io-proxy.c
 *
 *  Revision 1.4  2003/05/17 13:03:41  tomzo
 *  Use new io.h API function vbi_capture_add_services()
 *  - adapted vbi_proxy-update_services(): call add_services() for each client
 *  - removed obsolete function vbi_proxy_merge_parameters()
 *
 *  Revision 1.3  2003/05/10 13:29:43  tomzo
 *  - bugfix: busy loop until the first client connect (unless -nodetach was used)
 *  - copy group and permissions from VBI device onto socket path
 *
 *  Revision 1.2  2003/05/03 12:06:36  tomzo
 *  - removed swap32 inline function from proxyd.c and io-proxy.c: use new macro
 *    VBIPROXY_ENDIAN_MISMATCH instead (contains swapped value of endian magic)
 *  - fixed copyright headers, added description to file headers
 *
 */

static const char rcsid[] = "$Id: proxyd.c,v 1.6 2003/06/01 19:33:51 tomzo Exp $";

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#ifdef ENABLE_PROXY

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>

#include "vbi.h"
#include "io.h"
#include "bcd.h"
#include "proxy-msg.h"

#define dprintf1(fmt, arg...)    do {if (opt_debug_level >= 1) printf("proxyd: " fmt, ## arg);} while (0)
#define dprintf2(fmt, arg...)    do {if (opt_debug_level >= 2) printf("proxyd: " fmt, ## arg);} while (0)

/* Macro to cast (void *) to (int) and backwards without compiler warning
** (note: 64-bit compilers warn when casting a pointer to an int) */
#define  PVOID2INT(X)    ((int)((long)(X)))
#define  INT2PVOID(X)    ((void *)((long)(X)))

/* ----------------------------------------------------------------------------
** This struct is one element in the slicer data queue
*/
typedef struct PROXY_QUEUE_s
{
        struct PROXY_QUEUE_s  * p_next;
        unsigned int            ref_count;
        unsigned int            use_count;

        int                     max_lines;
        int                     line_count;
        double                  timestamp;
        vbi_sliced              lines[1];
} PROXY_QUEUE;

#define QUEUE_ELEM_SIZE(Q,C)  (sizeof(PROXY_QUEUE) + (sizeof(vbi_sliced) * ((C) - 1)))

/* ----------------------------------------------------------------------------
** Declaration of types of internal state variables
*/

/* Note mutex conventions:
** - mutex are only required for v4l devices which do not support select(2),
**   because only then a separate thread is started which blocks in read(2)
** - when both the client chain and a slicer queue mutex is required, the
**   client mutex is acquired first; order is important to prevent deadlocks
** - the master thread locks the client chain mutex only for write access,
**   i.e. if a client is added or removed
*/

/* client connection state */
typedef enum
{
        REQ_STATE_WAIT_CON_REQ,
        REQ_STATE_WAIT_CLOSE,
        REQ_STATE_FORWARD,
        REQ_STATE_SUSPENDED,
        REQ_STATE_CLOSED,
} REQ_STATE;

#define SRV_MAX_DEVICES                 4
#define VBI_MIN_STRICT                 -1
#define VBI_MAX_STRICT                  2
#define VBI_GET_SERVICE_P(PREQ,STRICT)  ((PREQ)->services + (signed)(STRICT) - VBI_MIN_STRICT)

/* this struct holds client-specific state and parameters */
typedef struct PROXY_CLNT_s
{
        struct PROXY_CLNT_s   * p_next;

        REQ_STATE               state;
        VBIPROXY_MSG_STATE      io;
        vbi_bool                endianSwap;
        int                     dev_idx;

        VBIPROXY_MSG_BODY       msg_buf;

        unsigned int            services[VBI_MAX_STRICT - VBI_MIN_STRICT + 1];
        unsigned int            all_services;
        int                     vbi_start[2];
        int                     vbi_count[2];
        int                     buffer_count;
        vbi_bool                buffer_overflow;
        PROXY_QUEUE           * p_sliced;

        VBIPROXY_CHN_PROFILE    chn_profile;

} PROXY_CLNT;

/* this struct holds the state of a device */
typedef struct
{
        const char            * p_dev_name;
        char                  * p_sock_path;
        int                     pipe_fd;

        vbi_capture           * p_capture;
        vbi_raw_decoder       * p_decoder;
        int                     vbi_fd;
        VBI_API_REV             vbi_api;

        unsigned int            scanning;
        int                     max_lines;
        PROXY_QUEUE           * p_sliced;
        PROXY_QUEUE           * p_free;
        PROXY_QUEUE           * p_tmp_buf;

        vbi_bool                use_thread;
        int                     wr_fd;
        vbi_bool                wait_for_exit;
        vbi_bool                thread_active;
        pthread_t               thread_id;
        pthread_cond_t          start_cond;
        pthread_mutex_t         start_mutex;
        pthread_mutex_t         queue_mutex;

} PROXY_DEV;

/* this struct holds the global state of the module */
typedef struct
{
        char                  * listen_ip;
        char                  * listen_port;
        vbi_bool                do_tcp_ip;
        int                     tcp_ip_fd;
        int                     max_conn;
        vbi_bool                should_exit;

        PROXY_CLNT            * p_clnts;
        int                     clnt_count;
        pthread_mutex_t         clnt_mutex;

        PROXY_DEV               dev[4];
        int                     dev_count;

} PROXY_SRV;

#define SRV_REPLY_TIMEOUT       60
#define SRV_STALLED_STATS_INTV  15
#define SRV_BUFFER_COUNT        10

#define DEFAULT_MAX_CLIENTS     10
#define DEFAULT_DEVICE_PATH     "/dev/vbi"

/* ----------------------------------------------------------------------------
** Local variables
*/
static PROXY_SRV      proxy;

static char         * p_opt_log_name = NULL;
static int            opt_log_level = -1;
static int            opt_syslog_level = -1;
static vbi_bool       opt_no_detach = FALSE;
static int            opt_max_clients = DEFAULT_MAX_CLIENTS;
static int            opt_debug_level = 0;

/* ----------------------------------------------------------------------------
** Add one buffer to the tail of a queue
** - slicer queue is organized so that new data is appended to the tail,
**   forwarded data is taken from the head
** - note that a buffer is not released from the slicer queue until all
**   clients have processed it's data; client structs hold a pointer to
**   the first unprocessed (by the respective client) buffer in the queue
*/
static void vbi_proxy_queue_add_tail( PROXY_QUEUE ** q, PROXY_QUEUE * p_buf )
{
   PROXY_QUEUE * p_last;

   dprintf2("queue_add_tail: buffer 0x%lX\n", (long)p_buf);
   p_buf->p_next = NULL;

   if (*q != NULL)
   {
      assert(*q != p_buf);
      p_last = *q;
      while (p_last->p_next != NULL)
         p_last = p_last->p_next;

      p_last->p_next = p_buf;
   }
   else
      *q = p_buf;

   assert((*q != NULL) && ((*q)->p_next != *q));
}

/* ----------------------------------------------------------------------------
** Retrieve one buffer from the queue of unused buffers
*/
static PROXY_QUEUE * vbi_proxy_queue_get_free( PROXY_DEV * p_proxy_dev )
{
   PROXY_QUEUE * p_buf;

   pthread_mutex_lock(&p_proxy_dev->queue_mutex);

   p_buf = p_proxy_dev->p_free;
   if (p_buf != NULL)
   {
      p_proxy_dev->p_tmp_buf = p_buf;
      p_proxy_dev->p_free    = p_buf->p_next;

      pthread_mutex_unlock(&p_proxy_dev->queue_mutex);

      if (p_buf->max_lines != p_proxy_dev->max_lines)
      {  /* max line parameter changed -> re-alloc the buffer */
         p_proxy_dev->p_tmp_buf = NULL;
         free(p_buf);
         p_buf = malloc(QUEUE_ELEM_SIZE(p_buf, p_proxy_dev->max_lines));
         p_buf->max_lines = p_proxy_dev->max_lines;
         p_proxy_dev->p_tmp_buf = p_buf;
      }

      p_buf->p_next     = NULL;
      p_buf->ref_count  = 0;
      p_buf->use_count  = 0;
   }
   else
      pthread_mutex_unlock(&p_proxy_dev->queue_mutex);

   dprintf2("queue_get_free: buffer 0x%lX\n", (long)p_buf);
   return p_buf;
}

/* ----------------------------------------------------------------------------
** Add a buffer to the queue of unused buffers
** - there's no ordering between buffers in the free queue, hence we don't
**   care if the buffer is inserted at head or tail of the queue
*/
static void vbi_proxy_queue_add_free( PROXY_DEV * p_proxy_dev, PROXY_QUEUE * p_buf )
{
   dprintf2("queue_add_free: buffer 0x%lX\n", (long)p_buf);

   p_buf->p_next = p_proxy_dev->p_free;
   p_proxy_dev->p_free = p_buf;
}

/* ----------------------------------------------------------------------------
** Decrease reference counter on a buffer, add back to free queue upon zero
** - called when a buffer has been processed for one client
*/
static void vbi_proxy_queue_release_sliced( PROXY_CLNT * req )
{
   PROXY_QUEUE * p_buf;
   PROXY_DEV   * p_proxy_dev;

   p_proxy_dev = proxy.dev + req->dev_idx;

   p_buf = req->p_sliced;
   req->p_sliced = p_buf->p_next;

   if (p_buf->ref_count > 0)
      p_buf->ref_count -= 1;

   if (p_buf->ref_count == 0)
   {
      assert(p_proxy_dev->p_sliced == p_buf);
      p_proxy_dev->p_sliced = p_buf->p_next;

      /* add the buffer to the free queue */
      p_buf->p_next = p_proxy_dev->p_free;
      p_proxy_dev->p_free = p_buf;
   }
}

/* ----------------------------------------------------------------------------
** Free all resources of all buffers in a queue
** - called upon stop of acquisition for all queues
*/
static void vbi_proxy_queue_free_all( PROXY_QUEUE ** q )
{
   PROXY_QUEUE * p_next;

   while (*q != NULL)
   {
      p_next = (*q)->p_next;
      free(*q);
      *q = p_next;
   }
}

/* ----------------------------------------------------------------------------
** Free the first buffer in the output queue by force
** - required if one client is blocked but others still active
** - client(s) will lose this frame's data
*/
static PROXY_QUEUE * vbi_proxy_queue_force_free( PROXY_DEV * p_proxy_dev )
{
   PROXY_CLNT   * req;

   pthread_mutex_lock(&proxy.clnt_mutex);
   pthread_mutex_lock(&p_proxy_dev->queue_mutex);

   if ((p_proxy_dev->p_free == NULL) && (p_proxy_dev->p_sliced != NULL))
   {
      dprintf2("queue_force_free: buffer 0x%lX\n", (long)p_proxy_dev->p_sliced);

      for (req = proxy.p_clnts; req != NULL; req = req->p_next)
      {
         if (req->p_sliced == p_proxy_dev->p_sliced)
         {
            vbi_proxy_queue_release_sliced(req);
         }
      }
   }

   pthread_mutex_unlock(&p_proxy_dev->queue_mutex);
   pthread_mutex_unlock(&proxy.clnt_mutex);

   return vbi_proxy_queue_get_free(p_proxy_dev);
}

/* ----------------------------------------------------------------------------
** Read sliced data and forward it to all clients
*/
static void vbi_proxyd_forward_data( int dev_idx )
{
   PROXY_QUEUE    * p_buf;
   PROXY_CLNT     * req;
   PROXY_DEV      * p_proxy_dev;
   struct timeval timeout;
   int    res;

   p_proxy_dev = proxy.dev + dev_idx;

   /* unlink a buffer from the free queue */
   p_buf = vbi_proxy_queue_get_free(p_proxy_dev);

   if (p_buf == NULL)
      p_buf = vbi_proxy_queue_force_free(p_proxy_dev);

   if (p_buf != NULL)
   {
      timeout.tv_sec  = 0;
      timeout.tv_usec = 0;

      res = vbi_capture_read_sliced(p_proxy_dev->p_capture, p_buf->lines,
                                    &p_buf->line_count, &p_buf->timestamp, &timeout);
      if (res > 0)
      {
         pthread_mutex_lock(&proxy.clnt_mutex);
         pthread_mutex_lock(&p_proxy_dev->queue_mutex);

         for (req = proxy.p_clnts; req != NULL; req = req->p_next)
         {
            if ( (req->dev_idx == dev_idx) &&
                 (req->state == REQ_STATE_FORWARD) )
            {
               p_buf->ref_count += 1;

               if (req->p_sliced == NULL)
                  req->p_sliced = p_buf;
            }
         }

         pthread_mutex_unlock(&p_proxy_dev->queue_mutex);
         pthread_mutex_unlock(&proxy.clnt_mutex);
      }
      else if (res < 0)
      {
         /* XXX abort upon error (esp. EBUSY) */
         perror("VBI read");
      }

      pthread_mutex_lock(&p_proxy_dev->queue_mutex);

      if (p_buf->ref_count > 0)
         vbi_proxy_queue_add_tail(&p_proxy_dev->p_sliced, p_buf);
      else
         vbi_proxy_queue_add_free(p_proxy_dev, p_buf);

      p_proxy_dev->p_tmp_buf = NULL;
      pthread_mutex_unlock(&p_proxy_dev->queue_mutex);
   }
   else
      dprintf1("forward_data: queue overflow\n");
}

/* ----------------------------------------------------------------------------
** Helper function: calculate timespec for 50ms timeout
*/
static void vbi_proxyd_calc_timeout_ms( struct timespec * p_tsp, int msecs )
{
   struct timeval  tv;

   gettimeofday(&tv, NULL);
   tv.tv_usec += msecs * 1000L;
   if (tv.tv_usec > 1000 * 1000L)
   {
      tv.tv_sec  += 1;
      tv.tv_usec -= 1000 * 1000;
   }
   p_tsp->tv_sec  = tv.tv_sec;
   p_tsp->tv_nsec = tv.tv_usec * 1000;
}

/* ----------------------------------------------------------------------------
** Clean up after thread cancellation: signal waiting master thread
*/
static void vbi_proxyd_acq_thread_cleanup( void * pvoid_arg )
{
   PROXY_DEV * p_proxy_dev;
   int         dev_idx;

   dev_idx     = PVOID2INT(pvoid_arg);
   p_proxy_dev = proxy.dev + dev_idx;

   dprintf2("acq thread cleanup: signaling master (%d)\n", p_proxy_dev->wait_for_exit);

   pthread_mutex_lock(&p_proxy_dev->start_mutex);
   if (p_proxy_dev->wait_for_exit)
   {
      pthread_cond_signal(&p_proxy_dev->start_cond);
   }
   if (p_proxy_dev->p_tmp_buf != NULL)
   {
      vbi_proxy_queue_add_free(p_proxy_dev, p_proxy_dev->p_tmp_buf);
      p_proxy_dev->p_tmp_buf = NULL;
   }
   p_proxy_dev->thread_active = FALSE;
   pthread_mutex_unlock(&p_proxy_dev->start_mutex);
}

/* ----------------------------------------------------------------------------
** Main loop for acquisition thread for devices that don't support select(2)
*/
static void * vbi_proxyd_acq_thread( void * pvoid_arg )
{
   PROXY_DEV * p_proxy_dev;
   int         dev_idx;
   int         ret;
   char        byte_buf[1];
   sigset_t    sigmask;

   dev_idx     = PVOID2INT(pvoid_arg);
   p_proxy_dev = proxy.dev + dev_idx;

   /* block signals which are handled by main thread */
   sigemptyset(&sigmask);
   sigaddset(&sigmask, SIGHUP);
   sigaddset(&sigmask, SIGINT);
   sigaddset(&sigmask, SIGTERM);
   pthread_sigmask(SIG_BLOCK, &sigmask, NULL);

   pthread_cleanup_push(vbi_proxyd_acq_thread_cleanup, pvoid_arg);
   pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
   pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

   p_proxy_dev->thread_active = TRUE;

   pthread_mutex_lock(&p_proxy_dev->start_mutex);
   pthread_cond_signal(&p_proxy_dev->start_cond);
   pthread_mutex_unlock(&p_proxy_dev->start_mutex);

   while (p_proxy_dev->wait_for_exit == FALSE)
   {
      /* read data from the VBI device and append the buffer to all client queues
      ** note: this function blocks in read(2) until data is available */
      vbi_proxyd_forward_data(dev_idx);

      /* wake up the master thread to process client queues */
      ret = write(p_proxy_dev->wr_fd, byte_buf, 1);

      if (ret < 0)
      {
         dprintf1("acq_thread: write to pipe: %d\n", errno);
         break;
      }
      else if (ret != 1)
         dprintf1("acq_thread: pipe overflow\n");
   }

   pthread_cleanup_pop(1);
   pthread_exit(0);

   return NULL;
}

/* ----------------------------------------------------------------------------
** Stop acquisition thread
*/
static void vbi_proxyd_stop_acq_thread( PROXY_DEV * p_proxy_dev )
{
   struct timespec tsp;
   int ret;
   int vbi_fd;

   assert(p_proxy_dev->use_thread);
   pthread_mutex_lock(&p_proxy_dev->start_mutex);

   if (p_proxy_dev->thread_active)
   {
      p_proxy_dev->wait_for_exit = TRUE;
      pthread_cancel(p_proxy_dev->thread_id);

      vbi_proxyd_calc_timeout_ms(&tsp, 50);
      ret = pthread_cond_timedwait(&p_proxy_dev->start_cond, &p_proxy_dev->start_mutex, &tsp);
      if (ret != 0)
      {  /* thread did not stop within 50ms: probably blocked in read with no incoming data */
         /* dirty hack: force to wake up by closing the file handle */
         vbi_fd = vbi_capture_fd(p_proxy_dev->p_capture);
         close(vbi_fd);
         dprintf1("stop_acq_thread: thread did not exit (%d): closed VBI filehandle %d\n", ret, vbi_fd);

         vbi_proxyd_calc_timeout_ms(&tsp, 50);
         ret = pthread_cond_timedwait(&p_proxy_dev->start_cond, &p_proxy_dev->start_mutex, &tsp);
      }
      if (ret == 0)
      {
         ret = pthread_join(p_proxy_dev->thread_id, NULL);
         if (ret == 0)
            dprintf1("stop_acq_thread: acq thread killed sucessfully\n");
         else
            dprintf1("stop_acq_thread: pthread_join failed: %d (%s)\n", errno, strerror(errno));
      }
   }

   close(p_proxy_dev->vbi_fd);
   close(p_proxy_dev->wr_fd);
   p_proxy_dev->vbi_fd = -1;
   p_proxy_dev->wr_fd = -1;
   p_proxy_dev->use_thread = FALSE;

   pthread_mutex_unlock(&p_proxy_dev->start_mutex);
}

/* ----------------------------------------------------------------------------
** Start a thread to block in read(2) for devices that don't support select(2)
*/
static vbi_bool vbi_proxyd_start_acq_thread( int dev_idx )
{
   PROXY_DEV * p_proxy_dev;
   int       pipe_fds[2];
   vbi_bool  result = FALSE;

   p_proxy_dev = proxy.dev + dev_idx;
   p_proxy_dev->use_thread    = TRUE;
   p_proxy_dev->wait_for_exit = FALSE;
   p_proxy_dev->thread_active = FALSE;

   if (pipe(pipe_fds) == 0)
   {
      p_proxy_dev->vbi_fd = pipe_fds[0];
      p_proxy_dev->wr_fd  = pipe_fds[1];

      fcntl(p_proxy_dev->vbi_fd, F_SETFL, O_NONBLOCK);
      fcntl(p_proxy_dev->wr_fd,  F_SETFL, O_NONBLOCK);

      /* start thread */
      pthread_mutex_lock(&p_proxy_dev->start_mutex);
      if (pthread_create(&p_proxy_dev->thread_id, NULL,
                         vbi_proxyd_acq_thread, INT2PVOID(dev_idx)) == 0)
      {
         dprintf1("acquisiton thread started: id %ld, device %d, pipe rd/wr %d/%d\n", (long)p_proxy_dev->thread_id, p_proxy_dev - proxy.dev, p_proxy_dev->vbi_fd, p_proxy_dev->wr_fd);

         /* wait for the slave to report the initialization result */
         pthread_cond_wait(&p_proxy_dev->start_cond, &p_proxy_dev->start_mutex);
         pthread_mutex_unlock(&p_proxy_dev->start_mutex);
         result = TRUE;
      }
      else
         dprintf1("start_acq_thread: pthread_create: %d (%s)\n", errno, strerror(errno));
   }
   else
      dprintf1("start_acq_thread: create pipe: %d (%s)\n", errno, strerror(errno));

   return result;
}

/* ----------------------------------------------------------------------------
** Start VBI acquisition (for the first client)
*/
static vbi_bool vbi_proxy_start_acquisition( int dev_idx, PROXY_CLNT * p_new_req, char ** pp_errorstr )
{
   PROXY_DEV    * p_proxy_dev;
   PROXY_QUEUE  * p_buf;
   char         * p_errorstr;
   unsigned int   tmp_services;
   int            strict;
   int            buf_idx;
   vbi_bool       result;

   p_proxy_dev = proxy.dev + dev_idx;
   result      = FALSE;

   /* assign dummy error string if necessary */
   p_errorstr = NULL;
   if (pp_errorstr == NULL)
      pp_errorstr = &p_errorstr;

   if (p_new_req != NULL)
   {
      for (strict = VBI_MIN_STRICT; strict <= VBI_MAX_STRICT; strict++)
         if (*VBI_GET_SERVICE_P(p_new_req, strict) != 0)
            break;

      tmp_services = *VBI_GET_SERVICE_P(p_new_req, strict);
      p_proxy_dev->vbi_api = VBI_API_V4L2;
      p_proxy_dev->p_capture = vbi_capture_v4l2_new(p_proxy_dev->p_dev_name, p_new_req->buffer_count,
                                                    &tmp_services, strict,
                                                    pp_errorstr, opt_debug_level);
      if (p_proxy_dev->p_capture == NULL)
      {
         tmp_services = *VBI_GET_SERVICE_P(p_new_req, strict);

         p_proxy_dev->vbi_api = VBI_API_V4L1;
         p_proxy_dev->p_capture = vbi_capture_v4l_new(p_proxy_dev->p_dev_name, p_proxy_dev->scanning,
                                                      &tmp_services, strict,
                                                      pp_errorstr, opt_debug_level);

         *VBI_GET_SERVICE_P(p_new_req, strict) = tmp_services;
      }

      if (p_proxy_dev->p_capture != NULL)
      {
         p_proxy_dev->p_decoder = vbi_capture_parameters(p_proxy_dev->p_capture);
         if (p_proxy_dev->p_decoder != NULL)
         {
            p_proxy_dev->max_lines = p_proxy_dev->p_decoder->count[0]
                                   + p_proxy_dev->p_decoder->count[1];
            assert(p_proxy_dev->max_lines > 0);

            /* XXX increase buffer count with number of clients */
            for (buf_idx=0; buf_idx < SRV_BUFFER_COUNT; buf_idx++)
            {
               p_buf = malloc(QUEUE_ELEM_SIZE(p_buf, p_proxy_dev->max_lines));
               p_buf->max_lines = p_proxy_dev->max_lines;
               vbi_proxy_queue_add_free(p_proxy_dev, p_buf);
            }
         }

         /* get file handle for select(2) to wait for VBI data */
         p_proxy_dev->vbi_fd = vbi_capture_get_poll_fd(p_proxy_dev->p_capture);

         if (p_proxy_dev->vbi_fd == -1)
            vbi_proxyd_start_acq_thread(p_proxy_dev - proxy.dev);

         result = TRUE;
      }
   }

   if ((pp_errorstr == &p_errorstr) && (p_errorstr != NULL))
      free(p_errorstr);

   return result;
}

/* ----------------------------------------------------------------------------
** Stop VBI acquisition (after the last client quit)
*/
static void vbi_proxy_stop_acquisition( PROXY_DEV * p_proxy_dev )
{
   if (p_proxy_dev->p_capture != NULL)
   {
      dprintf1("stop_acquisition: stopping (prev. services 0x%X)\n", p_proxy_dev->p_decoder->services);

      if (p_proxy_dev->use_thread)
         vbi_proxyd_stop_acq_thread(p_proxy_dev);

      vbi_capture_delete(p_proxy_dev->p_capture);
      p_proxy_dev->p_capture = NULL;
      p_proxy_dev->p_decoder = NULL;
      p_proxy_dev->vbi_fd = -1;

      vbi_proxy_queue_free_all(&p_proxy_dev->p_free);
      vbi_proxy_queue_free_all(&p_proxy_dev->p_sliced);
   }
}

/* ----------------------------------------------------------------------------
** Update service mask after a client was added or closed
** - TODO: update buffer_count
*/
static vbi_bool vbi_proxy_update_services( int dev_idx, PROXY_CLNT * p_new_req,
                                           int new_req_strict, char ** pp_errorstr )
{
   PROXY_CLNT   * req;
   PROXY_DEV    * p_proxy_dev;
   unsigned int   all_services;
   unsigned int   tmp_services;
   int            strict;
   vbi_bool       result;

   p_proxy_dev = proxy.dev + dev_idx;

   if (p_proxy_dev->p_capture == NULL)
   {  /* open VBI device for new client */

      result = vbi_proxy_start_acquisition(dev_idx, p_new_req, pp_errorstr);
   }
   else
   {  /* capturing already enabled */

      if (p_proxy_dev->use_thread)
         vbi_proxyd_stop_acq_thread(p_proxy_dev);

      all_services = 0;
      for (req = proxy.p_clnts; req != NULL; req = req->p_next)
      {
         if ( (req->dev_idx == dev_idx) &&
              (req->state == REQ_STATE_FORWARD) )
         {
            for (strict = VBI_MIN_STRICT; strict <= VBI_MAX_STRICT; strict++)
            {
               tmp_services = *VBI_GET_SERVICE_P(req, strict);
               if (tmp_services != 0)
               {
                  tmp_services =
                     vbi_capture_add_services( p_proxy_dev->p_capture,
                                               (req == proxy.p_clnts), (req->p_next == NULL),
                                               tmp_services, strict,
                                               /* return error strings only for the new client */
                                               (((req == p_new_req) &&
                                                 (strict == new_req_strict)) ? pp_errorstr : NULL) );

                  all_services |= tmp_services;

                  if (req == p_new_req)
                     *VBI_GET_SERVICE_P(req, strict) &= tmp_services;
               }
            }
         }
      }

      if (all_services != 0)
      {
         p_proxy_dev->max_lines = p_proxy_dev->p_decoder->count[0]
                                + p_proxy_dev->p_decoder->count[1];

         dprintf1("update_services: new service mask 0x%X\n", p_proxy_dev->p_decoder->services);

         if (vbi_capture_get_poll_fd(p_proxy_dev->p_capture) == -1)
            vbi_proxyd_start_acq_thread(dev_idx);
      }
      else
      {  /* no clients remaining -> stop acquisition */
         vbi_proxy_stop_acquisition(p_proxy_dev);
      }
      result = TRUE;
   }
   return result;
}

/* ----------------------------------------------------------------------------
** Process a client's service request
** - either during connect request or later upon service request
*/
static vbi_bool vbi_proxy_take_service_req( PROXY_CLNT * req,
                                            unsigned int new_services, int new_strict,
                                            char * errormsg )
{
   char        * p_errorstr;
   PROXY_DEV   * p_proxy_dev;
   int           strict;
   vbi_bool      result;

   p_proxy_dev = proxy.dev + req->dev_idx;
   p_errorstr  = NULL;

   for (strict = VBI_MIN_STRICT; strict <= VBI_MAX_STRICT; strict++)
      *VBI_GET_SERVICE_P(req, strict) &= ~ new_services;
      
   *VBI_GET_SERVICE_P(req, new_strict) |= new_services;

   result = vbi_proxy_update_services(req->dev_idx, req, new_strict, &p_errorstr);

   if ( (result == FALSE) || (p_proxy_dev->p_decoder == NULL) ||
        (*VBI_GET_SERVICE_P(req, new_strict) == 0) )
   {
      if (p_errorstr != NULL)
      {
         strncpy(errormsg, p_errorstr, VBIPROXY_ERROR_STR_MAX_LENGTH);
         errormsg[VBIPROXY_ERROR_STR_MAX_LENGTH - 1] = 0;
      }
      else if (req->services == 0)
      {
         strncpy(errormsg,
                 _("Sorry, proxy cannot capture any of the requested data services."),
                 VBIPROXY_ERROR_STR_MAX_LENGTH);
         errormsg[VBIPROXY_ERROR_STR_MAX_LENGTH - 1] = 0;
      }
      else
         errormsg[0] = 0;

      result = FALSE;
   }

   if (p_proxy_dev->p_decoder != NULL)
   {  /* keep a copy of the VBI line ranges */
      req->vbi_start[0] = p_proxy_dev->p_decoder->start[0];
      req->vbi_count[0] = p_proxy_dev->p_decoder->count[0];
      req->vbi_start[1] = p_proxy_dev->p_decoder->start[1];
      req->vbi_count[1] = p_proxy_dev->p_decoder->count[1];

      req->all_services = 0;
      for (strict = VBI_MIN_STRICT; strict <= VBI_MAX_STRICT; strict++)
         req->all_services |= *VBI_GET_SERVICE_P(req, strict);
   }
   else
      req->all_services = 0;

   if (req->all_services == 0)
   {  /* no services left on this client -> stop forwarding data */
      dprintf1("take_service_req: suspending client fd %d\n", req->io.sock_fd);
      req->state = REQ_STATE_SUSPENDED;
   }

   if (p_errorstr != NULL)
      free(p_errorstr);

   return result;
}

/* ----------------------------------------------------------------------------
** Process a channel change request
*/
static vbi_bool
vbi_proxy_take_channel_req( PROXY_CLNT * req, int chn_flags,
                            vbi_channel_desc * p_chn_desc,
                            VBIPROXY_CHN_PROFILE * p_chn_profile,
                            vbi_bool * p_has_tuner, int * p_scanning,
                            int * p_errno, uint8_t * p_errbuf )
{
   PROXY_CLNT  * p_walk;
   PROXY_DEV   * p_proxy_dev;
   char        * p_errorstr;
   vbi_bool      result;

   p_proxy_dev = proxy.dev + req->dev_idx;
   p_errorstr  = NULL;
   result      = FALSE;

   /* check for update of channel profile */
   if (p_chn_profile->is_valid)
   {
      memcpy(&req->chn_profile, p_chn_profile, sizeof(req->chn_profile));
   }

   /* check client prio against other proxy clients (sub-prio not considered) */
   for (p_walk = proxy.p_clnts; p_walk != NULL; p_walk = p_walk->p_next)
      if (p_walk->chn_profile.chn_prio > req->chn_profile.chn_prio)
         break;

   /* XXX TODO: go through the scheduler, i.e. consider other background clients */
   if (p_walk == NULL)
   {
      if ( vbi_capture_channel_change(p_proxy_dev->p_capture,
                                      chn_flags, req->chn_profile.chn_prio,
                                      p_chn_desc, p_has_tuner, p_scanning,
                                      &p_errorstr) == 0 )
      {
         /* XXX TODO set flag to skip 1-2 frames - here or in IO modules */
         /* XXX TODO: notify other clients
         ** XXX: do not flush out queues; insert change IND in slicer queue */
         *p_errno = 0;
         result = TRUE;
      }
      else
      {
         if (p_errorstr != NULL)
         {
            strncpy(p_errbuf, p_errorstr, VBIPROXY_ERROR_STR_MAX_LENGTH);
            p_errbuf[VBIPROXY_ERROR_STR_MAX_LENGTH - 1] = 0;
         }
         else
            p_errbuf[0] = 0;
         *p_errno = errno;
      }
   }
   else
   {
      strncpy(p_errbuf, _("Cannot switch channel: device is busy."), VBIPROXY_ERROR_STR_MAX_LENGTH);
      p_errbuf[VBIPROXY_ERROR_STR_MAX_LENGTH - 1] = 0;
      *p_errno = EBUSY;
   }

   return result;
}

/* ----------------------------------------------------------------------------
** Close the connection to the client
** - frees all allocated resources
*/
static void vbi_proxyd_close( PROXY_CLNT * req, vbi_bool close_all )
{
   if (req->state != REQ_STATE_CLOSED)
   {
      dprintf1("close: fd %d\n", req->io.sock_fd);
      vbi_proxy_msg_logger(LOG_INFO, req->io.sock_fd, 0, "closing connection", NULL);

      vbi_proxy_msg_close_io(&req->io);

      pthread_mutex_lock(&proxy.dev[req->dev_idx].queue_mutex);

      while (req->p_sliced != NULL)
      {
         vbi_proxy_queue_release_sliced(req);
      }

      pthread_mutex_unlock(&proxy.dev[req->dev_idx].queue_mutex);

      req->state = REQ_STATE_CLOSED;
   }
}

/* ----------------------------------------------------------------------------
** Initialize a request structure for a new client and add it to the list
*/
static void vbi_proxyd_add_connection( int listen_fd, int dev_idx, vbi_bool isLocal )
{
   PROXY_CLNT * req;
   PROXY_CLNT * p_walk;
   int sock_fd;

   sock_fd = vbi_proxy_msg_accept_connection(listen_fd);
   if (sock_fd != -1)
   {
      req = calloc(sizeof(*req), 1);
      if (req != NULL)
      {
         dprintf1("add_connection: fd %d\n", sock_fd);

         req->state         = REQ_STATE_WAIT_CON_REQ;
         req->io.lastIoTime = time(NULL);
         req->io.sock_fd    = sock_fd;
         req->dev_idx       = dev_idx;

         pthread_mutex_lock(&proxy.clnt_mutex);

         /* append request to the end of the chain
         ** note: order is significant for priority in adding services */
         if (proxy.p_clnts != NULL)
         {
            p_walk = proxy.p_clnts;
            while (p_walk->p_next != NULL)
               p_walk = p_walk->p_next;

            p_walk->p_next = req;
         }
         else
            proxy.p_clnts = req;

         proxy.clnt_count  += 1;

         pthread_mutex_unlock(&proxy.clnt_mutex);
      }
      else
         dprintf1("add_connection: fd %d: virtual memory exhausted, abort\n", sock_fd);
   }
}

/* ----------------------------------------------------------------------------
** Initialize state for a new device
*/
static void vbi_proxyd_add_device( const char * p_dev_name )
{
   PROXY_DEV  * p_proxy_dev;

   if (proxy.dev_count < SRV_MAX_DEVICES)
   {
      p_proxy_dev = proxy.dev + proxy.dev_count;

      p_proxy_dev->p_dev_name  = p_dev_name;
      p_proxy_dev->p_sock_path = vbi_proxy_msg_get_socket_name(p_dev_name);
      p_proxy_dev->pipe_fd = -1;
      p_proxy_dev->vbi_fd  = -1;
      p_proxy_dev->wr_fd   = -1;

      /* initialize synchonization facilities */
      pthread_cond_init(&p_proxy_dev->start_cond, NULL);
      pthread_mutex_init(&p_proxy_dev->start_mutex, NULL);
      pthread_mutex_init(&p_proxy_dev->queue_mutex, NULL);

      proxy.dev_count += 1;
   }
}

/* ----------------------------------------------------------------------------
** Checks the size of a message from client to server
*/
static vbi_bool vbi_proxyd_check_msg( uint len, VBIPROXY_MSG_HEADER * pHead,
                                      VBIPROXY_MSG_BODY * pBody, vbi_bool * pEndianSwap )
{
   vbi_bool result = FALSE;

   switch (pHead->type)
   {
      case MSG_TYPE_CONNECT_REQ:
         if ( (len == sizeof(VBIPROXY_MSG_HEADER) + sizeof(pBody->connect_req)) &&
              (memcmp(pBody->connect_req.magics.protocol_magic, VBIPROXY_MAGIC_STR, VBIPROXY_MAGIC_LEN) == 0) )
         {
            if (pBody->connect_req.magics.endian_magic == VBIPROXY_ENDIAN_MAGIC)
            {
               *pEndianSwap = FALSE;
               result       = TRUE;
            }
            else if (pBody->connect_req.magics.endian_magic == VBIPROXY_ENDIAN_MISMATCH)
            {
               *pEndianSwap = TRUE;
               result       = TRUE;
            }
         }
         break;

      case MSG_TYPE_SERVICE_REQ:
         result = (len == sizeof(VBIPROXY_MSG_HEADER) + sizeof(pBody->service_req));
         break;

      case MSG_TYPE_CHN_CHANGE_REQ:
         result = (len == sizeof(VBIPROXY_MSG_HEADER) + sizeof(pBody->chn_change_req));
         break;

      case MSG_TYPE_CLOSE_REQ:
         result = (len == sizeof(VBIPROXY_MSG_HEADER));
         break;

      case MSG_TYPE_CONNECT_CNF:
      case MSG_TYPE_CONNECT_REJ:
      case MSG_TYPE_SERVICE_CNF:
      case MSG_TYPE_SERVICE_REJ:
      case MSG_TYPE_CHN_CHANGE_CNF:
      case MSG_TYPE_CHN_CHANGE_IND:
      case MSG_TYPE_CHN_CHANGE_REJ:
      case MSG_TYPE_SLICED_IND:
         dprintf2("check_msg: recv client msg #%d at server side\n", pHead->type);
         result = FALSE;
         break;
      default:
         dprintf2("check_msg: unknown msg #%d\n", pHead->type);
         result = FALSE;
         break;
   }

   if (result == FALSE)
           dprintf2("check_msg: illegal msg: len=%d, type=%d", len, pHead->type);

   return result;
}

/* ----------------------------------------------------------------------------
** Handle message from client
** - note: consistancy checks were already done by the I/O handler
**   except for higher level messages (must be checked by acqctl module)
** - implemented as a matrix: "switch" over server state, and "if" cascades
**   over message type
** - XXX warning: inbound messages use the same buffer as outbound!
**   must have finished evaluating the message before assembling the reply
*/
static vbi_bool vbi_proxyd_take_message( PROXY_CLNT *req, VBIPROXY_MSG_BODY * pMsg )
{
   vbi_bool result = FALSE;

   dprintf2("take_message: fd %d: recv msg type %d\n", req->io.sock_fd, req->io.readHeader.type);

   switch (req->io.readHeader.type)
   {
      case MSG_TYPE_CONNECT_REQ:
         if (req->state == REQ_STATE_WAIT_CON_REQ)
         {
            if (pMsg->connect_req.magics.protocol_compat_version == VBIPROXY_COMPAT_VERSION)
            {
               /* if provided, update norm hint (used for first client on ancient v4l1 drivers only) */
               if (pMsg->connect_req.scanning != 0)
                  proxy.dev[req->dev_idx].scanning = pMsg->connect_req.scanning;

               /* XXX TODO */
               req->buffer_count = pMsg->connect_req.buffer_count;

               /* enable forwarding of captured data */
               req->state = REQ_STATE_FORWARD;

               if ( vbi_proxy_take_service_req(req, pMsg->connect_req.services,
                                                    pMsg->connect_req.strict,
                                                    req->msg_buf.connect_rej.errorstr) )
               {  /* open & service initialization succeeded -> reply with confirm */
                  vbi_proxy_msg_fill_magics(&req->msg_buf.connect_cnf.magics);
                  memcpy(&req->msg_buf.connect_cnf.dec,
                         proxy.dev[req->dev_idx].p_decoder,
                         sizeof(req->msg_buf.connect_cnf.dec));
                  strncpy(req->msg_buf.connect_cnf.dev_vbi_name,
                          proxy.dev[req->dev_idx].p_dev_name, VBIPROXY_DEV_NAME_MAX_LENGTH);
                  req->msg_buf.connect_cnf.dev_vbi_name[VBIPROXY_DEV_NAME_MAX_LENGTH - 1] = 0;
                  req->msg_buf.connect_cnf.dec.pattern = NULL;
                  req->msg_buf.connect_cnf.vbi_api_revision = proxy.dev[req->dev_idx].vbi_api;

                  vbi_proxy_msg_write(&req->io, MSG_TYPE_CONNECT_CNF,
                                      sizeof(req->msg_buf.connect_cnf),
                                      &req->msg_buf.connect_cnf, FALSE);
               }
               else
               {
                  vbi_proxy_msg_fill_magics(&req->msg_buf.connect_cnf.magics);

                  vbi_proxy_msg_write(&req->io, MSG_TYPE_CONNECT_REJ,
                                      sizeof(req->msg_buf.connect_rej),
                                      &req->msg_buf.connect_rej, FALSE);

                  /* drop the connection after sending the reject message */
                  req->state = REQ_STATE_WAIT_CLOSE;
               }
            }
            else
            {  /* client uses incompatible protocol version */
               vbi_proxy_msg_fill_magics(&req->msg_buf.connect_cnf.magics);
               strncpy(req->msg_buf.connect_rej.errorstr,
                       _("Incompatible proxy protocol version"), VBIPROXY_ERROR_STR_MAX_LENGTH);
               req->msg_buf.connect_rej.errorstr[VBIPROXY_ERROR_STR_MAX_LENGTH - 1] = 0;
               vbi_proxy_msg_write(&req->io, MSG_TYPE_CONNECT_REJ,
                                   sizeof(req->msg_buf.connect_rej),
                                   &req->msg_buf.connect_rej, FALSE);
               /* drop the connection */
               req->state = REQ_STATE_WAIT_CLOSE;
            }
            result = TRUE;
         }
         break;

      case MSG_TYPE_SERVICE_REQ:
         if ( (req->state == REQ_STATE_FORWARD) ||
              (req->state == REQ_STATE_SUSPENDED) )
         {
            if (pMsg->service_req.reset)
               memset(req->services, 0, sizeof(req->services));

            /* if suspended, enter service state again */
            req->state = REQ_STATE_FORWARD;

            /* flush all buffers in this client's queue */
            pthread_mutex_lock(&proxy.dev[req->dev_idx].queue_mutex);
            while (req->p_sliced != NULL)
            {
               vbi_proxy_queue_release_sliced(req);
            }
            pthread_mutex_unlock(&proxy.dev[req->dev_idx].queue_mutex);

            if ( vbi_proxy_take_service_req(req, pMsg->service_req.services,
                                                 pMsg->service_req.strict,
                                                 req->msg_buf.service_rej.errorstr) )
            {
               memcpy(&req->msg_buf.service_cnf.dec, proxy.dev[req->dev_idx].p_decoder, sizeof(req->msg_buf.service_cnf.dec));
               vbi_proxy_msg_write(&req->io, MSG_TYPE_SERVICE_CNF,
                                   sizeof(req->msg_buf.service_cnf),
                                   &req->msg_buf.service_cnf, FALSE);
            }
            else
            {
               vbi_proxy_msg_write(&req->io, MSG_TYPE_SERVICE_REJ,
                                   sizeof(req->msg_buf.service_rej),
                                   &req->msg_buf.service_rej, FALSE);
            }
            result = TRUE;
         }
         break;

      case MSG_TYPE_CHN_CHANGE_REQ:
         if ( (req->state == REQ_STATE_FORWARD) ||
              (req->state == REQ_STATE_SUSPENDED) )
         {
            uint32_t  serial = pMsg->chn_change_req.serial;
            vbi_bool  has_tuner;
            int scanning;
            int dev_errno;

            if ( vbi_proxy_take_channel_req(req, pMsg->chn_change_req.chn_flags,
                                                 &pMsg->chn_change_req.chn_desc,
                                                 &pMsg->chn_change_req.chn_profile,
                                                 &has_tuner, &scanning, &dev_errno,
                                                 req->msg_buf.chn_change_rej.errorstr) )
            {
               req->msg_buf.chn_change_cnf.serial    = serial;
               req->msg_buf.chn_change_cnf.scanning  = scanning;
               req->msg_buf.chn_change_cnf.has_tuner = has_tuner;
               vbi_proxy_msg_write(&req->io, MSG_TYPE_CHN_CHANGE_CNF,
                                   sizeof(req->msg_buf.chn_change_cnf),
                                   &req->msg_buf.chn_change_cnf, FALSE);
            }
            else
            {
               req->msg_buf.chn_change_rej.serial    = serial;
               req->msg_buf.chn_change_rej.dev_errno = dev_errno;
               vbi_proxy_msg_write(&req->io, MSG_TYPE_CHN_CHANGE_REJ,
                                   sizeof(req->msg_buf.chn_change_rej),
                                   &req->msg_buf.chn_change_rej, FALSE);
            }
            result = TRUE;
         }
         break;

      case MSG_TYPE_CLOSE_REQ:
         /* close the connection */
         vbi_proxyd_close(req, FALSE);
         result = TRUE;
         break;

      default:
         /* unknown message or client-only message */
         dprintf1("take_message: protocol error: unexpected message type %d\n", req->io.readHeader.type);
         break;
   }

   if (result == FALSE)
      dprintf1("take_message: message type %d (len %d) not expected in state %d", req->io.readHeader.type, req->io.readHeader.len, req->state);

   return result;
}

/* ----------------------------------------------------------------------------
** Set bits for all active sockets in fd_set for select syscall
*/
static int vbi_proxyd_get_fd_set( fd_set * rd, fd_set * wr )
{
   PROXY_CLNT   * req;
   PROXY_DEV    * p_proxy_dev;
   int            dev_idx;
   int            max_fd;

   max_fd = 0;

   /* add TCP/IP and UNIX-domain listening sockets */
   if ((proxy.max_conn == 0) || (proxy.clnt_count < proxy.max_conn))
   {
      if (proxy.tcp_ip_fd != -1)
      {
         FD_SET(proxy.tcp_ip_fd, rd);
         if (proxy.tcp_ip_fd > max_fd)
             max_fd = proxy.tcp_ip_fd;
      }
   }

   /* add listening sockets and VBI devices, if currently opened */
   p_proxy_dev = proxy.dev;
   for (dev_idx = 0; dev_idx < proxy.dev_count; dev_idx++, p_proxy_dev++)
   {
      if (p_proxy_dev->pipe_fd != -1)
      {
         FD_SET(p_proxy_dev->pipe_fd, rd);
         if (p_proxy_dev->pipe_fd > max_fd)
             max_fd = p_proxy_dev->pipe_fd;
      }

      if (p_proxy_dev->vbi_fd != -1)
      {
         FD_SET(p_proxy_dev->vbi_fd, rd);
         if (p_proxy_dev->vbi_fd > max_fd)
            max_fd = p_proxy_dev->vbi_fd;
      }
   }

   /* add client connection sockets */
   for (req = proxy.p_clnts; req != NULL; req = req->p_next)
   {
      /* read and write are exclusive and write takes prcedence over read
      ** (i.e. read only if no write is pending or if a read operation has already been started)
      */
      if (req->io.waitRead || (req->io.readLen > 0))
         FD_SET(req->io.sock_fd, rd);
      else
      if ((req->io.writeLen > 0) || (req->p_sliced != NULL))
         FD_SET(req->io.sock_fd, wr);
      else
         FD_SET(req->io.sock_fd, rd);

      if (req->io.sock_fd > max_fd)
          max_fd = req->io.sock_fd;
   }

   return max_fd;
}

/* ----------------------------------------------------------------------------
** Proxy daemon central connection handling
*/
static void vbi_proxyd_handle_sockets( fd_set * rd, fd_set * wr )
{
   PROXY_CLNT    *req;
   PROXY_CLNT    *prev, *tmp;
   vbi_bool      ioBlocked;  /* dummy */
   time_t now = time(NULL);

   /* handle active connections */
   for (req = proxy.p_clnts, prev = NULL; req != NULL; )
   {
      if ( FD_ISSET(req->io.sock_fd, rd) ||
           ((req->io.writeLen > 0) && FD_ISSET(req->io.sock_fd, wr)) )
      {
         req->io.lastIoTime = now;

         if ( vbi_proxy_msg_is_idle(&req->io) )
         {  /* currently no I/O in progress */

            if (FD_ISSET(req->io.sock_fd, rd))
            {  /* new incoming data -> start reading */
               dprintf2("handle_sockets: fd %d: receiving new msg\n", req->io.sock_fd);
               req->io.waitRead = TRUE;
               req->io.readLen  = 0;
               req->io.readOff  = 0;
            }
         }
         if (vbi_proxy_msg_handle_io(&req->io, &ioBlocked, TRUE, &req->msg_buf, sizeof(req->msg_buf)))
         {
            /* check for finished read -> process request */
            if ( (req->io.readLen != 0) && (req->io.readLen == req->io.readOff) )
            {
               if (vbi_proxyd_check_msg(req->io.readLen, &req->io.readHeader, &req->msg_buf, &req->endianSwap))
               {
                  req->io.readLen  = 0;

                  if (vbi_proxyd_take_message(req, &req->msg_buf) == FALSE)
                  {  /* message no accepted (e.g. wrong state) */
                     vbi_proxyd_close(req, FALSE);
                  }
               }
               else
               {  /* message has illegal size or content */
                  vbi_proxyd_close(req, FALSE);
               }
            }
         }
         else
            vbi_proxyd_close(req, FALSE);
      }
      else if (vbi_proxy_msg_is_idle(&req->io))
      {  /* currently no I/O in progress */
         vbi_bool  io_blocked = FALSE;

         if (req->state == REQ_STATE_WAIT_CLOSE)
         {  /* close was pending after last write */
            vbi_proxyd_close(req, FALSE);
         }
         else
         {
            /* forward data from slicer out queue */
            while ((req->p_sliced != NULL) && (io_blocked == FALSE))
            {
               dprintf2("handle_sockets: fd %d: forward sliced frame with %d lines (of max %d)\n", req->io.sock_fd, req->p_sliced->line_count, req->p_sliced->max_lines);
               if (vbi_proxy_msg_write_queue(&req->io, &io_blocked,
                                             req->all_services, req->vbi_count[0] + req->vbi_count[1],
                                             req->p_sliced->lines, req->p_sliced->line_count,
                                             req->p_sliced->timestamp) == FALSE)
               {
                  vbi_proxyd_close(req, FALSE);
                  io_blocked = TRUE;
               }
               else
               {  /* only in success case because close releases all buffers */
                  pthread_mutex_lock(&proxy.dev[req->dev_idx].queue_mutex);
                  vbi_proxy_queue_release_sliced(req);
                  pthread_mutex_unlock(&proxy.dev[req->dev_idx].queue_mutex);
               }
            }
         }

         #if 0
         if ((req->io.sock_fd != -1) && (req->io.writeLen > 0))
         {
            if (vbi_proxy_msg_handle_io(&req->io, &ioBlocked, TRUE, &req->msg_buf, sizeof(req->msg_buf)) == FALSE)
            {
               vbi_proxyd_close(req, FALSE);
               io_blocked = TRUE;
            }
         }
         #endif
      }

      if ((req->io.sock_fd == -1) && (req->state != REQ_STATE_CLOSED))
      {  /* free resources (should be redundant, but does no harm) */
         vbi_proxyd_close(req, FALSE);
      }
      else if (vbi_proxy_msg_check_timeout(&req->io, now))
      {
         dprintf1("handle_sockets: fd %d: i/o timeout in state %d (writeLen=%d, waitRead=%d, readLen=%d, readOff=%d, read msg type=%d)\n", req->io.sock_fd, req->state, req->io.writeLen, req->io.waitRead, req->io.readLen, req->io.readOff, req->io.readHeader.type);
         vbi_proxyd_close(req, FALSE);
      }
      else /* check for protocol or network I/O timeout */
      if ( (now > req->io.lastIoTime + SRV_REPLY_TIMEOUT) &&
           (req->state == REQ_STATE_WAIT_CON_REQ) )
      {
         dprintf1("handle_sockets: fd %d: protocol timeout in state %d\n", req->io.sock_fd, req->state);
         vbi_proxyd_close(req, FALSE);
      }
      else if ( (now > req->io.lastIoTime + SRV_STALLED_STATS_INTV) &&
                (req->state == REQ_STATE_FORWARD) &&
                vbi_proxy_msg_is_idle(&req->io) )
      {
         dprintf1("handle_sockets: fd %d: send 'no reception' stats\n", req->io.sock_fd);
         req->io.lastIoTime = now;
      }

      if (req->state == REQ_STATE_CLOSED)
      {  /* connection was closed after network error */
         int dev_idx = req->dev_idx;
         if (proxy.clnt_count > 0)
            proxy.clnt_count -= 1;
         dprintf1("handle_sockets: closed conn, %d remain\n", proxy.clnt_count);

         pthread_mutex_lock(&proxy.clnt_mutex);
         /* unlink from list */
         tmp = req;
         if (prev == NULL)
         {
            proxy.p_clnts = req->p_next;
            req = proxy.p_clnts;
         }
         else
         {
            prev->p_next = req->p_next;
            req = req->p_next;
         }
         pthread_mutex_unlock(&proxy.clnt_mutex);

         vbi_proxy_update_services(dev_idx, NULL, 0, NULL);
         free(tmp);
      }
      else
      {
         prev = req;
         req = req->p_next;
      }
   }
}

/* ----------------------------------------------------------------------------
** Set maximum number of open client connections
** - note: does not close connections if max count is already exceeded
*/
static void vbi_proxyd_set_max_conn( uint max_conn )
{
   proxy.max_conn = max_conn;
}

/* ----------------------------------------------------------------------------
** Set server IP address
** - must be called before the listening sockets are created
*/
static void vbi_proxyd_set_address( vbi_bool do_tcp_ip, const char * pIpStr, const char * pPort )
{
   /* free the memory allocated for the old config strings */
   if (proxy.listen_ip != NULL)
   {
      free(proxy.listen_ip);
      proxy.listen_ip = NULL;
   }
   if (proxy.listen_port != NULL)
   {
      free(proxy.listen_port);
      proxy.listen_port = NULL;
   }

   /* make a copy of the new config strings */
   if (pIpStr != NULL)
   {
      proxy.listen_ip = malloc(strlen(pIpStr) + 1);
      strcpy(proxy.listen_ip, pIpStr);
   }
   if (pPort != NULL)
   {
      proxy.listen_port = malloc(strlen(pPort) + 1);
      strcpy(proxy.listen_port, pPort);
   }
   proxy.do_tcp_ip = do_tcp_ip;
}

/* ----------------------------------------------------------------------------
** Emulate device permissions on the socket file
*/
static void vbi_proxyd_set_socket_perm( PROXY_DEV * p_proxy_dev )
{
   struct stat st;

   if (stat(p_proxy_dev->p_dev_name, &st) != -1)
   {
      if ( (chown(p_proxy_dev->p_sock_path, st.st_uid, st.st_gid) != 0) &&
           (chown(p_proxy_dev->p_sock_path, geteuid(), st.st_gid) != 0) )
         dprintf1("set_perm: failed to set socket owner %d.%d: %s\n", st.st_uid, st.st_gid, strerror(errno));

      if (chmod(p_proxy_dev->p_sock_path, st.st_mode) != 0)
         dprintf1("set_perm: failed to set socket permission %o: %s\n", st.st_mode, strerror(errno));
   }
   else
      dprintf1("set_perm: failed to stat VBI device %s\n", p_proxy_dev->p_dev_name);
}

/* ----------------------------------------------------------------------------
** Stop the server, close all connections, free resources
*/
static void vbi_proxyd_destroy( void )
{
   PROXY_CLNT  *req, *p_next;
   int  dev_idx;

   /* close all devices */
   for (dev_idx = 0; dev_idx < proxy.dev_count; dev_idx++)
   {
      vbi_proxy_stop_acquisition(proxy.dev + dev_idx);
   }

   /* shutdown all client connections & free resources */
   req = proxy.p_clnts;
   while (req != NULL)
   {
      p_next = req->p_next;
      vbi_proxyd_close(req, TRUE);
      free(req);
      req = p_next;
   }
   proxy.p_clnts = NULL;
   proxy.clnt_count = 0;

   /* close listening sockets */
   for (dev_idx = 0; dev_idx < proxy.dev_count; dev_idx++)
   {
      if (proxy.dev[dev_idx].pipe_fd != -1)
      {
         vbi_proxy_msg_stop_listen(FALSE, proxy.dev[dev_idx].pipe_fd, proxy.dev[dev_idx].p_sock_path);
      }

      if (proxy.dev[dev_idx].p_sock_path != NULL)
         free(proxy.dev[dev_idx].p_sock_path);

      pthread_cond_destroy(&proxy.dev[dev_idx].start_cond);
      pthread_mutex_destroy(&proxy.dev[dev_idx].start_mutex);
      pthread_mutex_destroy(&proxy.dev[dev_idx].queue_mutex);
   }

   if (proxy.tcp_ip_fd != -1)
   {
      vbi_proxy_msg_stop_listen(TRUE, proxy.tcp_ip_fd, NULL);
   }

   vbi_proxy_msg_logger(LOG_NOTICE, -1, 0, "shutting down", NULL);

   /* free the memory allocated for the config strings */
   vbi_proxyd_set_address(FALSE, NULL, NULL);
   vbi_proxy_msg_set_logging(FALSE, 0, 0, NULL);
}

/* ---------------------------------------------------------------------------
** Signal handler to catch deadly signals
*/
static void proxy_signal_handler( int sigval )
{
   char str_buf[10];

   sprintf(str_buf, "%d", sigval);
   vbi_proxy_msg_logger(LOG_NOTICE, -1, 0, "terminated by signal", str_buf, NULL);

   proxy.should_exit = TRUE;
}

/* ----------------------------------------------------------------------------
** Initialize DB server
*/
static void vbi_proxyd_init( void )
{
   struct sigaction  act;

   if (opt_no_detach == FALSE)
   {
      if (fork() > 0)
         exit(0);
      close(0); 
      open("/dev/null", O_RDONLY, 0);

      if (opt_debug_level == 0)
      {
         close(1);
         open("/dev/null", O_WRONLY, 0);
         close(2);
         dup(1);

         setsid();
      }
   }

   /* ignore broken pipes (handled by select/read) */
   memset(&act, 0, sizeof(act));
   act.sa_handler = SIG_IGN;
   sigaction(SIGPIPE, &act, NULL);

   /* catch deadly signals for a clean shutdown (remove socket file) */
   memset(&act, 0, sizeof(act));
   sigemptyset(&act.sa_mask);
   sigaddset(&act.sa_mask, SIGINT);
   sigaddset(&act.sa_mask, SIGTERM);
   sigaddset(&act.sa_mask, SIGHUP);
   act.sa_handler = proxy_signal_handler;
   act.sa_flags = SA_ONESHOT;
   sigaction(SIGINT, &act, NULL);
   sigaction(SIGTERM, &act, NULL);
   sigaction(SIGHUP, &act, NULL);
}

/* ----------------------------------------------------------------------------
** Set up sockets for listening to client requests
*/
static vbi_bool vbi_proxyd_listen( void )
{
   PROXY_DEV  * p_proxy_dev;
   int          dev_idx;
   vbi_bool     result;

   result      = TRUE;
   p_proxy_dev = proxy.dev;

   for (dev_idx = 0; (dev_idx < proxy.dev_count) && result; dev_idx++, p_proxy_dev++)
   {
      if (vbi_proxy_msg_check_connect(p_proxy_dev->p_sock_path) == FALSE)
      {
         /* create named socket in /tmp for listening to local clients */
         p_proxy_dev->pipe_fd = vbi_proxy_msg_listen_socket(FALSE, NULL, p_proxy_dev->p_sock_path);
         if (p_proxy_dev->pipe_fd != -1)
         {
            /* copy VBI device permissions to the listening socket */
            vbi_proxyd_set_socket_perm(p_proxy_dev);

            vbi_proxy_msg_logger(LOG_NOTICE, -1, 0, "started listening on local socket for ", p_proxy_dev->p_dev_name, NULL);
         }
         else
            result = FALSE;
      }
      else
      {
         vbi_proxy_msg_logger(LOG_ERR, -1, 0, "a proxy daemon is already running for ", p_proxy_dev->p_dev_name, NULL);
         result = FALSE;
      }
   }

   if (proxy.do_tcp_ip && result)
   {
      /* create TCP/IP socket */
      proxy.tcp_ip_fd = vbi_proxy_msg_listen_socket(TRUE, proxy.listen_ip, proxy.listen_port);
      if (proxy.tcp_ip_fd != -1)
      {
         vbi_proxy_msg_logger(LOG_NOTICE, -1, 0, "started listening on TCP/IP socket", NULL);
      }
      else
         result = FALSE;
   }

   return result;
}

/* ---------------------------------------------------------------------------
** Proxy main loop
*/
static void vbi_proxyd_main_loop( void )
{
   fd_set  rd, wr;
   int     max_fd;
   int     sel_cnt;
   int     dev_idx;

   while (proxy.should_exit == FALSE)
   {
      FD_ZERO(&rd);
      FD_ZERO(&wr);
      max_fd = vbi_proxyd_get_fd_set(&rd, &wr);

      /* wait for new clients, client messages or VBI device data (indefinitly) */
      sel_cnt = select(((max_fd > 0) ? (max_fd + 1) : 0), &rd, &wr, NULL, NULL);

      if (sel_cnt != -1)
      {
         if (sel_cnt > 0)
            dprintf2("main_loop: select: events on %d sockets\n", sel_cnt);

         for (dev_idx = 0; dev_idx < proxy.dev_count; dev_idx++)
         {
            /* accept new local connections */
            if ((proxy.dev[dev_idx].pipe_fd != -1) && (FD_ISSET(proxy.dev[dev_idx].pipe_fd, &rd)))
            {
               vbi_proxyd_add_connection(proxy.dev[dev_idx].pipe_fd, dev_idx, TRUE);
            }

            if ((proxy.dev[dev_idx].vbi_fd != -1) && (FD_ISSET(proxy.dev[dev_idx].vbi_fd, &rd)))
            {
               if (proxy.dev[dev_idx].use_thread == FALSE)
               {
                  vbi_proxyd_forward_data(dev_idx);
               }
               else
               {
                  char dummy_buf[100];
                  sel_cnt = read(proxy.dev[dev_idx].vbi_fd, dummy_buf, sizeof(dummy_buf));
                  dprintf2("main_loop: read from acq thread dev #%d pipe fd %d: %d errno=%d\n", dev_idx, proxy.dev[dev_idx].vbi_fd, sel_cnt, errno);
               }
            }
         }

         /* accept new TCP/IP connections */
         if ((proxy.tcp_ip_fd != -1) && (FD_ISSET(proxy.tcp_ip_fd, &rd)))
         {
            vbi_proxyd_add_connection(proxy.tcp_ip_fd, 0, FALSE);
         }

         vbi_proxyd_handle_sockets(&rd, &wr);
      }
      else
      {
         if (errno != EINTR)
         {  /* select syscall failed */
            dprintf1("main_loop: select with max. fd %d: %s\n", max_fd, strerror(errno));
            sleep(1);
         }
      }
   }
}

/* ---------------------------------------------------------------------------
** Print usage and exit
*/
static void proxy_usage_exit( const char *argv0, const char *argvn, const char * reason )
{
   fprintf(stderr, "%s: %s: %s\n"
                   "Options:\n"
                   "       -dev <path>         : VBI device path (allowed repeatedly)\n"
                   "       -nodetach           : process remains connected to tty\n"
                   "       -debug <level>      : enable debug output: 1=warnings, 2=all\n"
                   "       -syslog <level>     : enable syslog output\n"
                   "       -loglevel <level>   : log file level\n"
                   "       -logfile <path>     : log file name\n"
                   "       -maxclients <count> : max. number of clients\n"
                   "       -help               : this message\n",
                   argv0, reason, argvn);

   exit(1);
}

/* ---------------------------------------------------------------------------
** Parse numeric value in command line options
*/
static vbi_bool proxy_parse_argv_numeric( char * p_number, int * p_value )
{
   char * p_num_end;

   if (*p_number != 0)
   {
      *p_value = strtol(p_number, &p_num_end, 0);

      return (*p_num_end == 0);
   }
   else
      return FALSE;
}

/* ---------------------------------------------------------------------------
** Parse command line options
*/
static void proxy_parse_argv( int argc, char * argv[] )
{
   struct stat stb;
   int arg_val;
   int arg_idx = 1;

   while (arg_idx < argc)
   {
      if (strcasecmp(argv[arg_idx], "-dev") == 0)
      {
         if (arg_idx + 1 < argc)
         {
            if (proxy.dev_count >= SRV_MAX_DEVICES)
               proxy_usage_exit(argv[0], argv[arg_idx], "too many device paths");
            if (stat(argv[arg_idx + 1], &stb) == -1)
               proxy_usage_exit(argv[0], argv[arg_idx +1], strerror(errno));
            if (!S_ISCHR(stb.st_mode))
               proxy_usage_exit(argv[0], argv[arg_idx +1], "not a character device");
            if (access(argv[arg_idx + 1], R_OK | W_OK) == -1)
               proxy_usage_exit(argv[0], argv[arg_idx +1], "failed to access device");

            vbi_proxyd_add_device(argv[arg_idx + 1]);
            arg_idx += 2;
         }
         else
            proxy_usage_exit(argv[0], argv[arg_idx], "missing mode keyword after");
      }
      else if (strcasecmp(argv[arg_idx], "-debug") == 0)
      {
         if ((arg_idx + 1 < argc) && proxy_parse_argv_numeric(argv[arg_idx + 1], &arg_val))
         {
            opt_debug_level = arg_val;
            arg_idx += 2;
         }
         else
            proxy_usage_exit(argv[0], argv[arg_idx], "missing debug level after");
      }
      else if (strcasecmp(argv[arg_idx], "-nodetach") == 0)
      {
         opt_no_detach = TRUE;
         arg_idx += 1;
      }
      else if (strcasecmp(argv[arg_idx], "-syslog") == 0)
      {
         if ((arg_idx + 1 < argc) && proxy_parse_argv_numeric(argv[arg_idx + 1], &arg_val))
         {
            opt_syslog_level = arg_val;
            arg_idx += 2;
         }
         else
            proxy_usage_exit(argv[0], argv[arg_idx], "missing log level after");
      }
      else if (strcasecmp(argv[arg_idx], "-loglevel") == 0)
      {
         if ((arg_idx + 1 < argc) && proxy_parse_argv_numeric(argv[arg_idx + 1], &arg_val))
         {
            opt_log_level = arg_val;
            arg_idx += 2;
         }
         else
            proxy_usage_exit(argv[0], argv[arg_idx], "missing log level after");
      }
      else if (strcasecmp(argv[arg_idx], "-logfile") == 0)
      {
         if (arg_idx + 1 < argc)
         {
            p_opt_log_name = argv[arg_idx + 1];
            arg_idx += 2;
         }
         else
            proxy_usage_exit(argv[0], argv[arg_idx], "missing mode keyword after");
      }
      else if (strcasecmp(argv[arg_idx], "-maxclients") == 0)
      {
         if ((arg_idx + 1 < argc) && proxy_parse_argv_numeric(argv[arg_idx + 1], &arg_val))
         {
            opt_max_clients = arg_val;
            arg_idx += 2;
         }
         else
            proxy_usage_exit(argv[0], argv[arg_idx], "missing log level after");
      }
      else if (strcasecmp(argv[arg_idx], "-help") == 0)
      {
         char versbuf[50];
         sprintf(versbuf, "(version %d.%d.%d)", VBIPROXY_VERSION>>16, (VBIPROXY_VERSION>>8)&0xff, VBIPROXY_VERSION&0xff);
         proxy_usage_exit(argv[0], versbuf, "the following options are available");
      }
      else
         proxy_usage_exit(argv[0], argv[arg_idx], "unknown option or argument");
   }

   /* if no device was given, use default path */
   if (proxy.dev_count == 0)
   {
      vbi_proxyd_add_device(DEFAULT_DEVICE_PATH);
   }
}

/* ----------------------------------------------------------------------------
** Proxy daemon entry point
*/
int main( int argc, char ** argv )
{
   /* initialize state struct */
   memset(&proxy, 0, sizeof(proxy));
   proxy.tcp_ip_fd = -1;
   pthread_mutex_init(&proxy.clnt_mutex, NULL);

   proxy_parse_argv(argc, argv);

   dprintf1("proxy daemon starting, rev.\n%s\n", rcsid);

   vbi_proxyd_init();

   vbi_proxyd_set_max_conn(opt_max_clients);
   vbi_proxyd_set_address(FALSE, NULL, NULL);
   vbi_proxy_msg_set_debug_level(opt_debug_level);
   vbi_proxy_msg_set_logging(opt_debug_level > 0, opt_syslog_level, opt_log_level, p_opt_log_name);

   /* start listening for client connections */
   if (vbi_proxyd_listen())
   {
      vbi_proxyd_main_loop();
   }
   vbi_proxyd_destroy();
   pthread_mutex_destroy(&proxy.clnt_mutex);

   exit(0);
   return 0;
}

#endif  /* ENABLE_PROXY */
