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
 */

static const char rcsid[] = "$Id: io-proxy.c,v 1.2 2003/04/29 17:12:46 mschimek Exp $";

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
#include <utils.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <pthread.h>

#include "proxy-msg.h"
#include "bcd.h"


#define dprintf1(fmt, arg...)    if (v->trace) printf("WARN  io-proxy: " fmt, ## arg)
#define dprintf2(fmt, arg...)    if (v->trace) printf("TRACE io-proxy: " fmt, ## arg)

typedef enum
{
        CLNT_STATE_NULL,
        CLNT_STATE_ERROR,
        CLNT_STATE_RETRY,
        CLNT_STATE_WAIT_CONNECT,
        CLNT_STATE_WAIT_CON_CNF,
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
        VBIPROXY_MSG_BODY     * p_data_ind;

        int                     scanning;
        unsigned int            services;
        int                     strict;
        int                     buffer_count;
        vbi_bool                trace;

        PROXY_CLIENT_STATE      state;
        VBIPROXY_MSG_STATE      io;
        VBIPROXY_MSG_BODY       client_msg;
        PROXY_CLIENT_EVHAND     ev_hand;
        vbi_bool                endianSwap;
        unsigned long           rxTotal;
        unsigned long           rxStartTime;
        char                  * p_srv_host;
        char                  * p_srv_port;
        char                  * p_errorstr;

} vbi_capture_proxy;


/* ----------------------------------------------------------------------------
** Sent the connect request message to the proxy server
*/
static void proxy_client_send_connect_req( vbi_capture_proxy *v )
{
   vbi_proxy_msg_fill_magics(&v->client_msg.connect_req.magics);

   strncpy(v->client_msg.connect_req.client_name, CLNT_NAME_STR, VBIPROXY_CLIENT_NAME_MAX_LENGTH);
   v->client_msg.connect_req.client_name[VBIPROXY_CLIENT_NAME_MAX_LENGTH - 1] = 0;

   /* write service request parameters */
   v->client_msg.connect_req.scanning     = v->scanning;
   v->client_msg.connect_req.services     = v->services;
   v->client_msg.connect_req.strict       = v->strict;
   v->client_msg.connect_req.buffer_count = v->buffer_count;

   vbi_proxy_msg_write(&v->io, MSG_TYPE_CONNECT_REQ, sizeof(v->client_msg.connect_req),
                       &v->client_msg.connect_req, FALSE);
}

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

         result = TRUE;;
      }
   }
   else
   {
      dprintf2("connect_server: Hostname or port not configured\n");
      if (use_tcp_ip && (v->p_srv_host == NULL))
         vbi_asprintf(&v->p_errorstr, "%s", _("Server hostname not configured"));
      else if (v->p_srv_port == NULL)
         vbi_asprintf(&v->p_errorstr, "%s", _("Server service name (i.e. port) not configured"));
   }
   return result;
}

