/*
 *  Basic I/O between VBI proxy client & server
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
 *  Description:
 *
 *    This module contains a collection of functions for lower-level
 *    socket I/O which are shared between proxy daemon and clients.
 *    Error output is different for daemon and clients: daemon logs
 *    to a file or syslog facility, while the client returns error
 *    strings to the caller, which can be passed to the upper levels
 *    (e.g. the user interface)
 *
 *    Both UNIX domain and IPv4 and IPv6 sockets are implemented, but
 *    the latter ones are currently not officially supported.
 *
 *  $Id: proxy-msg.c,v 1.3 2003/05/03 12:05:26 tomzo Exp $
 *
 *  $Log: proxy-msg.c,v $
 *  Revision 1.3  2003/05/03 12:05:26  tomzo
 *  - use new function vbi_proxy_msg_resolve_symlinks() to get unique device path,
 *    e.g. allow both /dev/vbi and /dev/vbi0 to work as proxy device args
 *  - added new func vbi_proxy_msg_set_debug_level()
 *  - fixed debug output level in various dprintf statements
 *  - fixed copyright headers, added description to file headers
 *
 */

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#ifdef ENABLE_PROXY

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/signal.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <syslog.h>
#include <assert.h>

#include "bcd.h"
#include "vbi.h"
#include "io.h"
#include "proxy-msg.h"

#define dprintf1(fmt, arg...)    if (proxy_msg_trace >= 1) printf("proxy_msg: " fmt, ## arg)
#define dprintf2(fmt, arg...)    if (proxy_msg_trace >= 2) printf("proxy_msg: " fmt, ## arg)
static int proxy_msg_trace = 1;


/* settings for log output - only used by the daemon */
static struct
{
   vbi_bool  do_logtty;
   int       sysloglev;
   int       fileloglev;
   char    * pLogfileName;
} proxy_msg_logcf =
{
   FALSE, 0, 0, NULL
};

/* ----------------------------------------------------------------------------
** Local settings
*/
#define SRV_IO_TIMEOUT             60
#define SRV_LISTEN_BACKLOG_LEN     10
#define SRV_CLNT_SOCK_BASE_PATH    "/tmp/vbiproxy"

/* ----------------------------------------------------------------------------
** Append entry to logfile
*/
void vbi_proxy_msg_logger( int level, int clnt_fd, int errCode, const char * pText, ... )
{
   va_list argl;
   char timestamp[32], fdstr[20];
   const char *argv[10];
   uint32_t argc, idx;
   int fd;
   time_t now = time(NULL);

   if (pText != NULL)
   {
      /* open the logfile, if one is configured */
      if ( (level <= proxy_msg_logcf.fileloglev) &&
           (proxy_msg_logcf.pLogfileName != NULL) )
      {
         fd = open(proxy_msg_logcf.pLogfileName, O_WRONLY|O_CREAT|O_APPEND, 0666);
         if (fd >= 0)
         {  /* each line in the file starts with a timestamp */
            strftime(timestamp, sizeof(timestamp) - 1, "[%d/%b/%Y:%H:%M:%S +0000] ", gmtime(&now));
            write(fd, timestamp, strlen(timestamp));
         }
      }
      else
         fd = -1;

      if (proxy_msg_logcf.do_logtty && (level <= LOG_WARNING))
         fprintf(stderr, "vbiproxy: ");

      argc = 0;
      memset(argv, 0, sizeof(argv));
      /* add pointer to file descriptor (for client requests) or pid (for general infos) */
      if (clnt_fd != -1)
         sprintf(fdstr, "fd %d: ", clnt_fd);
      else
      {
         sprintf(fdstr, "pid %d: ", (int)getpid());
      }
      argv[argc++] = fdstr;

      /* add pointer to first log output string */
      argv[argc++] = pText;

      /* append pointers to the rest of the log strings */
      va_start(argl, pText);
      while ((argc < 5) && ((pText = va_arg(argl, char *)) != NULL))
      {
         argv[argc++] = pText;
      }
      va_end(argl);

      /* add system error message */
      if (errCode != 0)
      {
         argv[argc++] = strerror(errCode);
      }

      /* print the strings to the file and/or stderr */
      for (idx=0; idx < argc; idx++)
      {
         if (fd >= 0)
            write(fd, argv[idx], strlen(argv[idx]));
         if (proxy_msg_logcf.do_logtty && (level <= LOG_WARNING))
            fprintf(stderr, "%s", argv[idx]);
      }

      /* terminate the line with a newline character and close the file */
      if (fd >= 0)
      {
         write(fd, "\n", 1);
         close(fd);
      }
      if (proxy_msg_logcf.do_logtty && (level <= LOG_WARNING))
      {
         fprintf(stderr, "\n");
         fflush(stderr);
      }

      /* syslog output */
      if (level <= proxy_msg_logcf.sysloglev)
      {
         switch (argc)
         {
            case 1: syslog(level, "%s", argv[0]); break;
            case 2: syslog(level, "%s%s", argv[0], argv[1]); break;
            case 3: syslog(level, "%s%s%s", argv[0], argv[1],argv[2]); break;
            case 4: syslog(level, "%s%s%s%s", argv[0], argv[1],argv[2],argv[3]); break;
         }
      }
   }
}

