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
 *    /tmp for the device given on the command line and wait for client
 *    connections.  When a client connects the VBI device is opened and
 *    configured for the requested services.  If more clients connect, the
 *    daemon will attempt to merge the service parameters and re-configure
 *    the device if necessary.
 *
 *    Socket and client handling was originally derived from alevtd by
 *    Gerd Knorr, then adapted/extended for nxtvepg and again adapted/reduced
 *    for the VBI proxy by Tom Zoerner.
 *
 *
 *  $Log: proxyd.c,v $
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

static const char rcsid[] = "$Id: proxyd.c,v 1.3 2003/05/10 13:29:43 tomzo Exp $";

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#include "vbi.h"
#include "io.h"

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

#include "bcd.h"
#include "proxy-msg.h"

#define dprintf1(fmt, arg...)    if (opt_debug_level >= 1) printf("proxyd: " fmt, ## arg)
#define dprintf2(fmt, arg...)    if (opt_debug_level >= 2) printf("proxyd: " fmt, ## arg)

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

typedef enum
{
        REQ_STATE_WAIT_CON_REQ,
        REQ_STATE_WAIT_CLOSE,
        REQ_STATE_FORWARD,
        REQ_STATE_CLOSED,
} REQ_STATE;

/* this struct holds client-specific state and parameters */
typedef struct PROXY_CLNT_s
{
        struct PROXY_CLNT_s   * p_next;

        REQ_STATE               state;
        VBIPROXY_MSG_STATE      io;
        vbi_bool                endianSwap;

        VBIPROXY_MSG_BODY       msg_buf;

        unsigned int            scanning;
        unsigned int            services;
        int                     strict;
        int                     vbi_start[2];
        int                     vbi_count[2];
        int                     buffer_count;
        vbi_bool                buffer_overflow;
        PROXY_QUEUE           * p_sliced;

} PROXY_CLNT;

/* this struct holds the global state of the module */
typedef struct
{
        char                  * listen_ip;
        char                  * listen_port;
        char                  * p_sock_path;
        vbi_bool                do_tcp_ip;
        int                     tcp_ip_fd;
        int                     pipe_fd;
        int                     max_conn;
        int                     con_count;
        vbi_bool                should_exit;

        vbi_capture           * p_capture;
        vbi_raw_decoder       * p_decoder;
        int                     vbi_fd;

        int                     max_lines;
        PROXY_QUEUE           * p_sliced;
        PROXY_QUEUE           * p_free;
} PROXY_SRV;

#define SRV_REPLY_TIMEOUT       60
#define SRV_STALLED_STATS_INTV  15
#define SRV_BUFFER_COUNT        10

#define DEFAULT_MAX_CLIENTS     10
#define DEFAULT_DEVICE_PATH     "/dev/vbi0"

/* ----------------------------------------------------------------------------
** Local variables
*/
static PROXY_CLNT   * pReqChain = NULL;
static PROXY_SRV      proxy;

static char         * p_dev_name = DEFAULT_DEVICE_PATH;
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
static PROXY_QUEUE * vbi_proxy_queue_get_free( void )
{
   PROXY_QUEUE * p_buf;

   p_buf = proxy.p_free;
   if (p_buf != NULL)
   {
      proxy.p_free = p_buf->p_next;

      if (p_buf->max_lines != proxy.max_lines)
      {  /* max line parameter changed -> re-alloc the buffer */
         free(p_buf);
         p_buf = malloc(QUEUE_ELEM_SIZE(p_buf, proxy.max_lines));
         p_buf->max_lines = proxy.max_lines;
      }

      p_buf->p_next     = NULL;
      p_buf->ref_count  = 0;
      p_buf->use_count  = 0;
   }

   dprintf2("queue_get_free: buffer 0x%lX\n", (long)p_buf);
   return p_buf;
}

/* ----------------------------------------------------------------------------
** Add a buffer to the queue of unused buffers
** - there's no ordering between buffers in the free queue, hence we don't
**   care if the buffer is inserted at head or tail of the queue
*/
static void vbi_proxy_queue_add_free( PROXY_QUEUE * p_buf )
{
   dprintf2("queue_add_free: buffer 0x%lX\n", (long)p_buf);
   p_buf->p_next = proxy.p_free;
   proxy.p_free = p_buf;
}

