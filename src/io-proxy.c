/*
 *  libzvbi - slicer proxy interface
 *
 *  Copyright (C) 2003 Tom Zoerner
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
 *  $Log: io-proxy.c,v $
 *  Revision 1.10  2003/10/16 18:16:11  mschimek
 *  *** empty log message ***
 *
 *  Revision 1.9  2003/06/07 09:42:32  tomzo
 *  Optimized client I/O in proxy-msg.c/.h: keep message header and body in one
 *  struct VBIPROXY_MSG to be able to write it to the pipe in one syscall.
 *
 *  Revision 1.8  2003/06/01 19:34:24  tomzo
 *  Redesigned message I/O handling: from async to synchronous design
 *  - previous design was still derived from nxtvepg client, where I/O is event
 *    driven; in the proxy however messages are synchronous (RPC style), i.e. we
 *    wait for the daemon's reply before the vbi_capture function returns.
 *  -> replaced generic _handle_socket() with specific: _rpc() and _read_message()
 *  - total i/o timeout given to _read() client is now observed
 *  - optimized message I/O in proxy-msg.c: use static buffer to read messages
 *  - prepared for auto-reconnect after broken connections (will be optional)
 *  Implemented server-side channel switching
 *  - new interface function _channel_change(): send CHN_CHANGE_REQ
 *
 *  Revision 1.7  2003/05/24 12:17:13  tomzo
 *  Implemented new capture interfaces:
 *  - add_services(): message exchange with daemon: MSG_TYPE_SERVICE_REQ/_CNF/_REJ
 *  - added get_poll_fd(): return file handle for select (socket file handle)
 *  - changed get_fd() interface: return -1 since VBI device is not acessible
 *  Also:
 *  - renamed MSG_TYPE_DATA_IND into _SLICED_IND in preparation for raw data
 *  - added dummy function for flush() capture interface
 *
 *  Revision 1.6  2003/05/17 13:02:57  tomzo
 *  Fixed definition of dprintf2() macro: print only for (v->trace >= 2)
 *
 *  Revision 1.5  2003/05/10 13:30:24  tomzo
 *  - bugfix proxy_read(): loop around select() until a complete VBI frame was
 *    received or timeout expired; before the func returned 0 when a partial
 *    message was received only, falsely indicating a timeout to the caller
 *  - fixed debug level on dprintfx() messages
 *
 *  Revision 1.4  2003/05/03 12:05:58  tomzo
 *  - added documentation for vbi_capture_proxy_new()
 *  - removed swap32 inline function from proxyd.c and io-proxy.c: use new macro
 *    VBIPROXY_ENDIAN_MISMATCH instead (contains swapped value of endian magic)
 *  - fixed copyright headers, added description to file headers
 *
 */

static const char rcsid[] = "$Id: io-proxy.c,v 1.10 2003/10/16 18:16:11 mschimek Exp $";

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#include "vbi.h"
#include "io.h"

#ifdef ENABLE_PROXY

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <pthread.h>

#include "proxy-msg.h"
#include "bcd.h"

#define dprintf1(fmt, arg...)    do {if (v->trace >= 1) fprintf(stderr, "io-proxy: " fmt, ## arg);} while(0)
#define dprintf2(fmt, arg...)    do {if (v->trace >= 2) fprintf(stderr, "io-proxy: " fmt, ## arg);} while(0)

/* ----------------------------------------------------------------------------
** Declaration of types of internal state variables
*/
typedef enum
{
        CLNT_STATE_NULL,
        CLNT_STATE_ERROR,
        CLNT_STATE_RETRY,
        CLNT_STATE_WAIT_CON_CNF,
        CLNT_STATE_WAIT_IDLE,
        CLNT_STATE_WAIT_SRV_CNF,
        CLNT_STATE_WAIT_CHN_CNF,
        CLNT_STATE_RECEIVE,
} PROXY_CLIENT_STATE;

typedef struct
{
        vbi_bool                blockOnRead;
        vbi_bool                blockOnWrite;
} PROXY_CLIENT_EVHAND;

#define SRV_REPLY_TIMEOUT       60
#define CLNT_NAME_STR           "libzvbi io-proxy"
#define CLNT_RETRY_INTERVAL     20
#define CLNT_MAX_MSG_LOOP_COUNT 50

typedef struct vbi_capture_proxy
{
        vbi_capture             capture;
        vbi_raw_decoder         dec;
        double                  time_per_frame;
        vbi_capture_buffer      sliced_buffer;
        vbi_bool                sliced_ind;

        int                     scanning;
        unsigned int            services;
        int                     strict;
        int                     buffer_count;
        vbi_bool                trace;

        PROXY_CLIENT_STATE      state;
        VBIPROXY_CHN_PROFILE    chn_profile;
        VBIPROXY_MSG_STATE      io;
        VBIPROXY_MSG          * p_client_msg;
        int                     max_client_msg_size;
        vbi_bool                endianSwap;
        unsigned long           rxTotal;
        unsigned long           rxStartTime;
        char                  * p_srv_host;
        char                  * p_srv_port;
        char                  * p_errorstr;

} vbi_capture_proxy;