/* ----------------------------------------------------------------------------
** Set parameters for event logging
** - loglevel usage
**   ERR    : fatal errors (which lead to program termination)
**   WARNING: this shouldn't happen error (OS failure or internal errors)
**   NOTICE : start/stop of the daemon
**   INFO   : connection establishment and shutdown
*/
void vbi_proxy_msg_set_logging( vbi_bool do_logtty, int sysloglev,
                                int fileloglev, const char * pLogfileName )
{
   /* free the memory allocated for the old config strings */
   if (proxy_msg_logcf.pLogfileName != NULL)
   {
      free(proxy_msg_logcf.pLogfileName);
      proxy_msg_logcf.pLogfileName = NULL;
   }

   proxy_msg_logcf.do_logtty = do_logtty;

   /* make a copy of the new config strings */
   if (pLogfileName != NULL)
   {
      proxy_msg_logcf.pLogfileName = malloc(strlen(pLogfileName) + 1);
      strcpy(proxy_msg_logcf.pLogfileName, pLogfileName);
      proxy_msg_logcf.fileloglev = ((fileloglev > 0) ? (fileloglev + LOG_ERR) : -1);
   }
   else
      proxy_msg_logcf.fileloglev = -1;

   if (sysloglev && !proxy_msg_logcf.sysloglev)
   {
      openlog("vbiproxy", LOG_PID, LOG_DAEMON);
   }
   else if (!sysloglev && proxy_msg_logcf.sysloglev)
   {
   }

   /* convert GUI log-level setting to syslog enum value */
   proxy_msg_logcf.sysloglev = ((sysloglev > 0) ? (sysloglev + LOG_ERR) : -1);
}

/* ----------------------------------------------------------------------------
** Enable debug output
*/
void vbi_proxy_msg_set_debug_level( int level )
{
   proxy_msg_trace = level;
}

/* ----------------------------------------------------------------------------
** Check for incomplete read or write buffer
*/
vbi_bool vbi_proxy_msg_is_idle( VBIPROXY_MSG_STATE * pIO )
{
   return ( (pIO->writeLen == 0) &&
            (pIO->waitRead == FALSE) && (pIO->readLen == 0) );
}

/* ----------------------------------------------------------------------------
** Check for I/O timeout
** - returns TRUE in case of timeout
*/
vbi_bool vbi_proxy_msg_check_timeout( VBIPROXY_MSG_STATE * pIO, time_t now )
{
   return ( (now > pIO->lastIoTime + SRV_IO_TIMEOUT) &&
            (vbi_proxy_msg_is_idle(pIO) == FALSE) );
}

