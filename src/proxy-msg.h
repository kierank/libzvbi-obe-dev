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
 *  $Id: proxy-msg.h,v 1.3 2003/05/03 12:04:52 tomzo Exp $
 *
 *  $Log: proxy-msg.h,v $
 *  Revision 1.3  2003/05/03 12:04:52  tomzo
 *  - added new macro VBIPROXY_ENDIAN_MISMATCH to replace use of swap32()
 *  - added declaration for new func vbi_proxy_msg_set_debug_level()
 *  - fixed copyright headers, added description to file headers
 *
 */

#ifndef __VBIPROXYMSG_H
#define __VBIPROXYMSG_H

#include <sys/syslog.h>

/* ----------------------------------------------------------------------------
** Declaration of message IDs and the common header struct
*/
typedef enum
{
   MSG_TYPE_CONNECT_REQ,
   MSG_TYPE_CONNECT_CNF,
   MSG_TYPE_CONNECT_REJ,
   MSG_TYPE_CLOSE_REQ,
   MSG_TYPE_DATA_IND,
   /*
   MSG_TYPE_SERVICE_REQ,
   MSG_TYPE_SERVICE_CNF,
   MSG_TYPE_CHN_CHANGE_REQ,       // also resets VBI buffer
   MSG_TYPE_CHN_CHANGE_IND,
   MSG_TYPE_CHN_CHANGE_CNF,
   MSG_TYPE_CHN_LOCK_REQ,
   MSG_TYPE_CHN_LOCK_CNF,
   MSG_TYPE_CHANTAB_REQ,
   MSG_TYPE_CHANTAB_CNF,
   */
} VBIPROXY_MSG_TYPE;

#define VBIPROXY_MSG_MAXSIZE  65536

typedef struct
{
        uint32_t                len;
        uint32_t                type;
        uint32_t                reserved;
} VBIPROXY_MSG_HEADER;

#define VBIPROXY_MAGIC_STR                 "LIBZVBI VBIPROXY"
#define VBIPROXY_MAGIC_LEN                 16
#define VBIPROXY_VERSION                   0x00000000
#define VBIPROXY_COMPAT_VERSION            0x00000000
#define VBIPROXY_ENDIAN_MAGIC              0x11223344
#define VBIPROXY_ENDIAN_MISMATCH           0x44332211
#define VBIPROXY_CLIENT_NAME_MAX_LENGTH    64
#define VBIPROXY_DEV_NAME_MAX_LENGTH      128
#define VBIPROXY_ERROR_STR_MAX_LENGTH     128

typedef struct
{
        uint8_t                 protocol_magic[VBIPROXY_MAGIC_LEN];
        uint32_t                protocol_compat_version;
        uint32_t                protocol_version;
        uint32_t                endian_magic;
} VBIPROXY_MAGICS;

typedef struct
{
        VBIPROXY_MAGICS         magics;
        uint8_t                 client_name[VBIPROXY_CLIENT_NAME_MAX_LENGTH];

        /* service request */
        uint32_t                scanning;
        uint32_t                services;
        uint8_t                 strict;
        uint8_t                 buffer_count;
} VBIPROXY_CONNECT_REQ;

typedef struct
{
        VBIPROXY_MAGICS         magics;
        vbi_raw_decoder         dec;            /* req. e.g. for VBI line counts */
} VBIPROXY_CONNECT_CNF;

typedef struct
{
        VBIPROXY_MAGICS         magics;
        uint8_t                 errorstr[VBIPROXY_ERROR_STR_MAX_LENGTH];
} VBIPROXY_CONNECT_REJ;

typedef struct
{
        double                  timestamp;
        uint32_t                line_count;
        vbi_sliced              sliced[1];
} VBIPROXY_DATA_IND;

#define VBIPROXY_DATA_IND_SIZE(C)  (sizeof(VBIPROXY_DATA_IND) + (sizeof(vbi_sliced) * ((C) - 1)))

typedef union
{
        VBIPROXY_CONNECT_REQ    connect_req;
        VBIPROXY_CONNECT_CNF    connect_cnf;
        VBIPROXY_CONNECT_REJ    connect_rej;
        VBIPROXY_DATA_IND       data_ind;
} VBIPROXY_MSG_BODY;

/* ----------------------------------------------------------------------------
** Declaration of the IO state struct
*/
typedef struct
{
        int                     sock_fd;        /* socket file handle or -1 if closed */
        time_t                  lastIoTime;     /* timestamp of last i/o (for timeouts) */

        uint32_t                writeLen;       /* number of bytes in write buffer, including header */
        uint32_t                writeOff;       /* number of already written bytes, including header */
        VBIPROXY_MSG_HEADER     writeHeader;    /* header to be written */
        void                    * pWriteBuf;    /* data to be written */
        vbi_bool                freeWriteBuf;   /* TRUE if the buffer shall be freed by the I/O handler */

        vbi_bool                waitRead;       /* TRUE while length of incoming msg is not completely read */
        uint32_t                readLen;        /* length of incoming message (including itself) */
        uint32_t                readOff;        /* number of already read bytes */
        uint8_t                 *pReadBuf;      /* msg buffer; allocated after length is read */
        VBIPROXY_MSG_HEADER     readHeader;     /* received message header */
} VBIPROXY_MSG_STATE;

/* ----------------------------------------------------------------------------
** Declaration of the service interface functions
*/
void     vbi_proxy_msg_logger( int level, int clnt_fd, int errCode, const char * pText, ... );
void     vbi_proxy_msg_set_logging( vbi_bool do_logtty, int sysloglev,
                                    int fileloglev, const char * pLogfileName );
void     vbi_proxy_msg_set_debug_level( int level );

vbi_bool vbi_proxy_msg_is_idle( VBIPROXY_MSG_STATE * pIO );
vbi_bool vbi_proxy_msg_check_timeout( VBIPROXY_MSG_STATE * pIO, time_t now );
vbi_bool vbi_proxy_msg_handle_io( VBIPROXY_MSG_STATE * pIO, vbi_bool * pBlocked, vbi_bool closeOnZeroRead );
void     vbi_proxy_msg_close_io( VBIPROXY_MSG_STATE * pIO );
void     vbi_proxy_msg_fill_magics( VBIPROXY_MAGICS * p_magic );
void     vbi_proxy_msg_write( VBIPROXY_MSG_STATE * pIO, VBIPROXY_MSG_TYPE type, uint32_t msgLen, void * pMsg, vbi_bool freeBuf );
vbi_bool vbi_proxy_msg_write_queue( VBIPROXY_MSG_STATE * p_io, vbi_bool * p_blocked,
                                    unsigned int services, int max_lines,
                                    vbi_sliced * p_lines, unsigned int line_count,
                                    double timestamp );

int      vbi_proxy_msg_listen_socket( vbi_bool is_tcp_ip, const char * listen_ip, const char * listen_port );
void     vbi_proxy_msg_stop_listen( vbi_bool is_tcp_ip, int sock_fd, char * pSrvPort );
int      vbi_proxy_msg_accept_connection( int listen_fd );
char   * vbi_proxy_msg_get_socket_name( const char * p_dev_name );
vbi_bool vbi_proxy_msg_check_connect( const char * p_sock_path );
int      vbi_proxy_msg_connect_to_server( vbi_bool use_tcp_ip, const char * pSrvHost, const char * pSrvPort, char ** ppErrorText );
vbi_bool vbi_proxy_msg_finish_connect( int sock_fd, char ** ppErrorText );

#endif  /* __VBIPROXYMSG_H */