/* ----------------------------------------------------------------------------
** Open client connection
** - automatically chooses the optimum transport: TCP/IP or pipe for local
** - since the socket is made non-blocking, the result of the connect is not
**   yet available when the function finishes; the caller has to wait for
**   completion with select() and then query the socket error status
*/
static vbi_bool proxy_client_connect_server( vbi_capture_proxy *v )
{
   vbi_bool use_tcp_ip;
   int  sock_fd;
   vbi_bool result = FALSE;

   use_tcp_ip = FALSE;

   /* check if a server address has been configured */
   if ( ((v->p_srv_host != NULL) || (use_tcp_ip == FALSE)) &&
        (v->p_srv_port != NULL))
   {
      sock_fd = vbi_proxy_msg_connect_to_server(use_tcp_ip, v->p_srv_host, v->p_srv_port, &v->p_errorstr);
      if (sock_fd != -1)
      {
         /* initialize IO state */
         memset(&v->io, 0, sizeof(v->io));
         v->io.sock_fd    = sock_fd;
         v->io.lastIoTime = time(NULL);
         v->rxStartTime   = v->io.lastIoTime;
         v->rxTotal       = 0;

         result = TRUE;
      }
   }
   else
   {
      dprintf1("connect_server: hostname or port not configured\n");
      if (use_tcp_ip && (v->p_srv_host == NULL))
         vbi_asprintf(&v->p_errorstr, "%s", _("Server hostname not configured"));
      else if (v->p_srv_port == NULL)
         vbi_asprintf(&v->p_errorstr, "%s", _("Server service name (i.e. port) not configured"));
   }
   return result;
}

/* ----------------------------------------------------------------------------
** Allocate buffer for client/servier message exchange
** - buffer is allocated statically, large enough for all expected messages
*/
static void proxy_client_alloc_msg_buf( vbi_capture_proxy *v )
{
   int  msg_size;

   msg_size = sizeof(VBIPROXY_MSG_BODY);

   if (v->state == CLNT_STATE_RECEIVE)
   {
      msg_size = VBIPROXY_SLICED_IND_SIZE(v->dec.count[0] + v->dec.count[1]);
      if (msg_size < sizeof(VBIPROXY_MSG_BODY))
         msg_size = sizeof(VBIPROXY_MSG_BODY);
   }
   else
      msg_size = sizeof(VBIPROXY_MSG_BODY);

   msg_size += sizeof(VBIPROXY_MSG_HEADER);

   if ((msg_size != v->max_client_msg_size) || (v->p_client_msg == NULL))
   {
      if (v->p_client_msg != NULL)
         free(v->p_client_msg);

      dprintf2("alloc_msg_buf: allocate buffer for max. %d bytes\n", msg_size);
      v->p_client_msg = malloc(msg_size);
      v->max_client_msg_size = msg_size;
   }
}

/* ----------------------------------------------------------------------------
** Checks the size of a message from server to client
*/
static vbi_bool proxy_client_check_msg( vbi_capture_proxy *v, uint len,
                                        VBIPROXY_MSG * pMsg )
{
   VBIPROXY_MSG_HEADER * pHead = &pMsg->head;
   VBIPROXY_MSG_BODY * pBody = &pMsg->body;
   vbi_bool result = FALSE;

   /*if (v->p_client_msg->head.type != MSG_TYPE_SLICED_IND) */
   dprintf2("check_msg: recv msg type %d, len %d\n", pHead->type, pHead->len);

   switch (pHead->type)
   {
      case MSG_TYPE_CONNECT_CNF:
         if ( (len == sizeof(VBIPROXY_MSG_HEADER) + sizeof(pBody->connect_cnf)) &&
              (memcmp(pBody->connect_cnf.magics.protocol_magic, VBIPROXY_MAGIC_STR, VBIPROXY_MAGIC_LEN) == 0) )
         {
            if (pBody->connect_cnf.magics.endian_magic == VBIPROXY_ENDIAN_MAGIC)
            {  /* endian type matches -> no swapping required */
               v->endianSwap = FALSE;
            }
            else if (pBody->connect_cnf.magics.endian_magic == VBIPROXY_ENDIAN_MISMATCH)
            {  /* endian type does not match -> convert "endianess" of all msg elements > 1 byte */
               /* enable byte swapping for all following messages */
               v->endianSwap = TRUE;
            }
            result = TRUE;
         }
         break;

      case MSG_TYPE_CONNECT_REJ:
         result = ( (len == sizeof(VBIPROXY_MSG_HEADER) + sizeof(pBody->connect_rej)) &&
                    (memcmp(pBody->connect_rej.magics.protocol_magic, VBIPROXY_MAGIC_STR, VBIPROXY_MAGIC_LEN) == 0) );
         break;

      case MSG_TYPE_SLICED_IND:
         result = (len == sizeof(VBIPROXY_MSG_HEADER) + sizeof(pBody->sliced_ind) +
                          sizeof(pBody->sliced_ind.sliced[0]) * (pBody->sliced_ind.line_count - 1));
         break;

      case MSG_TYPE_SERVICE_CNF:
         result = (len == sizeof(VBIPROXY_MSG_HEADER) + sizeof(pBody->service_cnf));
         break;

      case MSG_TYPE_SERVICE_REJ:
         result = (len == sizeof(VBIPROXY_MSG_HEADER) + sizeof(pBody->service_rej));
         break;

      case MSG_TYPE_CLOSE_REQ:
         result = (len == sizeof(VBIPROXY_MSG_HEADER));
         break;

      case MSG_TYPE_CHN_CHANGE_CNF:
         result = (len == sizeof(VBIPROXY_MSG_HEADER) + sizeof(pBody->chn_change_cnf));
         break;

      case MSG_TYPE_CHN_CHANGE_REJ:
         result = (len == sizeof(VBIPROXY_MSG_HEADER) + sizeof(pBody->chn_change_rej));
         break;

      case MSG_TYPE_CHN_CHANGE_IND:
         result = (len == sizeof(VBIPROXY_MSG_HEADER) + sizeof(pBody->chn_change_ind));
         break;

      case MSG_TYPE_CONNECT_REQ:
      case MSG_TYPE_SERVICE_REQ:
      case MSG_TYPE_CHN_CHANGE_REQ:
         dprintf1("check_msg: recv server msg type %d\n", pHead->type);
         result = FALSE;
         break;
      default:
         dprintf1("check_msg: unknown msg type %d\n", pHead->type);
         result = FALSE;
         break;
   }

   if (result == FALSE)
      dprintf1("check_msg: illegal msg len %d for type %d\n", len, pHead->type);

   return result;
}