/* ----------------------------------------------------------------------------
** Read or write a message from/to the network socket
** - only one of read or write is processed at the same time
** - reading is done in 2 phases: first the length of the message is read into
**   a small buffer; then a buffer is allocated for the complete message and the
**   length variable copied into it and the rest of the message read afterwords.
** - a closed network connection is indicated by a 0 read from a readable socket.
**   Readability is indicated by the select syscall and passed here via
**   parameter closeOnZeroRead.
** - after errors the I/O state (indicated by FALSE result) is not reset, because
**   the caller is expected to close the connection.
*/
vbi_bool vbi_proxy_msg_handle_io( VBIPROXY_MSG_STATE * pIO, vbi_bool * pBlocked, vbi_bool closeOnZeroRead )
{
   time_t   now;
   ssize_t  len;
   vbi_bool     err = FALSE;
   vbi_bool     result = TRUE;

   *pBlocked = FALSE;
   now = time(NULL);
   if (pIO->writeLen > 0)
   {
      /* write the message header */
      assert(pIO->writeLen >= sizeof(VBIPROXY_MSG_HEADER));
      if (pIO->writeOff < sizeof(VBIPROXY_MSG_HEADER))
      {  /* write message header */
         len = send(pIO->sock_fd, ((char *)&pIO->writeHeader) + pIO->writeOff, sizeof(VBIPROXY_MSG_HEADER) - pIO->writeOff, 0);
         if (len >= 0)
         {
            pIO->lastIoTime = now;
            pIO->writeOff += len;
         }
         else
            err = TRUE;
      }

      /* write the message body, if the header is written */
      if ((err == FALSE) && (pIO->writeOff >= sizeof(VBIPROXY_MSG_HEADER)) && (pIO->writeOff < pIO->writeLen))
      {
         len = send(pIO->sock_fd, ((char *)pIO->pWriteBuf) + pIO->writeOff - sizeof(VBIPROXY_MSG_HEADER), pIO->writeLen - pIO->writeOff, 0);
         if (len > 0)
         {
            pIO->lastIoTime = now;
            pIO->writeOff += len;
         }
         else
            err = TRUE;
      }

      if (err == FALSE)
      {
         if (pIO->writeOff >= pIO->writeLen)
         {  /* all data has been written -> free the buffer; reset write state */
            if (pIO->freeWriteBuf)
               free(pIO->pWriteBuf);
            pIO->pWriteBuf = NULL;
            pIO->writeLen  = 0;
         }
         else if (pIO->writeOff < pIO->writeLen)
            *pBlocked = TRUE;
      }
      else if ((errno != EAGAIN) && (errno != EINTR))
      {  /* network error -> close the connection */
         dprintf1("handle_io: write error on fd %d: %s\n", pIO->sock_fd, strerror(errno));
         result = FALSE;
      }
      else if (errno == EAGAIN)
         *pBlocked = TRUE;
   }
   else if (pIO->waitRead || (pIO->readLen > 0))
   {
      len = 0;  /* compiler dummy */
      if (pIO->waitRead)
      {  /* in read phase one: read the message length */
         assert(pIO->readOff < sizeof(pIO->readHeader));
         len = recv(pIO->sock_fd, (char *)&pIO->readHeader + pIO->readOff, sizeof(pIO->readHeader) - pIO->readOff, 0);
         if (len > 0)
         {
            closeOnZeroRead = FALSE;
            pIO->lastIoTime = now;
            pIO->readOff += len;
            if (pIO->readOff >= sizeof(pIO->readHeader))
            {  /* message length variable has been read completely */
               /* convert from network byte order (big endian) to host byte order */
               pIO->readLen = ntohs(pIO->readHeader.len);
               pIO->readHeader.len = pIO->readLen;
               /*XXX//dprintf1("handle_io: fd %d: new block: size %d\n", pIO->sock_fd, pIO->readLen); */
               if ((pIO->readLen < VBIPROXY_MSG_MAXSIZE) &&
                   (pIO->readLen >= sizeof(VBIPROXY_MSG_HEADER)))
               {  /* message size acceptable -> allocate a buffer with the given size */
                  if (pIO->readLen > sizeof(VBIPROXY_MSG_HEADER))
                     pIO->pReadBuf = malloc(pIO->readLen - sizeof(VBIPROXY_MSG_HEADER));
                  /* enter the second phase of the read process */
                  pIO->waitRead = FALSE;
               }
               else
               {  /* illegal message size -> protocol error */
                  dprintf1("handle_io: fd %d: illegal block size %d\n", pIO->sock_fd, pIO->readLen);
                  result = FALSE;
               }
            }
            else
               *pBlocked = TRUE;
         }
         else
            err = TRUE;
      }

      if ((err == FALSE) && (pIO->waitRead == FALSE) && (pIO->readLen > sizeof(VBIPROXY_MSG_HEADER)))
      {  /* in read phase two: read the complete message into the allocated buffer */
         assert(pIO->pReadBuf != NULL);
         len = recv(pIO->sock_fd, pIO->pReadBuf + pIO->readOff - sizeof(VBIPROXY_MSG_HEADER), pIO->readLen - pIO->readOff, 0);
         if (len > 0)
         {
            pIO->lastIoTime = now;
            pIO->readOff += len;
         }
         else
            err = TRUE;
      }

      if (err == FALSE)
      {
         if (pIO->readOff < pIO->readLen)
         {  /* not all data has been read yet */
            *pBlocked = TRUE;
         }
      }
      else
      {
         if ((len == 0) && closeOnZeroRead)
         {  /* zero bytes read after select returned readability -> network error or connection closed by peer */
            dprintf1("handle_io: zero len read on fd %d\n", pIO->sock_fd);
            result = FALSE;
         }
         else if ((len < 0) && (errno != EAGAIN) && (errno != EINTR))
         {  /* network error -> close the connection */
            dprintf1("handle_io: read error on fd %d: len=%d, %s\n", pIO->sock_fd, len, strerror(errno));
            result = FALSE;
         }
         else if (errno == EAGAIN)
            *pBlocked = TRUE;
      }
   }

   return result;
}

/* ----------------------------------------------------------------------------
** Free resources allocated for IO
*/
void vbi_proxy_msg_close_io( VBIPROXY_MSG_STATE * pIO )
{
   if (pIO->sock_fd != -1)
   {
      close(pIO->sock_fd);
      pIO->sock_fd = -1;
   }

   if (pIO->pReadBuf != NULL)
   {
      free(pIO->pReadBuf);
      pIO->pReadBuf = NULL;
   }

   if (pIO->pWriteBuf != NULL)
   {
      if (pIO->freeWriteBuf)
         free(pIO->pWriteBuf);
      pIO->pWriteBuf = NULL;
   }
}

/* ----------------------------------------------------------------------------
** Fill a magic header struct with protocol constants
*/
void vbi_proxy_msg_fill_magics( VBIPROXY_MAGICS * p_magic )
{
   memcpy(p_magic->protocol_magic, VBIPROXY_MAGIC_STR, VBIPROXY_MAGIC_LEN);
   p_magic->protocol_compat_version = VBIPROXY_COMPAT_VERSION;
   p_magic->protocol_version = VBIPROXY_VERSION;
   p_magic->endian_magic = VBIPROXY_ENDIAN_MAGIC;
}

/* ----------------------------------------------------------------------------
** Create a new message and prepare the I/O state for writing
** - length and pointer of the body may be zero (no payload)
*/
void vbi_proxy_msg_write( VBIPROXY_MSG_STATE * p_io, VBIPROXY_MSG_TYPE type,
                          uint32_t msgLen, void * pMsg, vbi_bool freeBuf )
{
   assert((p_io->waitRead == FALSE) && (p_io->readLen == 0));  /* I/O must be idle */
   assert((p_io->writeLen == 0) && (p_io->pWriteBuf == NULL));
   assert((msgLen == 0) || (pMsg != NULL));

   dprintf2("write: msg type %d, len %d\n", type, sizeof(VBIPROXY_MSG_HEADER) + msgLen);

   p_io->pWriteBuf    = pMsg;
   p_io->freeWriteBuf = freeBuf;
   p_io->writeLen     = sizeof(VBIPROXY_MSG_HEADER) + msgLen;
   p_io->writeOff     = 0;

   /* message header: length is coded in network byte order (i.e. big endian) */
   p_io->writeHeader.len  = htons(p_io->writeLen);
   p_io->writeHeader.type = type;
}