/* ----------------------------------------------------------------------------
** Checks the size of a message from server to client
*/
static vbi_bool proxy_client_check_msg( vbi_capture_proxy *v, uint len, VBIPROXY_MSG_HEADER * pHead, VBIPROXY_MSG_BODY * pBody )
{
   vbi_bool result = FALSE;

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
            else if (pBody->connect_cnf.magics.endian_magic == swap32(VBIPROXY_ENDIAN_MAGIC))
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

      case MSG_TYPE_DATA_IND:
         result = (len == sizeof(VBIPROXY_MSG_HEADER) + sizeof(pBody->data_ind) +
                          sizeof(pBody->data_ind.sliced[0]) * (pBody->data_ind.line_count - 1));
         break;

      case MSG_TYPE_CLOSE_REQ:
         result = (len == sizeof(VBIPROXY_MSG_HEADER));
         break;

      case MSG_TYPE_CONNECT_REQ:
         dprintf2("check_msg: recv server msg type %d\n", pHead->type);
         result = FALSE;
         break;
      default:
         dprintf2("check_msg: unknown msg type %d\n", pHead->type);
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
static vbi_bool proxy_client_take_message( vbi_capture_proxy *v, VBIPROXY_MSG_BODY * pMsg )
{
   vbi_bool result = FALSE;

   /*if (v->io.readHeader.type != MSG_TYPE_DATA_IND) //XXX */
   dprintf1("take_message: recv msg type %d, len %d\n", v->io.readHeader.type, v->io.readHeader.len);

   switch (v->io.readHeader.type)
   {
      case MSG_TYPE_CONNECT_CNF:
         if (v->state == CLNT_STATE_WAIT_CON_CNF)
         {
            dprintf1("take_message: CONNECT_CNF: reply version %x, protocol %x\n", pMsg->connect_cnf.magics.protocol_version, pMsg->connect_cnf.magics.protocol_compat_version);
            /* first server message received: contains version info */
            /* note: nxtvepg and endian magics are already checked */
            if (pMsg->connect_cnf.magics.protocol_compat_version != VBIPROXY_COMPAT_VERSION)
            {
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
               memcpy(&v->dec, &pMsg->connect_cnf.dec, sizeof(v->dec));

               v->state = CLNT_STATE_RECEIVE;
               result = TRUE;
            }
         }
         break;

      case MSG_TYPE_CONNECT_REJ:
         if (v->state == CLNT_STATE_WAIT_CON_CNF)
         {
            dprintf1("take_message: CONNECT_REJ: reply version %x, protocol %x\n", pMsg->connect_rej.magics.protocol_version, pMsg->connect_rej.magics.protocol_compat_version);
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

      case MSG_TYPE_DATA_IND:
         if (v->state == CLNT_STATE_RECEIVE)
         {
            if (v->p_data_ind != NULL)
               free(v->p_data_ind);
            if (pMsg->data_ind.line_count > v->dec.count[0] + v->dec.count[1])
            {  /* more lines than req. for service -> would overflow the allocated slicer buffer
               ** -> discard extra lines (should never happen; proxy checks for line counts) */
               dprintf1("take_message: DATA_IND: too many lines: %d > %d\n", pMsg->data_ind.line_count, v->dec.count[0] + v->dec.count[1]);
               pMsg->data_ind.line_count = v->dec.count[0] + v->dec.count[1];
            }
            v->p_data_ind = pMsg;
            pMsg = NULL;
            result = TRUE;
         }
         break;

      case MSG_TYPE_CLOSE_REQ:
         break;

      case MSG_TYPE_CONNECT_REQ:
      default:
         break;
   }

   if ((result == FALSE) && (v->p_errorstr == NULL))
   {
      dprintf2("take_message: message type %d (len %d) not expected in state %d\n", v->io.readHeader.type, v->io.readHeader.len, v->state);
      vbi_asprintf(&v->p_errorstr, "%s", _("Proxy protocol error (unecpected message)"));
   }
   if (pMsg != NULL)
      free(pMsg);

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
** Main client handler
** - called by file handler when network socket is readable or writable
** - accept new incoming messages; advance ongoing I/O; when complete, process
**   messages according to current protocol state
** - don't stop after one message complete; loop until socket blocks,
**   or a maximum message count is reached (allow other events to be processed)
** - most protocol states are protected by a timeout, which is implemented
**   on a polling basis (i.e. the func is called periodically) in a separate
**   check function.
*/
static void proxy_client_handle_socket( vbi_capture_proxy *v )
{
   uint loopCount;
   vbi_bool readable;
   vbi_bool read2ndMsg;
   vbi_bool ioBlocked;

   readable = v->ev_hand.blockOnRead;
   read2ndMsg = TRUE;
   loopCount  = 0;

   if (v->state == CLNT_STATE_WAIT_CONNECT)
   {
      if (vbi_proxy_msg_finish_connect(v->io.sock_fd, &v->p_errorstr))
      {
         proxy_client_send_connect_req(v);
         v->state = CLNT_STATE_WAIT_CON_CNF;
      }
      else
      {  /* failed to establish a connection to the server */
         proxy_client_close(v);
      }
   }

   if ( (v->state != CLNT_STATE_RETRY) &&
        (v->state != CLNT_STATE_ERROR) &&
        (v->state != CLNT_STATE_WAIT_CONNECT) )
   {
      do
      {
         if (vbi_proxy_msg_is_idle(&v->io))
         {  /* no ongoing I/O -> check if anything needs to be sent and for incoming data */
            #if 0
            if (v->xxxFlag)
            {  /* send a message to the server */
               v->io.lastIoTime  = time(NULL);
               v->provUpdate     = FALSE;
               v->statsReqUpdate = FALSE;

               memcpy(&v->client_msg.fwd_req.provCnis, v->provCnis, sizeof(v->client_msg.fwd_req.provCnis));
               for (dbIdx=0; dbIdx < v->cniCount; dbIdx++)
                  v->client_msg.fwd_req.dumpStartTimes[dbIdx] = EpgContextCtl_GetAiUpdateTime(v->provCnis[dbIdx]);
               v->client_msg.fwd_req.cniCount     = v->cniCount;
               v->client_msg.fwd_req.statsReqBits = v->statsReqBits;
               vbi_proxy_msg_write(&v->io, MSG_TYPE_FORWARD_REQ, sizeof(v->client_msg.fwd_req), &v->client_msg.fwd_req, FALSE);
            }
            else
            #endif
            if (readable || read2ndMsg)
            {  /* new incoming data -> start reading */
               v->io.waitRead = TRUE;
               v->io.readLen  = 0;
               v->io.readOff  = 0;
               if (readable == FALSE)
                  read2ndMsg = FALSE;
            }
         }

         if (vbi_proxy_msg_handle_io(&v->io, &ioBlocked, readable))
         {
            assert((v->io.waitRead == FALSE) || (v->io.readOff > 0) || (readable == FALSE));

            if ( (v->io.writeLen == 0) &&
                 (v->io.readLen != 0) &&
                 (v->io.readLen == v->io.readOff) )
            {  /* message completed and no outstanding I/O */

               if (proxy_client_check_msg(v, v->io.readLen, &v->io.readHeader, (VBIPROXY_MSG_BODY *) v->io.pReadBuf))
               {
                  v->rxTotal += v->io.readHeader.len;
                  v->io.readLen = 0;
                  
                  /* process the message - frees the buffer if neccessary */
                  if (proxy_client_take_message(v, (VBIPROXY_MSG_BODY *) v->io.pReadBuf) == FALSE)
                  {  /* protocol error -> abort connection */
                     v->io.pReadBuf = NULL;
                     proxy_client_close(v);
                  }
                  else
                     v->io.pReadBuf = NULL;
               }
               else
               {  /* consistancy error */
                  proxy_client_close(v);
               }

               /* check if one more message can be read before leaving the handler */
               read2ndMsg = TRUE;
            }
         }
         else
         {  /* I/O error; note: acq is not stopped, instead try to reconnect periodically */
            vbi_asprintf(&v->p_errorstr, "%s", _("Lost connection (I/O error)"));
            proxy_client_close(v);
         }
         readable = FALSE;
         loopCount += 1;
      }
      while ( (ioBlocked == FALSE) &&
              (v->io.sock_fd != -1) &&
              (loopCount < CLNT_MAX_MSG_LOOP_COUNT) &&
              ( v->io.waitRead || (v->io.readLen > 0) ||
                (v->io.writeLen > 0) ||
                read2ndMsg ) );
   }

   /* reset "start new block" flag if no data was received using read2ndMsg */
   if (v->io.readLen == 0)
      v->io.waitRead = 0;

   v->ev_hand.blockOnWrite = (v->io.writeLen > 0);
   v->ev_hand.blockOnRead  = !v->ev_hand.blockOnWrite;
}

/* ----------------------------------------------------------------------------
** Wait for I/O event on socket
*/
static int
proxy_client_wait_select( vbi_capture_proxy *v, struct timeval * timeout )
{
        struct timeval tv;
        fd_set fd_rd;
        fd_set fd_wr;
        int    ret;

        if (v->io.sock_fd == -1)
                return -1;

        do {
                FD_ZERO(&fd_rd);
                FD_ZERO(&fd_wr);

                if (v->ev_hand.blockOnRead)
                   FD_SET(v->io.sock_fd, &fd_rd);
                if (v->ev_hand.blockOnWrite)
                   FD_SET(v->io.sock_fd, &fd_wr);

                tv = *timeout; /* Linux kernel overwrites this */

                ret = select(v->io.sock_fd + 1, &fd_rd, &fd_wr, NULL, &tv);

        } while ((ret < 0) && (errno == EINTR));

        if (ret > 0) {
                dprintf1("handle_socket: wait r/w %d/%d -> sock r/w %d/%d\n", v->ev_hand.blockOnRead, v->ev_hand.blockOnWrite, FD_ISSET(v->io.sock_fd, &fd_rd), FD_ISSET(v->io.sock_fd, &fd_wr));

                v->ev_hand.blockOnRead  = FD_ISSET(v->io.sock_fd, &fd_rd);
                v->ev_hand.blockOnWrite = FD_ISSET(v->io.sock_fd, &fd_wr);
        }

        return ret;
}

/* ----------------------------------------------------------------------------
** Start acquisition, i.e. initiate network connection
*/
static vbi_bool proxy_client_start_acq( vbi_capture_proxy *v )
{
   struct timeval tv;
   int ret;

   if (v->state == CLNT_STATE_NULL)
   {
      if ( proxy_client_connect_server(v) )
      {
         v->state = CLNT_STATE_WAIT_CONNECT;

         memset(&v->ev_hand, 0, sizeof(v->ev_hand));
         v->ev_hand.blockOnWrite   = TRUE;

         /* wait for CONNECT_CNF because v->dec must be available */
         while ( (v->state == CLNT_STATE_WAIT_CONNECT) ||
                 (v->state == CLNT_STATE_WAIT_CON_CNF) )
         {
            tv.tv_sec  = 5;
            tv.tv_usec = 0;
            ret = proxy_client_wait_select(v, &tv);

            if (ret <= 0)
                    break;

            proxy_client_handle_socket(v);
        }
      }
      else
      {  /* connect failed -> abort */
         v->state = CLNT_STATE_NULL;
      }
   }
   else
      dprintf2("start_acq: acq already enabled\n");

   return (v->state != CLNT_STATE_NULL);
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
      dprintf2("stop_acq: acq not enabled\n");
}

/* ----------------------------------------------------------------------------
** Check for protocol timeouts
** - returns TRUE if an error occurred since the last polling
**   note: in subsequent calls it's no longer set, even if acq is not running
** - should be called every second
** - also used to initiate connect retries
*/
static vbi_bool proxy_client_check_timeouts( vbi_capture_proxy *v )
{
   vbi_bool    stopped;
   time_t  now = time(NULL);

   stopped = FALSE;

   /* check for protocol or network I/O timeout */
   if ( (now > v->io.lastIoTime + SRV_REPLY_TIMEOUT) &&
        ( (vbi_proxy_msg_is_idle(&v->io) == FALSE) ||
          (v->state == CLNT_STATE_WAIT_CONNECT) ||
          (v->state == CLNT_STATE_WAIT_CON_CNF) ))
   {
      dprintf2("check_timeouts: network timeout\n");
      vbi_asprintf(&v->p_errorstr, "%s", _("Lost connection (I/O timeout)"));
      proxy_client_close(v);
   }
   else if ( (v->state == CLNT_STATE_RETRY) &&
             (now > v->io.lastIoTime + CLNT_RETRY_INTERVAL) )
   {
      dprintf1("check_timeouts: initiate connect retry\n");
      v->io.lastIoTime = now;
      if ( proxy_client_connect_server(v) )
      {
         v->state  = CLNT_STATE_WAIT_CONNECT;

         memset(&v->ev_hand, 0, sizeof(v->ev_hand));
         v->ev_hand.blockOnWrite   = TRUE;
      }
      else
         stopped = TRUE;
   }

   if (v->state == CLNT_STATE_ERROR)
   {  /* an error has occured and the upper layer is not yet informed */
      dprintf1("check_timeouts: report error\n");
      v->state = CLNT_STATE_RETRY;
      stopped = TRUE;
   }

   return stopped;
}

/* ----------------------------------------------------------------------------
** Read one frame's worth of sliced VBI data
*/
static int
proxy_read( vbi_capture *vc, vbi_capture_buffer **raw,
            vbi_capture_buffer **sliced, struct timeval *timeout )
{
        vbi_capture_proxy *v = PARENT(vc, vbi_capture_proxy, capture);
        int ret;

        if (v->p_data_ind != NULL) {
                free(v->p_data_ind);
                v->p_data_ind = NULL;
                v->sliced_buffer.data = NULL;
        }

        ret = proxy_client_wait_select(v, timeout);
        if (ret <= 0)
                return ret;

        if (raw != NULL) {
                /* warn that raw buffer is not available */
        }

        pthread_testcancel();

        proxy_client_handle_socket(v);
        if (proxy_client_check_timeouts(v))
           return -1;

        if (v->p_data_ind != NULL) {
                int lines = v->p_data_ind->data_ind.line_count;

                if (*sliced) {
                        memcpy( (vbi_sliced *)(*sliced)->data,
                                v->p_data_ind->data_ind.sliced,
                                lines * sizeof(vbi_sliced) );

                        (*sliced)->timestamp = v->p_data_ind->data_ind.timestamp;
                        (*sliced)->size      = lines * sizeof(vbi_sliced);

                        free(v->p_data_ind);
                        v->p_data_ind = NULL;
                } else {
                        *sliced = &v->sliced_buffer;
                        (*sliced)->data      = v->p_data_ind->data_ind.sliced;
                        (*sliced)->timestamp = v->p_data_ind->data_ind.timestamp;
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

        if (v->p_data_ind != NULL)
                free(v->p_data_ind);
        else /* note: v->sliced_buffer.data points into p_data_ind */
                assert(v->sliced_buffer.data == NULL);

        if (v->p_errorstr != NULL)
           free(v->p_errorstr);

        free(v);
}

static int
proxy_fd(vbi_capture *vc)
{
        vbi_capture_proxy *v = PARENT(vc, vbi_capture_proxy, capture);

        return v->io.sock_fd;
}

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

        if (trace)
                fprintf(stderr, "Try to connect vbi proxy, libzvbi interface rev.\n%s\n", rcsid);

        v = (vbi_capture_proxy *) calloc(1, sizeof(*v));
        if (v == NULL) {
                vbi_asprintf(pp_errorstr, _("Virtual memory exhausted."));
                errno = ENOMEM;
                return NULL;
        }

        v->capture.parameters = proxy_parameters;
        v->capture._delete = proxy_delete;
        v->capture.get_fd = proxy_fd;
        v->capture.read = proxy_read;

        v->scanning     = scanning,
        v->services     = *services;
        v->strict       = strict;
        v->buffer_count = buffers;
        v->trace        = trace;

        v->p_srv_port = vbi_proxy_msg_get_socket_name(dev_name);
        v->p_srv_host = NULL;

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