/* ----------------------------------------------------------------------------
** Handle message from server
*/
static vbi_bool proxy_client_take_message( vbi_capture_proxy *v )
{
   VBIPROXY_MSG_BODY * pMsg = &v->p_client_msg->body;
   vbi_bool result = FALSE;

   switch (v->p_client_msg->head.type)
   {
      case MSG_TYPE_CONNECT_CNF:
         if (v->state == CLNT_STATE_WAIT_CON_CNF)
         {
            /* first server message received: contains version info */
            /* note: nxtvepg and endian magics are already checked */
            if (pMsg->connect_cnf.magics.protocol_compat_version != VBIPROXY_COMPAT_VERSION)
            {
               dprintf1("take_message: CONNECT_CNF: reply version %x, protocol %x\n", pMsg->connect_cnf.magics.protocol_version, pMsg->connect_cnf.magics.protocol_compat_version);

               vbi_asprintf(&v->p_errorstr, "%s: %d.%d.%d", _("Incompatible server version"),
                                         ((pMsg->connect_cnf.magics.protocol_compat_version >> 16) & 0xff),
                                         ((pMsg->connect_cnf.magics.protocol_compat_version >>  8) & 0xff),
                                         ((pMsg->connect_cnf.magics.protocol_compat_version      ) & 0xff));
            }
            else if (v->endianSwap)
            {  /* endian swapping currently unsupported */
               vbi_asprintf(&v->p_errorstr, "%s", _("Incompatible server CPU architecture (endianess mismatch)"));
            }
            else
            {  /* version ok -> request block forwarding */
               dprintf1("Successfully connected to proxy (version %x, protocol %x)\n", pMsg->connect_cnf.magics.protocol_version, pMsg->connect_cnf.magics.protocol_compat_version);
               memcpy(&v->dec, &pMsg->connect_cnf.dec, sizeof(v->dec));

               v->state = CLNT_STATE_RECEIVE;
               result = TRUE;
            }
         }
         break;

      case MSG_TYPE_CONNECT_REJ:
         if (v->state == CLNT_STATE_WAIT_CON_CNF)
         {
            dprintf2("take_message: CONNECT_REJ: reply version %x, protocol %x\n", pMsg->connect_rej.magics.protocol_version, pMsg->connect_rej.magics.protocol_compat_version);
            if (v->p_errorstr != NULL)
            {
               free(v->p_errorstr);
               v->p_errorstr = NULL;
            }
            if (pMsg->connect_rej.errorstr[0] != 0)
               v->p_errorstr = strdup(pMsg->connect_rej.errorstr);

            v->state = CLNT_STATE_ERROR;
            result = TRUE;
         }
         break;

      case MSG_TYPE_SLICED_IND:
         if (v->state == CLNT_STATE_RECEIVE)
         {
            if (pMsg->sliced_ind.line_count > v->dec.count[0] + v->dec.count[1])
            {  /* more lines than req. for service -> would overflow the allocated slicer buffer
               ** -> discard extra lines (should never happen; proxy checks for line counts) */
               dprintf1("take_message: SLICED_IND: too many lines: %d > %d\n", pMsg->sliced_ind.line_count, v->dec.count[0] + v->dec.count[1]);
               pMsg->sliced_ind.line_count = v->dec.count[0] + v->dec.count[1];
            }
            /*assert(v->sliced_ind == FALSE);*/
            v->sliced_ind = TRUE;
            result = TRUE;
         }
         else if ( (v->state == CLNT_STATE_WAIT_IDLE) ||
                   (v->state == CLNT_STATE_WAIT_SRV_CNF) ||
                   (v->state == CLNT_STATE_WAIT_CHN_CNF) )
         {
            /* discard incoming data during service changes */
            result = TRUE;
         }
         break;

      case MSG_TYPE_SERVICE_CNF:
         if (v->state == CLNT_STATE_WAIT_SRV_CNF)
         {
            memcpy(&v->dec, &pMsg->service_cnf.dec, sizeof(v->dec));
            dprintf1("service cnf: granted service %d\n", v->dec.services);

            v->state = CLNT_STATE_RECEIVE;
            result = TRUE;
         }
         break;

      case MSG_TYPE_SERVICE_REJ:
         if (v->state == CLNT_STATE_WAIT_SRV_CNF)
         {
            if (v->p_errorstr != NULL)
            {
               free(v->p_errorstr);
               v->p_errorstr = NULL;
            }
            if (pMsg->service_rej.errorstr[0] != 0)
               v->p_errorstr = strdup(pMsg->service_rej.errorstr);

            memset(&v->dec, 0, sizeof(v->dec));

            v->state = CLNT_STATE_RECEIVE;
            result = TRUE;
         }
         break;

      case MSG_TYPE_CHN_CHANGE_CNF:
      case MSG_TYPE_CHN_CHANGE_REJ:
         if (v->state == CLNT_STATE_WAIT_CHN_CNF)
         {
            /* message content is evaluated by caller */
            v->state = CLNT_STATE_RECEIVE;
            result = TRUE;
         }
         break;

      case MSG_TYPE_CHN_CHANGE_IND:
         /* XXX TODO */
         result = TRUE;
         break;

      case MSG_TYPE_CLOSE_REQ:
         break;

      case MSG_TYPE_CONNECT_REQ:
      default:
         break;
   }

   if ((result == FALSE) && (v->p_errorstr == NULL))
   {
      dprintf1("take_message: message type %d (len %d) not expected in state %d\n", v->p_client_msg->head.type, v->p_client_msg->head.len, v->state);
      vbi_asprintf(&v->p_errorstr, "%s", _("Proxy protocol error (unecpected message)"));
   }

   return result;
}