/* ----------------------------------------------------------------------------
** Transmit one buffer of sliced data
** - returns FALS upon I/O error
** - also returns a "blocked" flag which is TRUE if not all data could be written
**   can be used by the caller to "stuff" the pipe, i.e. write a series of messages
**   until the pipe is full
** - XXX optimization required: don't copy the block
*/
vbi_bool vbi_proxy_msg_write_queue( VBIPROXY_MSG_STATE * p_io, vbi_bool * p_blocked,
                                    unsigned int services, int max_lines,
                                    vbi_sliced * p_lines, unsigned int line_count,
                                    double timestamp )
{
   VBIPROXY_DATA_IND * p_data_ind;
   uint32_t  body_size;
   vbi_bool  result = TRUE;
   int idx;
   #ifdef linux
   int val;
   #endif

   if ((p_io != NULL) && (p_blocked != NULL) && (p_lines != NULL))
   {
      body_size = VBIPROXY_DATA_IND_SIZE(line_count);
      p_data_ind = malloc(body_size);
      p_data_ind->timestamp  = timestamp;

      /* filter for services requested by this client */
      p_data_ind->line_count = 0;
      for (idx = 0; (idx < line_count) && (idx < max_lines); idx++)
      {
         if ((p_lines[idx].id & services) != 0)
         {
            memcpy(p_data_ind->sliced + p_data_ind->line_count, p_lines + idx, sizeof(vbi_sliced));
            p_data_ind->line_count += 1;
         }
      }
      body_size = VBIPROXY_DATA_IND_SIZE(p_data_ind->line_count);

      dprintf2("msg_write_queue: fd %d: msg body size %d\n", p_io->sock_fd, body_size);
      p_io->pWriteBuf        = p_data_ind;
      p_io->freeWriteBuf     = TRUE;
      p_io->writeLen         = sizeof(VBIPROXY_MSG_HEADER) + body_size;
      p_io->writeOff         = 0;
      p_io->writeHeader.len  = htons(p_io->writeLen);
      p_io->writeHeader.type = MSG_TYPE_DATA_IND;

      #ifdef linux
      val = 1;  /* "kork" the socket so that only one TCP packet is sent for the message, if possible */
      setsockopt(p_io->sock_fd, SOL_TCP, TCP_CORK, &val, sizeof(val));
      #endif

      if (vbi_proxy_msg_handle_io(p_io, p_blocked, FALSE))
      {
         /* if the last block could not be transmitted fully, quit the loop */
         if (p_io->writeLen > 0)
         {
            dprintf2("msg_write_queue: socket blocked\n");
            *p_blocked = TRUE;
         }
      }
      else
         result = FALSE;

      #ifdef linux
      val = 0;  /* unkork the socket */
      setsockopt(p_io->sock_fd, SOL_TCP, TCP_CORK, &val, sizeof(val));
      #endif
   }
   else
      dprintf1("msg_write_queue: illegal NULL ptr params\n");

   return result;
}

/* ----------------------------------------------------------------------------
** Implementation of the C library address handling functions
** - for platforms which to not have them in libc
** - documentation see the manpages
*/
#ifndef HAVE_GETADDRINFO

#ifndef AI_PASSIVE
# define AI_PASSIVE 1
#endif

struct addrinfo
{
   int  ai_flags;
   int  ai_family;
   int  ai_socktype;
   int  ai_protocol;
   struct sockaddr * ai_addr;
   int  ai_addrlen;
};

enum
{
   GAI_UNSUP_FAM       = -1,
   GAI_NO_SERVICE_NAME = -2,
   GAI_UNKNOWN_SERVICE = -3,
   GAI_UNKNOWN_HOST    = -4,
};

static int getaddrinfo( const char * pHostName, const char * pServiceName, const struct addrinfo * pInParams, struct addrinfo ** ppResult )
{
   struct servent  * pServiceEntry;
   struct hostent  * pHostEntry;
   struct addrinfo * res;
   char  * pServiceNumEnd;
   uint32_t  port;
   int   result;

   res = malloc(sizeof(struct addrinfo));
   *ppResult = res;

   memset(res, 0, sizeof(*res));
   res->ai_socktype  = pInParams->ai_socktype;
   res->ai_family    = pInParams->ai_family;
   res->ai_protocol  = pInParams->ai_protocol;

   if (pInParams->ai_family == PF_INET)
   {
      if ((pServiceName != NULL) || (*pServiceName == 0))
      {
         port = strtol(pServiceName, &pServiceNumEnd, 0);
         if (*pServiceNumEnd != 0)
         {
            pServiceEntry = getservbyname(pServiceName, "tcp");
            if (pServiceEntry != NULL)
               port = ntohs(pServiceEntry->s_port);
            else
               port = 0;
         }

         if (port != 0)
         {
            if (pHostName != NULL)
               pHostEntry = gethostbyname(pHostName);
            else
               pHostEntry = NULL;

            if ((pHostName == NULL) || (pHostEntry != NULL))
            {
               struct sockaddr_in * iad;

               iad = malloc(sizeof(struct sockaddr_in));
               res->ai_addr    = (struct sockaddr *) iad;
               res->ai_addrlen = sizeof(struct sockaddr_in);

               iad->sin_family      = AF_INET;
               iad->sin_port        = htons(port);
               if (pHostName != NULL)
                  memcpy(&iad->sin_addr, (char *) pHostEntry->h_addr, pHostEntry->h_length);
               else
                  iad->sin_addr.s_addr = INADDR_ANY;
               result = 0;
            }
            else
               result = GAI_UNKNOWN_HOST;
         }
         else
            result = GAI_UNKNOWN_SERVICE;
      }
      else
         result = GAI_NO_SERVICE_NAME;
   }
   else
      result = GAI_UNSUP_FAM;

   if (result != 0)
   {
      free(res);
      *ppResult = NULL;
   }
   return result;
}