/* ----------------------------------------------------------------------------
** Decrease reference counter on a buffer, add back to free queue upon zero
** - called when a buffer has been processed for one client
*/
static void vbi_proxy_queue_release_sliced( PROXY_CLNT * req )
{
   PROXY_QUEUE * p_buf;

   p_buf = req->p_sliced;
   req->p_sliced = p_buf->p_next;

   if (p_buf->ref_count > 0)
      p_buf->ref_count -= 1;

   if (p_buf->ref_count == 0)
   {
      assert(proxy.p_sliced == p_buf);
      proxy.p_sliced = p_buf->p_next;

      vbi_proxy_queue_add_free(p_buf);
   }
}

/* ----------------------------------------------------------------------------
** Free all resources of all buffers in a queue
** - called upon stop of acquisition for all queues
*/
static void vbi_proxy_queue_free_all( PROXY_QUEUE ** q )
{
   PROXY_QUEUE * p_buf;

   while (*q != NULL)
   {
      p_buf = (*q)->p_next;
      free(*q);
      *q = p_buf;
   }
}

/* ----------------------------------------------------------------------------
** Free the first buffer in the output queue by force
** - required if one client is blocked but others still active
** - client(s) will lose this frame's data
*/
static PROXY_QUEUE * vbi_proxy_queue_force_free( void )
{
   PROXY_CLNT   * req;

   if ((proxy.p_free == NULL) && (proxy.p_sliced != NULL))
   {
      dprintf2("queue_force_free: buffer 0x%lX\n", (long)proxy.p_sliced);

      for (req = pReqChain; req != NULL; req = req->p_next)
      {
         if (req->p_sliced == proxy.p_sliced)
         {
            vbi_proxy_queue_release_sliced(req);
         }
      }
   }
   return vbi_proxy_queue_get_free();
}

/* ----------------------------------------------------------------------------
** Merge service parameters from all clients
*/
static void vbi_proxy_merge_parameters( unsigned int * p_services,
                                        int          * p_buffer_count,
                                        int          * p_strict,
                                        int          * p_scanning )
{
   PROXY_CLNT   * req;

   *p_buffer_count = 0;
   *p_services = 0;
   for (req = pReqChain; req != NULL; req = req->p_next)
   {
      *p_services |= req->services;

      if (req->buffer_count > *p_buffer_count)
         *p_buffer_count = req->buffer_count;
   }

   if (*p_buffer_count < 1)
      *p_buffer_count = 1;

   /* XXX FIXME */
   *p_scanning = 0;
   *p_strict = 0;
}

/* ----------------------------------------------------------------------------
** Start VBI acquisition (for the first client)
*/
static vbi_bool vbi_proxy_start_acquisition( char ** pp_errorstr )
{
   PROXY_QUEUE * p_buf;
   char        * p_errorstr;
   vbi_bool     result;
   unsigned int services;
   unsigned int tmp_services;
   int          buffer_count;
   int          strict;
   int          scanning;
   int          idx;

   /* assign dummy error string if necessary */
   p_errorstr = NULL;
   if (pp_errorstr == NULL)
      pp_errorstr = &p_errorstr;

   vbi_proxy_merge_parameters(&services, &buffer_count, &strict, &scanning);

   tmp_services = services;
   proxy.p_capture = vbi_capture_v4l2_new(p_dev_name, buffer_count, &tmp_services, strict, pp_errorstr, opt_debug_level);
   if (proxy.p_capture == NULL)
   {
      tmp_services = services;
      proxy.p_capture = vbi_capture_v4l_new(p_dev_name, scanning, &tmp_services, strict, pp_errorstr, opt_debug_level);
   }

   if (proxy.p_capture != NULL)
   {
      proxy.p_decoder = vbi_capture_parameters(proxy.p_capture);
      if (proxy.p_decoder != NULL)
      {
         proxy.max_lines = proxy.p_decoder->count[0] + proxy.p_decoder->count[1];
         assert(proxy.max_lines > 0);

         for (idx=0; idx < SRV_BUFFER_COUNT; idx++)  /* XXX increase with number of clients */
         {
            p_buf = malloc(QUEUE_ELEM_SIZE(p_buf, proxy.max_lines));
            p_buf->max_lines = proxy.max_lines;
            vbi_proxy_queue_add_free(p_buf);
         }
      }

      /* get file handle for select(2) to wait for VBI data */
      proxy.vbi_fd = vbi_capture_fd(proxy.p_capture);

      result = TRUE;
   }
   else
      result = FALSE;

   if ((pp_errorstr == &p_errorstr) && (p_errorstr != NULL))
      free(p_errorstr);

   return result;
}