/* ----------------------------------------------------------------------------
** Close client connection
*/
static void proxy_client_close( vbi_capture_proxy *v )
{
   vbi_proxy_msg_close_io(&v->io);

   memset(&v->io, 0, sizeof(v->io));
   v->io.sock_fd    = -1;
   v->io.lastIoTime = time(NULL);

   if (v->state != CLNT_STATE_NULL)
   {
      /* enter error state */
      /* will be changed to retry or off once the error is reported to the upper layer */
      v->state = CLNT_STATE_ERROR;
   }
}

/* ----------------------------------------------------------------------------
** Substract time spent waiting in select from a given max. timeout struct
** - note that we don't use the Linux select(2) feature to return the
**   time not slept in the timeout struct, because that's not portable
** - instead gettimeofday(2) must be called before and after the select(2)
**   and the delta calculated from that
*/
static void proxy_update_timeout_delta( struct timeval * tv_start,
                                        struct timeval * tv_stop,
                                        struct timeval * timeout )
{
   struct timeval delta;

   /* first calculate difference between start and stop time */
   delta.tv_sec = tv_stop->tv_sec - tv_start->tv_sec;
   if (tv_stop->tv_usec < tv_start->tv_usec)
   {
      delta.tv_usec = 1000000 + tv_stop->tv_usec - tv_start->tv_usec;
      delta.tv_sec += 1;
   }
   else
      delta.tv_usec = tv_stop->tv_usec - tv_start->tv_usec;

   assert((delta.tv_sec >= 0) && (delta.tv_usec >= 0));

   /* substract delta from the given max. timeout */
   timeout->tv_sec -= delta.tv_sec;
   if (timeout->tv_usec < delta.tv_usec)
   {
      timeout->tv_usec = 1000000 + timeout->tv_usec - delta.tv_usec;
      timeout->tv_sec -= 1;
   }
   else
      timeout->tv_usec -= delta.tv_usec;

   /* check if timeout was underrun -> set rest to zero */
   if ( (timeout->tv_sec < 0) || (timeout->tv_usec < 0) )
   {
      timeout->tv_sec  = 0;
      timeout->tv_usec = 0;
   }
}