static void freeaddrinfo( struct addrinfo * res )
{
   if (res->ai_addr != NULL)
      free(res->ai_addr);
   free(res);
}

static char * gai_strerror( int errCode )
{
   switch (errCode)
   {
      case GAI_UNSUP_FAM:       return "unsupported protocol family";
      case GAI_NO_SERVICE_NAME: return "missing service name or port number for TCP/IP";
      case GAI_UNKNOWN_SERVICE: return "unknown service name";
      case GAI_UNKNOWN_HOST:    return "unknown host";
      default:                  return "internal or unknown error";
   }
}
#endif  /* HAVE_GETADDRINFO */

/* ----------------------------------------------------------------------------
** Get socket address for PF_UNIX aka PF_LOCAL address family
** - result is in the same format as from getaddrinfo
** - note: Linux getaddrinfo currently supports PF_UNIX queries too, however
**   this feature is not standardized and hence not portable (e.g. to NetBSD)
*/
static int vbi_proxy_msg_get_local_socket_addr( const char * pPathName, const struct addrinfo * pInParams, struct addrinfo ** ppResult )
{
   struct addrinfo * res;
   struct sockaddr_un * saddr;

   if ((pInParams->ai_family == PF_UNIX) && (pPathName != NULL))
   {
      /* note: use regular malloc instead of malloc in case memory is freed by the libc internal freeaddrinfo */
      res = malloc(sizeof(struct addrinfo));
      *ppResult = res;

      memset(res, 0, sizeof(*res));
      res->ai_socktype  = pInParams->ai_socktype;
      res->ai_family    = pInParams->ai_family;
      res->ai_protocol  = pInParams->ai_protocol;

      saddr = malloc(sizeof(struct sockaddr_un));
      res->ai_addr      = (struct sockaddr *) saddr;
      res->ai_addrlen   = sizeof(struct sockaddr_un);

      strncpy(saddr->sun_path, pPathName, sizeof(saddr->sun_path) - 1);
      saddr->sun_path[sizeof(saddr->sun_path) - 1] = 0;
      saddr->sun_family = AF_UNIX;

      return 0;
   }
   else
      return -1;
}

/* ----------------------------------------------------------------------------
** Open socket for listening
*/
int vbi_proxy_msg_listen_socket( vbi_bool is_tcp_ip, const char * listen_ip, const char * listen_port )
{
   struct addrinfo    ask, *res;
   int  opt, rc;
   int  sock_fd;
   vbi_bool result = FALSE;

   memset(&ask, 0, sizeof(ask));
   ask.ai_flags    = AI_PASSIVE;
   ask.ai_socktype = SOCK_STREAM;
   sock_fd = -1;
   res = NULL;

   #ifdef PF_INET6
   if (is_tcp_ip)
   {  /* try IP-v6: not supported everywhere yet, so errors must be silently ignored */
      ask.ai_family = PF_INET6;
      rc = getaddrinfo(listen_ip, listen_port, &ask, &res);
      if (rc == 0)
      {
         sock_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
         if (sock_fd == -1)
         {
            dprintf2("listen_socket: socket (ipv6)\n");
            freeaddrinfo(res);
            res = NULL;
         }
      }
      else
         dprintf2("listen_socket: getaddrinfo (ipv6): %s\n", gai_strerror(rc));
   }
   #endif

   if (sock_fd == -1)
   {
      if (is_tcp_ip)
      {  /* IP-v4 (IP-address is optional, defaults to localhost) */
         ask.ai_family = PF_INET;
         rc = getaddrinfo(listen_ip, listen_port, &ask, &res);
      }
      else
      {  /* UNIX domain socket: named pipe located in /tmp directory */
         ask.ai_family = PF_UNIX;
         rc = vbi_proxy_msg_get_local_socket_addr(listen_port, &ask, &res);
      }
      if (rc == 0)
      {
         sock_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
         if (sock_fd == -1)
         {
            vbi_proxy_msg_logger(LOG_ERR, -1, errno, "socket create failed: ", NULL);
         }
      }
      else
         vbi_proxy_msg_logger(LOG_ERR, -1, 0, "Invalid hostname or service/port: ", gai_strerror(rc), NULL);
   }

   if (sock_fd != -1)
   {
      /* allow immediate reuse of the port (e.g. after server stop and restart) */
      opt = 1;
      if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, (void *)&opt, sizeof(opt)) == 0)
      {
         /* make the socket non-blocking */
         if (fcntl(sock_fd, F_SETFL, O_NONBLOCK) == 0)
         {
            /* bind the socket */
            if (bind(sock_fd, res->ai_addr, res->ai_addrlen) == 0)
            {
               #ifdef linux
               /* set socket permissions: r/w allowed to everyone */
               if ( (is_tcp_ip == FALSE) &&
                    (chmod(listen_port, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH) != 0) )
                  vbi_proxy_msg_logger(LOG_WARNING, -1, errno, "chmod failed for named socket: ", NULL);
               #endif

               /* enable listening for new connections */
               if (listen(sock_fd, SRV_LISTEN_BACKLOG_LEN) == 0)
               {  /* finished without errors */
                  result = TRUE;
               }
               else
               {
                  vbi_proxy_msg_logger(LOG_ERR, -1, errno, "socket listen failed: ", NULL);
                  if ((is_tcp_ip == FALSE) && (listen_port != NULL))
                     unlink(listen_port);
               }
            }
            else
               vbi_proxy_msg_logger(LOG_ERR, -1, errno, "socket bind failed: ", NULL);
         }
         else
            vbi_proxy_msg_logger(LOG_ERR, -1, errno, "failed to set socket non-blocking: ", NULL);
      }
      else
         vbi_proxy_msg_logger(LOG_ERR, -1, errno, "socket setsockopt(SOL_SOCKET=SO_REUSEADDR) failed: ", NULL);
   }

   if (res != NULL)
      freeaddrinfo(res);

   if ((result == FALSE) && (sock_fd != -1))
   {
      close(sock_fd);
      sock_fd = -1;
   }

   return sock_fd;
}