/* ----------------------------------------------------------------------------
** Stop VBI acquisition (after the last client quit)
*/
static void vbi_proxy_stop_acquisition( void )
{
   if (proxy.p_capture != NULL)
   {
      dprintf1("stop_acquisition: stopping (prev. services 0x%X)\n", proxy.p_decoder->services);

      vbi_raw_decoder_destroy(proxy.p_decoder);
      vbi_capture_delete(proxy.p_capture);
      proxy.p_capture = NULL;
      proxy.p_decoder = NULL;
      proxy.vbi_fd = -1;

      vbi_proxy_queue_free_all(&proxy.p_free);
      vbi_proxy_queue_free_all(&proxy.p_sliced);
   }
}

/* ----------------------------------------------------------------------------
** Update service mask after a client was added or closed
** - TODO: update buffer_count; allocate new VBI lines
*/
static vbi_bool vbi_proxy_update_services( char ** pp_errorstr )
{
   unsigned int services;
   int          buffer_count;
   int          strict;
   int          scanning;
   vbi_bool     result;

   if (proxy.con_count > 0)
   {
      if (proxy.p_capture == NULL)
      {
         /* open VBI device */
         result = vbi_proxy_start_acquisition(pp_errorstr);
      }
      else
      {  /* capturing already enabled */

         vbi_proxy_merge_parameters(&services, &buffer_count, &strict, &scanning);

         if (proxy.p_decoder->services & ~ services)
         {  /* remove obsolete services */
            dprintf1("update_services: removing 0x%X\n", proxy.p_decoder->services & ~ services);
            proxy.p_decoder->services =
               vbi_raw_decoder_remove_services(proxy.p_decoder, proxy.p_decoder->services & ~ services);
         }
         if ((proxy.p_decoder->services & services) != services)
         {  /* add new services */
            dprintf1("update_services: adding 0x%X\n", services & ~ proxy.p_decoder->services);
            proxy.p_decoder->services =
               vbi_raw_decoder_add_services(proxy.p_decoder, services & ~ proxy.p_decoder->services, strict);
         }
         dprintf1("update_services: new service mask 0x%X\n", proxy.p_decoder->services);
         result = TRUE;
      }
   }
   else
   {  /* last client -> stop acquisition */
      vbi_proxy_stop_acquisition();
      result = TRUE;
   }
   return result;
}

/* ----------------------------------------------------------------------------
** Read sliced data it and forward to all clients
*/
static void vbi_proxyd_forward_data( void )
{
   PROXY_QUEUE    * p_buf;
   PROXY_CLNT     * req;
   struct timeval timeout;
   int    res;

   /* unlink a buffer from the free queue */
   p_buf = vbi_proxy_queue_get_free();

   if ((p_buf == NULL) && (proxy.con_count > 1))
      p_buf = vbi_proxy_queue_force_free();

   if (p_buf != NULL)
   {
      timeout.tv_sec  = 0;
      timeout.tv_usec = 0;

      res = vbi_capture_read_sliced(proxy.p_capture, p_buf->lines,
                                    &p_buf->line_count, &p_buf->timestamp, &timeout);
      if (res > 0)
      {
         for (req = pReqChain; req != NULL; req = req->p_next)
         {
            if (req->state == REQ_STATE_FORWARD)
            {
               p_buf->ref_count += 1;
               if (req->p_sliced == NULL)
                  req->p_sliced = p_buf;
            }
         }
      }
      else if (res < 0)
      {
         perror("VBI read");
      }

      if (p_buf->ref_count > 0)
         vbi_proxy_queue_add_tail(&proxy.p_sliced, p_buf);
      else
         vbi_proxy_queue_add_free(p_buf);
   }
   else
      dprintf1("forward_data: queue overflow\n");
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

      while (req->p_sliced != NULL)
      {
         vbi_proxy_queue_release_sliced(req);
      }

      req->state = REQ_STATE_CLOSED;
   }
}