/* ----------------------------------------------------------------------------
** Wait for I/O event on socket with the given timeout
*/
static int proxy_client_wait_select( vbi_capture_proxy *v, struct timeval * timeout )
{
   struct timeval tv_start;
   struct timeval tv_stop;
   struct timeval tv;
   fd_set fd_rd;
   fd_set fd_wr;
   int    ret;

   if (v->io.sock_fd == -1)
      return -1;

   do
   {
      pthread_testcancel();

      FD_ZERO(&fd_rd);
      FD_ZERO(&fd_wr);

      if (v->io.writeLen > 0)
         FD_SET(v->io.sock_fd, &fd_wr);
      else
         FD_SET(v->io.sock_fd, &fd_rd);

      tv = *timeout; /* Linux kernel overwrites this */

      gettimeofday(&tv_start, NULL);
      ret = select(v->io.sock_fd + 1, &fd_rd, &fd_wr, NULL, &tv);
      gettimeofday(&tv_stop, NULL);

      proxy_update_timeout_delta(&tv_start, &tv_stop, timeout);

   } while ((ret < 0) && (errno == EINTR));

   if (ret > 0)
      dprintf2("wait_select: waited for %c -> sock r/w %d/%d\n", ((v->io.writeLen > 0) ? 'w':'r'), FD_ISSET(v->io.sock_fd, &fd_rd), FD_ISSET(v->io.sock_fd, &fd_wr));
   else if (ret == 0)
      dprintf1("wait_select: timeout\n");
   else
      dprintf1("wait_select: error %d (%s)\n", errno, strerror(errno));

   return ret;
}

/* ----------------------------------------------------------------------------
** Call remote procedure, i.e. write message then wait for reply
** - this is a synchronous message exchange with the daemon, i.e. the function
**   does not return until a reply is available or a timeout occured (in which
**   case the connection is dropped.)
*/
static vbi_bool proxy_client_rpc( vbi_capture_proxy *v, long tv_sec, long tv_usec )
{
   struct timeval tv;
   vbi_bool io_blocked;

   assert ((v->state != CLNT_STATE_RETRY) && (v->state != CLNT_STATE_ERROR));
   assert (v->io.sock_fd != -1);

   tv.tv_sec  = tv_sec;
   tv.tv_usec = tv_usec;

   /* wait for write to finish */
   do
   {
      if (proxy_client_wait_select(v, &tv) <= 0)
         goto failure;

      if (vbi_proxy_msg_handle_io(&v->io, &io_blocked, FALSE, NULL, 0) == FALSE)
         goto failure;

   } while (v->io.writeLen > 0);

   /* wait for reply message */
   v->io.waitRead = TRUE;
   v->io.readLen  = 0;
   v->io.readOff  = 0;
   do
   {
      if (proxy_client_wait_select(v, &tv) <= 0)
         goto failure;

      if (vbi_proxy_msg_handle_io(&v->io, &io_blocked, TRUE, v->p_client_msg, v->max_client_msg_size) == FALSE)
         goto failure;

      assert((v->io.waitRead == FALSE) || (v->io.readOff > 0));

   } while (v->io.waitRead || (v->io.readOff < v->io.readLen));

   /* perform security checks on received message */
   if (proxy_client_check_msg(v, v->io.readLen, v->p_client_msg) == FALSE)
      goto failure;

   v->rxTotal += v->p_client_msg->head.len;
   v->io.readLen = 0;

   return TRUE;

failure:
   vbi_asprintf(&v->p_errorstr, "%s", _("Lost connection to proxy (I/O error)"));
   return FALSE;
}

/* ----------------------------------------------------------------------------
** Read a message from the socket
*/
static int proxy_client_read_message( vbi_capture_proxy *v, struct timeval * p_timeout )
{
   vbi_bool io_blocked;
   int  ret;

   assert (v->io.writeLen == 0);

   proxy_client_alloc_msg_buf(v);

   /* new incoming data -> start reading */
   if (vbi_proxy_msg_is_idle(&v->io))
   {
      v->io.waitRead = TRUE;
      v->io.readLen  = 0;
      v->io.readOff  = 0;
   }

   do
   {
      ret = proxy_client_wait_select(v, p_timeout);
      if (ret < 0)
         goto failure;
      if (ret == 0)
         return 0;

      if (vbi_proxy_msg_handle_io(&v->io, &io_blocked, TRUE, v->p_client_msg, v->max_client_msg_size) == FALSE)
         goto failure;

      assert((v->io.waitRead == FALSE) || (v->io.readOff > 0));

   } while (v->io.waitRead || (v->io.readOff < v->io.readLen));

   /* perform security checks on received message */
   if (proxy_client_check_msg(v, v->io.readLen, v->p_client_msg) == FALSE)
      goto failure;

   v->rxTotal += v->p_client_msg->head.len;
   v->io.readLen = 0;

   /* process the message - frees the buffer if neccessary */
   if (proxy_client_take_message(v) == FALSE)
      goto failure;

   return 1;

failure:
   vbi_asprintf(&v->p_errorstr, "%s", _("Lost connection (I/O error)"));
   proxy_client_close(v);
   return -1;
}