/* ----------------------------------------------------------------------------
** Stop listening a socket
*/
void vbi_proxy_msg_stop_listen( vbi_bool is_tcp_ip, int sock_fd, char * pSrvPort )
{
   if (sock_fd != -1)
   {
      if (is_tcp_ip == FALSE)
         unlink(pSrvPort);

      close(sock_fd);
      sock_fd = -1;
   }
}

/* ----------------------------------------------------------------------------
** Accept a new connection
*/
int vbi_proxy_msg_accept_connection( int listen_fd )
{
   struct hostent * hent;
   char  hname_buf[129];
   uint32_t  length, maxLength;
   struct {  /* allocate enough room for all possible types of socket address structs */
      struct sockaddr  sa;
      char             padding[64];
   } peerAddr;
   int   sock_fd;
   vbi_bool  result = FALSE;

   maxLength = length = sizeof(peerAddr);
   sock_fd = accept(listen_fd, &peerAddr.sa, &length);
   if (sock_fd != -1)
   {
      if (length <= maxLength)
      {
         if (fcntl(sock_fd, F_SETFL, O_NONBLOCK) == 0)
         {
            if (peerAddr.sa.sa_family == AF_INET)
            {
               hent = gethostbyaddr((void *) &peerAddr.sa, maxLength, AF_INET);
               if (hent != NULL)
               {
                  strncpy(hname_buf, hent->h_name, sizeof(hname_buf) -1);
                  hname_buf[sizeof(hname_buf) - 1] = 0;
               }
               else
                  sprintf(hname_buf, "%s, port %d", inet_ntoa(((struct sockaddr_in *) &peerAddr.sa)->sin_addr), ((struct sockaddr_in *) &peerAddr.sa)->sin_port);

               vbi_proxy_msg_logger(LOG_INFO, sock_fd, 0, "new connection from ", hname_buf, NULL);
               result = TRUE;
            }
            #ifdef HAVE_GETADDRINFO
            else if (peerAddr.sa.sa_family == AF_INET6)
            {
               if (getnameinfo(&peerAddr.sa, length, hname_buf, sizeof(hname_buf) - 1, NULL, 0, 0) == 0)
               {  /* address could be resolved to hostname */
                  vbi_proxy_msg_logger(LOG_INFO, sock_fd, 0, "new connection from ", hname_buf, NULL);
                  result = TRUE;
               }
               else if (getnameinfo(&peerAddr.sa, length, hname_buf, sizeof(hname_buf) - 1, NULL, 0,
                                    NI_NUMERICHOST | NI_NUMERICSERV) == 0)
               {  /* resolver failed - but numeric conversion was successful */
                  dprintf2("accept_connection: IPv6 resolver failed for %s\n", hname_buf);
                  vbi_proxy_msg_logger(LOG_INFO, sock_fd, 0, "new connection from ", hname_buf, NULL);
                  result = TRUE;
               }
               else
               {  /* neither name looup nor numeric name output succeeded -> fatal error */
                  vbi_proxy_msg_logger(LOG_INFO, sock_fd, errno, "new connection: failed to get IPv6 peer name or IP-addr: ", NULL);
                  result = FALSE;
               }
            }
            #endif
            else if (peerAddr.sa.sa_family == AF_UNIX)
            {
               vbi_proxy_msg_logger(LOG_INFO, sock_fd, 0, "new connection from localhost via named socket", NULL);
               result = TRUE;
            }
            else
            {  /* neither INET nor named socket -> internal error */
               sprintf(hname_buf, "%d", peerAddr.sa.sa_family);
               vbi_proxy_msg_logger(LOG_WARNING, -1, 0, "new connection via unexpected protocol family ", hname_buf, NULL);
            }
         }
         else
         {  /* fcntl failed: OS error (should never happen) */
            vbi_proxy_msg_logger(LOG_WARNING, -1, errno, "new connection: failed to set socket to non-blocking: ", NULL);
         }
      }
      else
      {  /* socket address buffer too small: internal error */
         sprintf(hname_buf, "need %d, have %d", length, maxLength);
         vbi_proxy_msg_logger(LOG_WARNING, -1, 0, "new connection: saddr buffer too small: ", hname_buf, NULL);
      }

      if (result == FALSE)
      {  /* error -> drop the connection */
         close(sock_fd);
         sock_fd = -1;
      }
   }
   else
   {  /* connect accept failed: remote host may already have closed again */
      if (errno == EAGAIN)
         vbi_proxy_msg_logger(LOG_INFO, -1, errno, "accept failed: ", NULL);
   }

   return sock_fd;
}