/* ----------------------------------------------------------------------------
** Initialize a request structure for a new client and add it to the list
*/
static void vbi_proxyd_add_connection( int listen_fd, vbi_bool isLocal )
{
   PROXY_CLNT * req;
   int sock_fd;

   sock_fd = vbi_proxy_msg_accept_connection(listen_fd);
   if (sock_fd != -1)
   {
      dprintf1("add_connection: fd %d\n", sock_fd);

      req = malloc(sizeof(PROXY_CLNT));
      memset(req, 0, sizeof(PROXY_CLNT));

      req->state         = REQ_STATE_WAIT_CON_REQ;
      req->io.lastIoTime = time(NULL);
      req->io.sock_fd    = sock_fd;

      /* insert request into the chain */
      req->p_next = pReqChain;
      pReqChain  = req;
      proxy.con_count  += 1;
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

      case MSG_TYPE_CLOSE_REQ:
         result = (len == sizeof(VBIPROXY_MSG_HEADER));
         break;

      case MSG_TYPE_CONNECT_CNF:
      case MSG_TYPE_DATA_IND:
         dprintf2("check_msg: recv client msg type %d\n", pHead->type);
         result = FALSE;
         break;
      default:
         dprintf2("check_msg: unknown msg type %d\n", pHead->type);
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
               char    * p_errorstr = NULL;
               vbi_bool  open_result;

               /* copy service request parameters */
               req->scanning     = pMsg->connect_req.scanning;
               req->services     = pMsg->connect_req.services;
               req->strict       = pMsg->connect_req.strict;
               req->buffer_count = pMsg->connect_req.buffer_count;
               open_result = vbi_proxy_update_services(&p_errorstr);
               if (proxy.p_decoder != NULL)
                  req->services &= proxy.p_decoder->services;

               if ( (open_result != FALSE) && (proxy.p_decoder != NULL) &&
                    (req->services != 0) )
               {  /* open & service initialization succeeded -> reply with confirm */
                  vbi_proxy_msg_fill_magics(&req->msg_buf.connect_cnf.magics);

                  memcpy(&req->msg_buf.connect_cnf.dec, proxy.p_decoder, sizeof(req->msg_buf.connect_cnf.dec));
                  /* keep a copy of the VBI line ranges */
                  req->vbi_start[0] = proxy.p_decoder->start[0];
                  req->vbi_count[0] = proxy.p_decoder->count[0];
                  req->vbi_start[1] = proxy.p_decoder->start[1];
                  req->vbi_count[1] = proxy.p_decoder->count[1];

                  vbi_proxy_msg_write(&req->io, MSG_TYPE_CONNECT_CNF, sizeof(req->msg_buf.connect_cnf), &req->msg_buf.connect_cnf, FALSE);

                  /* enable forwarding of captured data */
                  req->state = REQ_STATE_FORWARD;
               }
               else
               {
                  vbi_proxy_msg_fill_magics(&req->msg_buf.connect_cnf.magics);
                  if (p_errorstr != NULL)
                  {
                     strncpy(req->msg_buf.connect_rej.errorstr, p_errorstr, VBIPROXY_ERROR_STR_MAX_LENGTH);
                     req->msg_buf.connect_rej.errorstr[VBIPROXY_ERROR_STR_MAX_LENGTH - 1] = 0;
                  }
                  else if (req->services == 0)
                  {
                     strncpy(req->msg_buf.connect_rej.errorstr,
                             _("Sorry, proxy cannot capture any of the requested data services."),
                             VBIPROXY_ERROR_STR_MAX_LENGTH);
                     req->msg_buf.connect_rej.errorstr[VBIPROXY_ERROR_STR_MAX_LENGTH - 1] = 0;
                  }
                  else
                     req->msg_buf.connect_rej.errorstr[0] = 0;

                  vbi_proxy_msg_write(&req->io, MSG_TYPE_CONNECT_REJ, sizeof(req->msg_buf.connect_rej), &req->msg_buf.connect_cnf, FALSE);

                  /* drop the connection */
                  req->state = REQ_STATE_WAIT_CLOSE;
               }

               if (p_errorstr != NULL)
                  free(p_errorstr);
            }
            else
            {  /* client uses incompatible protocol version */
               vbi_proxy_msg_fill_magics(&req->msg_buf.connect_cnf.magics);
               strncpy(req->msg_buf.connect_rej.errorstr,
                       _("Incompatible proxy protocol version"), VBIPROXY_ERROR_STR_MAX_LENGTH);
               req->msg_buf.connect_rej.errorstr[VBIPROXY_ERROR_STR_MAX_LENGTH - 1] = 0;
               vbi_proxy_msg_write(&req->io, MSG_TYPE_CONNECT_REJ, sizeof(req->msg_buf.connect_rej), &req->msg_buf.connect_cnf, FALSE);
               /* drop the connection */
               req->state = REQ_STATE_WAIT_CLOSE;
            }
            result = TRUE;
         }
         break;

      case MSG_TYPE_CLOSE_REQ:
         /* close the connection */
         vbi_proxyd_close(req, FALSE);
         /* message was freed in close function */
         pMsg = NULL;
         result = TRUE;
         break;

      default:
         /* unknown message or client-only message */
         dprintf1("take_message: protocol error: unexpected message type %d\n", req->io.readHeader.type);
         break;
   }

   if (result == FALSE)
      dprintf1("take_message: message type %d (len %d) not expected in state %d", req->io.readHeader.type, req->io.readHeader.len, req->state);
   if (pMsg != NULL)
      free(pMsg);

   return result;
}