/* ----------------------------------------------------------------------------
** Wait until ongoing read is finished
** - incoming data is discarded
*/
static vbi_bool proxy_client_wait_idle( vbi_capture_proxy *v )
{
   PROXY_CLIENT_STATE old_state;
   struct timeval tv;
   vbi_bool io_blocked;

   assert (v->io.writeLen == 0);

   if (v->io.readLen > 0)
   {
      /* set intermediate state so that incoming data is discarded in the handler */
      tv.tv_sec  = 2;
      tv.tv_usec = 0;

      while (v->io.readOff < v->io.readLen)
      {
         if (proxy_client_wait_select(v, &tv) <= 0)
            goto failure;

         if (vbi_proxy_msg_handle_io(&v->io, &io_blocked, TRUE, v->p_client_msg, v->max_client_msg_size) == FALSE)
            goto failure;
      }

      /* perform security checks on received message */
      if (proxy_client_check_msg(v, v->io.readLen, v->p_client_msg) == FALSE)
         goto failure;

      old_state = v->state;
      v->state = CLNT_STATE_WAIT_IDLE;

      if (proxy_client_take_message(v) == FALSE)
         goto failure;

      v->state = old_state;
   }

   return TRUE;

failure:
   return FALSE;
}

/* ----------------------------------------------------------------------------
** Start acquisition, i.e. initiate network connection
*/
static vbi_bool proxy_client_start_acq( vbi_capture_proxy *v )
{
   struct timeval tv;

   assert(v->state == CLNT_STATE_NULL);

   if (proxy_client_connect_server(v) == FALSE)
      goto failure;

   /* fake write request: make select to wait for socket to become writable */
   v->io.writeLen = 1;
   tv.tv_sec  = 4;
   tv.tv_usec = 0;

   /* wait for socket to reach connected state */
   if (proxy_client_wait_select(v, &tv) <= 0)
      goto failure;

   v->io.writeLen = 0;

   if (vbi_proxy_msg_finish_connect(v->io.sock_fd, &v->p_errorstr) == FALSE)
      goto failure;

   proxy_client_alloc_msg_buf(v);

   /* write service request parameters */
   vbi_proxy_msg_fill_magics(&v->p_client_msg->body.connect_req.magics);

   strncpy(v->p_client_msg->body.connect_req.client_name, CLNT_NAME_STR, VBIPROXY_CLIENT_NAME_MAX_LENGTH);
   v->p_client_msg->body.connect_req.client_name[VBIPROXY_CLIENT_NAME_MAX_LENGTH - 1] = 0;

   v->p_client_msg->body.connect_req.scanning     = v->scanning;
   v->p_client_msg->body.connect_req.services     = v->services;
   v->p_client_msg->body.connect_req.strict       = v->strict;
   v->p_client_msg->body.connect_req.buffer_count = v->buffer_count;

   /* send the connect request message to the proxy server */
   vbi_proxy_msg_write(&v->io, MSG_TYPE_CONNECT_REQ, sizeof(v->p_client_msg->body.connect_req),
                       v->p_client_msg, FALSE);

   if (proxy_client_rpc(v, 5, 0) == FALSE)
      goto failure;

   v->state = CLNT_STATE_WAIT_CON_CNF;

   /* process the message - frees the buffer if neccessary */
   if (proxy_client_take_message(v) == FALSE)
      goto failure;

   return TRUE;

failure:
   /* failed to establish a connection to the server */
   v->state = CLNT_STATE_NULL;
   proxy_client_close(v);
   return FALSE;
}

/* ----------------------------------------------------------------------------
** Stop acquisition, i.e. close connection
*/
static void proxy_client_stop_acq( vbi_capture_proxy *v )
{
   if (v->state != CLNT_STATE_NULL)
   {
      /* note: set the new state first to prevent callback from close function */
      v->state = CLNT_STATE_NULL;

      proxy_client_close(v);
   }
   else
      dprintf1("stop_acq: acq not enabled\n");
}

/* ----------------------------------------------------------------------------
** Attempt to reconnect to the daemon
*/
static vbi_bool proxy_client_reconnect( vbi_capture_proxy *v, struct timeval *p_timeout )
{
   time_t   now = time(NULL);
   vbi_bool result = FALSE;

   if (v->state == CLNT_STATE_ERROR)
   {
      if (now + p_timeout->tv_sec > v->io.lastIoTime + CLNT_RETRY_INTERVAL)
      {
         dprintf1("initiate connect retry\n");
         v->io.lastIoTime = now;

         if ( proxy_client_connect_server(v) )
         {
            v->state = CLNT_STATE_NULL;
            if (proxy_client_start_acq(v))
            {
               result = TRUE;
            }
            else
               v->state = CLNT_STATE_ERROR;
         }
      }
      else
      {
         select(0, NULL, NULL, NULL, p_timeout);
         errno = EPIPE;
      }
   }

   return result;
}