/* ----------------------------------------------------------------------------
** Follow path through symlinks (in an attempt to get a unique path)
** - note: "." and ".." in relative symlinks appear to be resolved by Linux
**   already when creating the symlink
*/
static char * vbi_proxy_msg_resolve_symlinks( const char * p_dev_name )
{
   struct stat stbuf;
   char   link_name[MAXPATHLEN + 1];
   char * p_path;
   char * p_tmp;
   char * p_tmp2;
   int    name_len;
   int    res;
   int    slink_idx;

   p_path = strdup(p_dev_name);

   for (slink_idx = 0; slink_idx < 100; slink_idx++)
   {
      res = lstat(p_path, &stbuf);
      if ((res == 0) && S_ISLNK(stbuf.st_mode))
      {
         name_len = readlink(p_path, link_name, sizeof(link_name));
         if ((name_len > 0) && (name_len < sizeof(link_name)))
         {
            link_name[name_len] = 0;
            dprintf2("resolve_symlinks: following symlink %s to: %s\n", p_path, link_name);
            if (link_name[0] != '/')
            {  /* relative path -> replace only last path element */
               p_tmp = malloc(strlen(p_path) + name_len + 1 + 1);
               p_tmp2 = strrchr(p_path, '/');
               if (p_tmp2 != NULL)
               {  /* copy former path up to and including the separator character */
                  p_tmp2 += 1;
                  strncpy(p_tmp, p_path, p_tmp2 - p_path);
               }
               else
               {  /* no path separator in the former path -> replace completely */
                  p_tmp2 = p_path;
               }
               /* append the path read from the symlink file */
               strcpy(p_tmp + (p_tmp2 - p_path), link_name);
            }
            else
            {  /* absolute path -> replace symlink completely */
               p_tmp = strdup(link_name);
            }

            free((void *) p_path);
            p_path = p_tmp;
         }
         else
         {  /* symlink string too long for the buffer */
            if (name_len > 0)
            {
               link_name[sizeof(link_name) - 1] = 0;
               dprintf1("resolve_symlinks: abort: symlink too long: %s\n", link_name);
            }
            else
               dprintf1("resolve_symlinks: zero length symlink - abort\n");
            break;
         }
      }
      else
         break;
   }

   if (slink_idx >= 100)
      dprintf1("resolve_symlinks: symlink level too deep: abort after %d\n", slink_idx);

   return p_path;
}

/* ----------------------------------------------------------------------------
** Derive file name for socket from device path
*/
char * vbi_proxy_msg_get_socket_name( const char * p_dev_name )
{
   char * p_real_dev_name;
   char * p_sock_path;
   char * po;
   const char * ps;
   char   c;
   int    name_len;

   if (p_dev_name != NULL)
   {
      p_real_dev_name = vbi_proxy_msg_resolve_symlinks(p_dev_name);

      name_len = strlen(SRV_CLNT_SOCK_BASE_PATH) + strlen(p_real_dev_name) + 1;
      p_sock_path = malloc(name_len);
      if (p_sock_path != NULL)
      {
         strcpy(p_sock_path, SRV_CLNT_SOCK_BASE_PATH);
         po = p_sock_path + strlen(SRV_CLNT_SOCK_BASE_PATH);
         ps = p_real_dev_name;
         while ((c = *(ps++)) != 0)
         {
            if (c == '/')
               *(po++) = '-';
            else
               *(po++) = c;
         }
         *po = 0;
      }

      free(p_real_dev_name);
   }
   else
      p_sock_path = NULL;

   return p_sock_path;
}

/* ----------------------------------------------------------------------------
** Attempt to connect to an already running server
*/
vbi_bool vbi_proxy_msg_check_connect( const char * p_sock_path )
{
   VBIPROXY_MSG_HEADER msgCloseInd;
   struct sockaddr_un saddr;
   int  fd;
   vbi_bool result = FALSE;

   fd = socket(AF_UNIX, SOCK_STREAM, 0);
   if (fd != -1)
   {
      saddr.sun_family = AF_UNIX;
      strcpy(saddr.sun_path, p_sock_path);
      if (connect(fd, (struct sockaddr *) &saddr, sizeof(saddr)) != -1)
      {
         msgCloseInd.len  = htons(sizeof(VBIPROXY_MSG_HEADER));
         msgCloseInd.type = MSG_TYPE_CLOSE_REQ;
         if (write(fd, &msgCloseInd, sizeof(msgCloseInd)) == sizeof(msgCloseInd))
         {
            result = TRUE;
         }
      }
      close(fd);
   }

   /* if no server is listening, remove the socket from the file system */
   if (result == FALSE)
      unlink(p_sock_path);

   return result;
}