/* ----------------------------------------------------------------------------
** Set bits for all active sockets in fd_set for select syscall
*/
static uint vbi_proxyd_get_fd_set( fd_set * rd, fd_set * wr )
{
   PROXY_CLNT    *req;
   uint           max;

   /* add TCP/IP and UNIX-domain listening sockets */
   max = 0;
   if ((proxy.max_conn == 0) || (proxy.con_count < proxy.max_conn))
   {
      if (proxy.tcp_ip_fd != -1)
      {
         FD_SET(proxy.tcp_ip_fd, rd);
         if (proxy.tcp_ip_fd > max)
             max = proxy.tcp_ip_fd;
      }
      if (proxy.pipe_fd != -1)
      {
         FD_SET(proxy.pipe_fd, rd);
         if (proxy.pipe_fd > max)
             max = proxy.pipe_fd;
      }
   }

   /* add client connection sockets */
   for (req = pReqChain; req != NULL; req = req->p_next)
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

      if (req->io.sock_fd > max)
          max = req->io.sock_fd;
   }

   return max;
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

   /* accept new TCP/IP connections */
   if ((proxy.tcp_ip_fd != -1) && (FD_ISSET(proxy.tcp_ip_fd, rd)))
   {
      vbi_proxyd_add_connection(proxy.tcp_ip_fd, FALSE);
   }

   /* accept new local connections */
   if ((proxy.pipe_fd != -1) && (FD_ISSET(proxy.pipe_fd, rd)))
   {
      vbi_proxyd_add_connection(proxy.pipe_fd, TRUE);
   }

   /* handle active connections */
   for (req = pReqChain, prev = NULL; req != NULL; )
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
         if (vbi_proxy_msg_handle_io(&req->io, &ioBlocked, TRUE))
         {
            /* check for finished read -> process request */
            if ( (req->io.readLen != 0) && (req->io.readLen == req->io.readOff) )
            {
               if (vbi_proxyd_check_msg(req->io.readLen, &req->io.readHeader, (VBIPROXY_MSG_BODY *) req->io.pReadBuf, &req->endianSwap))
               {
                  req->io.readLen  = 0;

                  if (vbi_proxyd_take_message(req, (VBIPROXY_MSG_BODY *) req->io.pReadBuf) == FALSE)
                  {  /* message no accepted (e.g. wrong state) */
                     req->io.pReadBuf = NULL;
                     vbi_proxyd_close(req, FALSE);
                  }
                  else  /* ok */
                     req->io.pReadBuf = NULL;
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
               dprintf2("handle_sockets: fd %d: forward sliced frame with %d lines (of max %d)\n", req->io.sock_fd, req->p_sliced->line_count, proxy.max_lines);
               if (vbi_proxy_msg_write_queue(&req->io, &io_blocked,
                                             req->services, req->vbi_count[0] + req->vbi_count[1],
                                             req->p_sliced->lines, req->p_sliced->line_count,
                                             req->p_sliced->timestamp) == FALSE)
               {
                  vbi_proxyd_close(req, FALSE);
                  io_blocked = TRUE;
               }
               else  /* only in success case because close releases all buffers */
                  vbi_proxy_queue_release_sliced(req);
            }
         }

         #if 0
         if ((req->io.sock_fd != -1) && (req->io.writeLen > 0))
         {
            if (vbi_proxy_msg_handle_io(&req->io, &ioBlocked, TRUE) == FALSE)
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
         proxy.con_count -= 1;
         dprintf1("handle_sockets: closed conn, %d remain\n", proxy.con_count);
         /* unlink from list */
         tmp = req;
         if (prev == NULL)
         {
            pReqChain = req->p_next;
            req = pReqChain;
         }
         else
         {
            prev->p_next = req->p_next;
            req = req->p_next;
         }
         free(tmp);

         vbi_proxy_update_services(NULL);
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
static void proxy_set_perm( void )
{
   struct stat st;

   if (stat(p_dev_name, &st) != -1)
   {
      if ( (chown(proxy.p_sock_path, st.st_uid, st.st_gid) != 0) &&
           (chown(proxy.p_sock_path, geteuid(), st.st_gid) != 0) )
         dprintf1("set_perm: failed to set socket owner %d.%d: %s\n", st.st_uid, st.st_gid, strerror(errno));

      if (chmod(proxy.p_sock_path, st.st_mode) != 0)
         dprintf1("set_perm: failed to set socket permission %o: %s\n", st.st_mode, strerror(errno));
   }
   else
      dprintf1("set_perm: failed to stat VBI device %s\n", p_dev_name);
}

/* ----------------------------------------------------------------------------
** Stop the server, close all connections, free resources
*/
static void vbi_proxyd_destroy( void )
{
   PROXY_CLNT  *pReq, *p_next;

   /* shutdown all client connections & free resources */
   pReq = pReqChain;
   while (pReq != NULL)
   {
      p_next = pReq->p_next;
      vbi_proxyd_close(pReq, TRUE);
      free(pReq);
      pReq = p_next;
   }
   pReqChain = NULL;
   proxy.con_count = 0;

   vbi_proxy_stop_acquisition();

   /* close listening sockets */
   if (proxy.pipe_fd != -1)
   {
      vbi_proxy_msg_stop_listen(FALSE, proxy.pipe_fd, proxy.p_sock_path);
      vbi_proxy_msg_logger(LOG_NOTICE, -1, 0, "shutting down", NULL);
   }
   if (proxy.tcp_ip_fd != -1)
   {
      vbi_proxy_msg_stop_listen(TRUE, proxy.pipe_fd, proxy.p_sock_path);
   }

   /* free the memory allocated for the config strings */
   vbi_proxyd_set_address(FALSE, NULL, NULL);
   vbi_proxy_msg_set_logging(FALSE, 0, 0, NULL);

   if (proxy.p_sock_path != NULL)
      free(proxy.p_sock_path);
}

/* ---------------------------------------------------------------------------
** Signal handler
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

   /* setup signal handler */
   memset(&act, 0, sizeof(act));
   sigemptyset(&act.sa_mask);
   act.sa_handler = SIG_IGN;
   sigaction(SIGPIPE, &act, NULL);

   /* catch deadly signals for a clean shutdown (clear socket file) */
   signal(SIGINT, proxy_signal_handler);
   signal(SIGTERM, proxy_signal_handler);
   signal(SIGHUP, proxy_signal_handler);

   /* initialize state struct */
   memset(&proxy, 0, sizeof(proxy));
   proxy.pipe_fd = -1;
   proxy.tcp_ip_fd = -1;
   proxy.vbi_fd = -1;

   proxy.p_sock_path = vbi_proxy_msg_get_socket_name(p_dev_name);
}

/* ----------------------------------------------------------------------------
** Set up sockets for listening to client requests
** - XXX TODO set permissions for named socket to same as VBI device
*/
static vbi_bool vbi_proxyd_listen( void )
{
   vbi_bool result = FALSE;

   if (vbi_proxy_msg_check_connect(proxy.p_sock_path) == FALSE)
   {
      /* create named socket in /tmp for listening to local clients */
      proxy.pipe_fd = vbi_proxy_msg_listen_socket(FALSE, NULL, proxy.p_sock_path);
      if (proxy.pipe_fd != -1)
      {
         if (proxy.do_tcp_ip)
         {
            /* create TCP/IP socket */
            proxy.tcp_ip_fd = vbi_proxy_msg_listen_socket(TRUE, proxy.listen_ip, proxy.listen_port);
            if (proxy.tcp_ip_fd != -1)
            {
               vbi_proxy_msg_logger(LOG_NOTICE, -1, 0, "started listening on local and TCP/IP socket", NULL);
               result = TRUE;
            }
         }
         else
         {  /* no TCP/IP socket requested */
            vbi_proxy_msg_logger(LOG_NOTICE, -1, 0, "started listening on local socket", NULL);
            result = TRUE;
         }
      }
   }
   else
      vbi_proxy_msg_logger(LOG_ERR, -1, 0, "a nxtvepg daemon is already running", NULL);

   return result;
}

/* ---------------------------------------------------------------------------
** Proxy main loop
*/
static void proxy_main_loop( void )
{
   struct timeval timeout;
   fd_set  rd, wr;
   int     max;
   int     selSockCnt;

   while (proxy.should_exit == FALSE)
   {
      FD_ZERO(&rd);
      FD_ZERO(&wr);
      max = vbi_proxyd_get_fd_set(&rd, &wr);

      if (proxy.vbi_fd != -1)
      {
         FD_SET(proxy.vbi_fd, &rd);
         if (proxy.vbi_fd > max)
            max = proxy.vbi_fd;
      }

      /* wait for an event, but not indefinitly */
      timeout.tv_sec  = 1;
      timeout.tv_usec = 0;

      selSockCnt = select(((max > 0) ? (max + 1) : 0), &rd, &wr, NULL, &timeout);
      if (selSockCnt != -1)
      {  /* forward new blocks to network clients, handle incoming messages, check for timeouts */
         if (selSockCnt > 0)
            dprintf2("main_loop: select: events on %d sockets\n", selSockCnt);

         if ((proxy.vbi_fd != -1) && (FD_ISSET(proxy.vbi_fd, &rd)))
         {
            vbi_proxyd_forward_data();
         }

         vbi_proxyd_handle_sockets(&rd, &wr);
      }
      else
      {
         if (errno != EINTR)
         {  /* select syscall failed */
            dprintf1("main_loop: select with max. fd %d: %s\n", max, strerror(errno));
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
                   "       -dev <path>         : device path\n"
                   "       -nodetach           : daemon remains connected to tty\n"
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
   int arg_val;
   int arg_idx = 1;

   while (arg_idx < argc)
   {
      if (strcasecmp(argv[arg_idx], "-dev") == 0)
      {
         if (arg_idx + 1 < argc)
         {
            p_dev_name = argv[arg_idx + 1];
	    if (access(p_dev_name, R_OK | W_OK) == -1)
               proxy_usage_exit(argv[0], argv[arg_idx +1], "failed to access device");
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
         arg_idx += 1;
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
}

/* ----------------------------------------------------------------------------
** Proxy daemon entry point
*/
int main( int argc, char ** argv )
{
   proxy_parse_argv(argc, argv);

   dprintf1("proxy daemon starting, rev.\n%s\n", rcsid);

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

   vbi_proxyd_set_max_conn(opt_max_clients);
   vbi_proxyd_set_address(FALSE, NULL, NULL);
   vbi_proxy_msg_set_debug_level(opt_debug_level);
   vbi_proxy_msg_set_logging(opt_debug_level > 0, opt_syslog_level, opt_log_level, p_opt_log_name);

   vbi_proxyd_init();

   /* start listening for client connections (at least on the named socket in /tmp) */
   if (vbi_proxyd_listen())
   {
      proxy_set_perm();

      proxy_main_loop();
   }
   vbi_proxyd_destroy();

   exit(0);
   return 0;
}

#endif  /* ENABLE_PROXY */