/* ----------------------------------------------------------------------------
** Read one frame's worth of sliced VBI data
*/
static int
proxy_read( vbi_capture *vc, vbi_capture_buffer **raw,
            vbi_capture_buffer **sliced, struct timeval *p_timeout )
{
        vbi_capture_proxy *v = PARENT(vc, vbi_capture_proxy, capture);
        int  ret;

        if ( (v->state == CLNT_STATE_ERROR) &&
             (proxy_client_reconnect(v, p_timeout) == FALSE) )
                return -1; /* XXX return 0 if client requested auto-reconnect */

        if (v->state != CLNT_STATE_RECEIVE)
                return -1;

        v->sliced_ind = FALSE;

        do {
                ret = proxy_client_read_message(v, p_timeout);
                if (ret <= 0)
                        return ret;

        } while (v->sliced_ind == FALSE);

        if (raw != NULL) {
                /* XXX TODO raw buffer forward not implemented */
        }

        if (v->sliced_ind != FALSE) {
                int lines = v->p_client_msg->body.sliced_ind.line_count;

                if (*sliced) {
                        /* XXX optimization possible: read sliced msg into buffer to avoid memcpy */
                        memcpy( (vbi_sliced *)(*sliced)->data,
                                v->p_client_msg->body.sliced_ind.sliced,
                                lines * sizeof(vbi_sliced) );

                        (*sliced)->timestamp = v->p_client_msg->body.sliced_ind.timestamp;
                        (*sliced)->size      = lines * sizeof(vbi_sliced);
                } else {
                        *sliced = &v->sliced_buffer;
                        (*sliced)->data      = v->p_client_msg->body.sliced_ind.sliced;
                        (*sliced)->timestamp = v->p_client_msg->body.sliced_ind.timestamp;
                        (*sliced)->size      = lines * sizeof(vbi_sliced);
                }
                return 1;
        }
        else
                return 0;
}

static vbi_raw_decoder *
proxy_parameters(vbi_capture *vc)
{
        vbi_capture_proxy *v = PARENT(vc, vbi_capture_proxy, capture);

        return &v->dec;
}

static void
proxy_delete(vbi_capture *vc)
{
        vbi_capture_proxy *v = PARENT(vc, vbi_capture_proxy, capture);

        /* close the connection (during normal shutdown it should already be closed) */
        if (v->state != CLNT_STATE_NULL)
                proxy_client_stop_acq(v);

        if (v->p_srv_host != NULL)
                free(v->p_srv_host);

        if (v->p_srv_port != NULL)
                free(v->p_srv_port);

        if (v->p_client_msg != NULL)
                free(v->p_client_msg);

        if (v->p_errorstr != NULL)
           free(v->p_errorstr);

        free(v);
}

static int
proxy_channel_change(vbi_capture *vc, int chn_flags, int chn_prio,
                     vbi_channel_desc * p_chn_desc,
                     vbi_bool * p_has_tuner, int * p_scanning,
                     char ** errorstr)
{
        vbi_capture_proxy *v = PARENT(vc, vbi_capture_proxy, capture);
        int result;

        if (v->state == CLNT_STATE_ERROR)
                return -1;

        assert(v->state == CLNT_STATE_RECEIVE);

        /* check for prio change (XXX depreciated - should use separate config func) */
        if (chn_prio != v->chn_profile.chn_prio) {
                memset(&v->chn_profile.chn_prio, 0, sizeof(v->chn_profile.chn_prio));
                v->chn_profile.chn_prio = chn_prio;
                v->chn_profile.is_valid = TRUE;
        }

        proxy_client_alloc_msg_buf(v);

        /* wait for ongoing read to complete */
        if (proxy_client_wait_idle(v) == FALSE)
                goto failure;

        v->state = CLNT_STATE_WAIT_CHN_CNF;

        dprintf1("channel_change: req chn %d, freq %d\n", p_chn_desc->u.analog.channel, p_chn_desc->u.analog.freq);

        /* send service request to proxy daemon */
        v->p_client_msg->body.chn_change_req.chn_flags   = chn_flags;
        v->p_client_msg->body.chn_change_req.chn_desc    = *p_chn_desc;
        v->p_client_msg->body.chn_change_req.chn_profile = v->chn_profile;
        v->p_client_msg->body.chn_change_req.serial      = v->rxTotal;

        vbi_proxy_msg_write(&v->io, MSG_TYPE_CHN_CHANGE_REQ, sizeof(v->p_client_msg->body.chn_change_req),
                            v->p_client_msg, FALSE);

        if (proxy_client_rpc(v, 5, 0) == FALSE)
                goto failure;

        /* process the message - frees the buffer if neccessary */
        if (proxy_client_take_message(v) == FALSE)
                goto failure;

        /* XXX TODO check serial? (not mandatory b/c we wait for reply before next req.) */

        *errorstr = NULL;
        if (v->p_client_msg->head.type == MSG_TYPE_CHN_CHANGE_CNF) {
                *p_scanning  = v->p_client_msg->body.chn_change_cnf.scanning;
                *p_has_tuner = v->p_client_msg->body.chn_change_cnf.has_tuner;
                result = 0;
        }
        else {
                if (v->p_client_msg->body.chn_change_rej.errorstr[0] != 0)
                        *errorstr = strdup(v->p_client_msg->body.chn_change_rej.errorstr);
                errno = v->p_client_msg->body.chn_change_rej.dev_errno;
                result = -1;
        }

        return result;

failure:
        proxy_client_close(v);
        return -1;
}

static int
proxy_get_read_fd(vbi_capture *vc)
{
        /* direct access to device is not supported */
        return -1;
}

static int
proxy_get_poll_fd(vbi_capture *vc)
{
        vbi_capture_proxy *v = PARENT(vc, vbi_capture_proxy, capture);

        /* note: returned filehandle must only be used for poll(2) and select(2) */
        return v->io.sock_fd;
}