/* ----------------------------------------------------------------------------
** Open client connection
** - since the socket is made non-blocking, the result of the connect is not
**   yet available when the function finishes; the caller has to wait for
**   completion with select() and then query the socket error status
*/
int vbi_proxy_msg_connect_to_server( vbi_bool use_tcp_ip, const char * pSrvHost, const char * pSrvPort, char ** ppErrorText )
{
   struct addrinfo    ask, *res;
   int  sock_fd;
   int  rc;

   rc = 0;
   res = NULL;
   sock_fd = -1;
   memset(&ask, 0, sizeof(ask));
   ask.ai_flags = 0;
   ask.ai_socktype = SOCK_STREAM;

   #ifdef PF_INET6
   if (use_tcp_ip)
   {  /* try IP-v6: not supported everywhere yet, so errors must be silently ignored */
      ask.ai_family = PF_INET6;
      rc = getaddrinfo(pSrvHost, pSrvPort, &ask, &res);
      if (rc == 0)
      {
         sock_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
         if (sock_fd == -1)
         {
            freeaddrinfo(res);
            res = NULL;
            /*dprintf2("socket (ipv6)\n"); */
         }
      }
      else
         dprintf2("getaddrinfo (ipv6): %s\n", gai_strerror(rc));
   }
   #endif

   if (sock_fd == -1)
   {
      if (use_tcp_ip)
      {
         ask.ai_family = PF_INET;
         rc = getaddrinfo(pSrvHost, pSrvPort, &ask, &res);
      }
      else
      {
         ask.ai_family = PF_UNIX;
         rc = vbi_proxy_msg_get_local_socket_addr(pSrvPort, &ask, &res);
      }
      if (rc == 0)
      {
         sock_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
         if (sock_fd == -1)
         {
            dprintf1("socket (ipv4): %s\n", strerror(errno));
            vbi_asprintf(ppErrorText, "%s: %s (%d)", _("Cannot create network socket"), strerror(errno), errno);
         }
      }
      else
      {
         dprintf1("getaddrinfo (ipv4): %s\n", gai_strerror(rc));
         vbi_asprintf(ppErrorText, "%s: %s", _("Invalid hostname or service/port"), gai_strerror(rc));
      }
   }

   if (sock_fd != -1)
   {
      if (fcntl(sock_fd, F_SETFL, O_NONBLOCK) == 0)
      {
         /* connect to the server socket */
         if ( (connect(sock_fd, res->ai_addr, res->ai_addrlen) == 0)
              || (errno == EINPROGRESS)
              )
         {
            /* all ok: result is in sock_fd */
         }
         else
         {
            dprintf1("connect: %s\n", strerror(errno));
            if (use_tcp_ip)
               vbi_asprintf(ppErrorText, "%s: %s (%d)", _("Server not running or not reachable: connect via TCP/IP failed"), strerror(errno), errno);
            else
               vbi_asprintf(ppErrorText, "%s %s: %s (%d)", _("Server not running: connect via socket failed"), pSrvPort, strerror(errno), errno);
            close(sock_fd);
            sock_fd = -1;
         }
      }
      else
      {
         dprintf1("fcntl (F_SETFL=O_NONBLOCK): %s\n", strerror(errno));
         vbi_asprintf(ppErrorText, "%s: %s (%d)", _("Failed to set socket non-blocking"), strerror(errno), errno);
         close(sock_fd);
         sock_fd = -1;
      }
   }

   if (res != NULL)
      freeaddrinfo(res);

   return sock_fd;
}

/* ----------------------------------------------------------------------------
** Check for the result of the connect syscall
** - UNIX: called when select() indicates writability
** - Win32: called when select() indicates writablility (successful connected)
**   or an exception (connect failed)
*/
vbi_bool vbi_proxy_msg_finish_connect( int sock_fd, char ** ppErrorText )
{
   vbi_bool result = FALSE;
   int  sockerr, sockerrlen;

   sockerrlen = sizeof(sockerr);
   if (getsockopt(sock_fd, SOL_SOCKET, SO_ERROR, (void *)&sockerr, &sockerrlen) == 0)
   {
      if (sockerr == 0)
      {  /* success -> send the first message of the startup protocol to the server */
         dprintf2("finish_connect: socket connect succeeded\n");
         result = TRUE;
      }
      else
      {  /* failed to establish a connection to the server */
         dprintf1("finish_connect: socket connect failed: %s\n", strerror(sockerr));
         vbi_asprintf(ppErrorText, "%s: %s (%d)", _("Connect failed"), strerror(sockerr), sockerr);
      }
   }
   else
   {
      dprintf1("finish_connect: getsockopt: %s\n", strerror(errno));
      vbi_asprintf(ppErrorText, "%s: %s (%d)", _("Failed to query socket connect result"), strerror(errno), errno);
   }

   return result;
}

#endif  /* ENABLE_PROXY */