static unsigned int
proxy_add_services(vbi_capture *vc, vbi_bool reset, vbi_bool commit,
                  unsigned int services, int strict,
                  char ** errorstr)
{
        vbi_capture_proxy *v = PARENT(vc, vbi_capture_proxy, capture);

        if ((services == 0) || (v->state == CLNT_STATE_ERROR))
                return 0;

        assert(v->state == CLNT_STATE_RECEIVE);

        proxy_client_alloc_msg_buf(v);

        /* wait for ongoing read to complete */
        if (proxy_client_wait_idle(v) == FALSE)
                goto failure;

        v->state = CLNT_STATE_WAIT_SRV_CNF;

        dprintf1("add_services: send service req: srv %d, strict %d\n", services, strict);

        /* send service request to proxy daemon */
        v->p_client_msg->body.service_req.commit       = reset;
        v->p_client_msg->body.service_req.commit       = commit;
        v->p_client_msg->body.service_req.services     = services;
        v->p_client_msg->body.service_req.strict       = strict;
        vbi_proxy_msg_write(&v->io, MSG_TYPE_SERVICE_REQ, sizeof(v->p_client_msg->body.service_req),
                            v->p_client_msg, FALSE);

        if (proxy_client_rpc(v, 5, 0) == FALSE)
                goto failure;

        /* process the message - frees the buffer if neccessary */
        if (proxy_client_take_message(v) == FALSE)
                goto failure;

        return v->dec.services & services;

failure:
        proxy_client_close(v);
        return 0;
}

/* document below */
vbi_capture *
vbi_capture_proxy_new(const char *dev_name, int buffers, int scanning,
                      unsigned int *services, int strict,
                      char **pp_errorstr, vbi_bool trace)
{
        vbi_capture_proxy *v;

        assert(services && *services != 0);

        if (scanning != 525 && scanning != 625)
                scanning = 0;

        if (buffers < 1)
                buffers = 1;

        if (strict < -1)
                strict = -1;
        else if (strict > 2)
                strict = 2;

        if (trace) {
                fprintf(stderr, "Try to connect vbi proxy, libzvbi interface rev.\n%s\n", rcsid);
                vbi_proxy_msg_set_debug_level(trace);
        }

        v = (vbi_capture_proxy *) calloc(1, sizeof(*v));
        if (v == NULL) {
                vbi_asprintf(pp_errorstr, _("Virtual memory exhausted."));
                errno = ENOMEM;
                return NULL;
        }

        v->capture.parameters = proxy_parameters;
        v->capture._delete = proxy_delete;
        v->capture.get_fd = proxy_get_read_fd;
        v->capture.get_poll_fd = proxy_get_poll_fd;
        v->capture.read = proxy_read;
        v->capture.add_services = proxy_add_services;
        v->capture.channel_change = proxy_channel_change;

        v->scanning     = scanning,
        v->services     = *services;
        v->strict       = strict;
        v->buffer_count = buffers;
        v->trace        = trace;

        v->p_srv_port   = vbi_proxy_msg_get_socket_name(dev_name);
        v->p_srv_host   = NULL;

        proxy_client_start_acq(v);

        if (pp_errorstr != NULL) {
                *pp_errorstr = v->p_errorstr;
                v->p_errorstr = NULL;
        }

        if ( (v->state == CLNT_STATE_RECEIVE) &&
             (v->services != 0) )
        {
                *services = v->services;
                return &v->capture;
        }
        else
        {
                proxy_delete(&v->capture);
                return NULL;
        }
}

#else

/**
 * @param dev_name Name of the device to open, usually one of
 *   @c /dev/vbi or @c /dev/vbi0 and up.  The proxy daemon must be
 *   started with the same device name and there must be a proxy
 *   for each different device.
 * @param buffers Number of device buffers for raw vbi data, when
 *   the driver supports streaming. Otherwise one bounce buffer
 *   is allocated for vbi_capture_pull().
 * @param scanning This indicates the current norm: 625 for PAL and
 *   525 for NTSC; set to 0 if you don't know (you should not attempt
 *   to query the device for the norm, as this parameter is only required
 *   for v4l1 drivers which don't support video standard query ioctls)
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
 * Open a new connection to a VBI proxy to open a VBI device for the
 * given services.  On side of the proxy one of the regular v4l_new()
 * functions is invoked and if it succeeds, data slicing is started
 * and all captured data forwarded transparently.  Whenever possible
 * the proxy should be used instead of opening the device directly, since
 * it allows multiple VBI clients to operate concurrently.
 *
 * @since 0.2.5
 * 
 * @return
 * Initialized vbi_capture context, @c NULL on failure.
 */
vbi_capture *
vbi_capture_proxy_new(const char *dev_name, int buffers, int scanning,
                      unsigned int *services, int strict,
                      char **pp_errorstr, vbi_bool trace)
{
        pthread_once (&vbi_init_once, vbi_init);
        vbi_asprintf(pp_errorstr, _("PROXY interface not compiled."));
        return NULL;
}

#endif /* !ENABLE_PROXY */
