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
 *  $Id: proxy-msg.h,v 1.6 2003/06/07 09:43:08 tomzo Exp $
 *
 *  $Log: proxy-msg.h,v $
 *  Revision 1.6  2003/06/07 09:43:08  tomzo
 *  - added new message types MSG_TYPE_DAEMON_PID_REQ,CNF
 *  - added new struct VBIPROXY_MSG: holds message header and body structs
 *
 *  Revision 1.5  2003/06/01 19:36:23  tomzo
 *  Implemented server-side TV channel switching
 *  - implemented messages MSG_TYPE_CHN_CHANGE_REQ/CNF/REJ; IND is still TODO
 *  - removed obsolete PROFILE messages: profile is included with CHN_CHANGE_REQ
 *  Also: added VBI API identifier and device path to CONNECT_CNF (for future use)
 *
 *  Revision 1.4  2003/05/24 12:19:29  tomzo
 *  - added VBIPROXY_SERVICE_REQ/_CNF/_REJ messages
 *  - prepared channel change request and profile request
 *  - renamed MSG_TYPE_DATA_IND into _SLICED_IND in preparation for raw data
 *
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
   MSG_TYPE_SLICED_IND,

   MSG_TYPE_SERVICE_REQ,
   MSG_TYPE_SERVICE_REJ,
   MSG_TYPE_SERVICE_CNF,

   MSG_TYPE_CHN_CHANGE_REQ,
   MSG_TYPE_CHN_CHANGE_CNF,
   MSG_TYPE_CHN_CHANGE_REJ,
   MSG_TYPE_CHN_CHANGE_IND,

   MSG_TYPE_DAEMON_PID_REQ,
   MSG_TYPE_DAEMON_PID_CNF,

   MSG_TYPE_COUNT

} VBIPROXY_MSG_TYPE;

typedef enum
{
        VBI_API_V4L1,
        VBI_API_V4L2,
        VBI_API_BKTR,
} VBI_API_REV;

typedef struct
{
        uint32_t                len;
        uint32_t                type;
} VBIPROXY_MSG_HEADER;

typedef struct
{
        uint8_t                 is_valid;       /* boolean: ignore struct unless TRUE */
        uint8_t                 chn_prio;       /* driver prio: record,interact,background */
        uint8_t                 sub_prio;       /* background prio: VPS/PDC, initial load, update, check */
        uint8_t                 allow_suspend;  /* boolean: suspend allowed for checks by other aps */
        time_t                  duration;       /* min. continuous use of that channel */
} VBIPROXY_CHN_PROFILE;

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

        uint32_t                scanning;
        uint8_t                 buffer_count;

        uint32_t                services;
        uint8_t                 strict;

        uint32_t                reserved[32];  /* set to zero */
} VBIPROXY_CONNECT_REQ;

typedef struct
{
        VBIPROXY_MAGICS         magics;
        uint8_t                 dev_vbi_name[VBIPROXY_DEV_NAME_MAX_LENGTH];
        uint32_t                vbi_api_revision;
        vbi_raw_decoder         dec;            /* req. e.g. for VBI line counts */
        uint32_t                reserved[32];   /* set to zero */
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
} VBIPROXY_SLICED_IND;

#define VBIPROXY_SLICED_IND_SIZE(C)  (sizeof(VBIPROXY_SLICED_IND) + (sizeof(vbi_sliced) * ((C) - 1)))

typedef struct
{
        uint8_t                 reset;
        uint8_t                 commit;
        uint8_t                 strict;
        uint32_t                services;
} VBIPROXY_SERVICE_REQ;

typedef struct
{
        vbi_raw_decoder         dec;            /* req. e.g. for VBI line counts */
} VBIPROXY_SERVICE_CNF;

typedef struct
{
        uint8_t                 errorstr[VBIPROXY_ERROR_STR_MAX_LENGTH];
} VBIPROXY_SERVICE_REJ;

typedef struct
{
        uint32_t                chn_flags;
        vbi_channel_desc        chn_desc;
        VBIPROXY_CHN_PROFILE    chn_profile;
        uint32_t                serial;
} VBIPROXY_CHN_CHANGE_REQ;

typedef struct
{
        uint32_t                serial;         /* which request this refers to */
        uint32_t                scanning;
        uint8_t                 has_tuner;
} VBIPROXY_CHN_CHANGE_CNF;

typedef struct
{
        uint32_t                serial;         /* which request this refers to */
        uint32_t                dev_errno;
        uint8_t                 errorstr[VBIPROXY_ERROR_STR_MAX_LENGTH];
} VBIPROXY_CHN_CHANGE_REJ;

typedef struct
{                                               /* note: sent both by client and daemon */
        vbi_channel_desc        chn_desc;
        uint32_t                scanning;
} VBIPROXY_CHN_CHANGE_IND;

typedef struct
{
        VBIPROXY_MAGICS         magics;
} VBIPROXY_DAEMON_PID_REQ;

typedef struct
{
        VBIPROXY_MAGICS         magics;
        uint32_t                pid;
} VBIPROXY_DAEMON_PID_CNF;

typedef union
{
        VBIPROXY_CONNECT_REQ            connect_req;
        VBIPROXY_CONNECT_CNF            connect_cnf;
        VBIPROXY_CONNECT_REJ            connect_rej;

        VBIPROXY_SLICED_IND             sliced_ind;

        VBIPROXY_SERVICE_REQ            service_req;
        VBIPROXY_SERVICE_CNF            service_cnf;
        VBIPROXY_SERVICE_REJ            service_rej;

        VBIPROXY_CHN_CHANGE_REQ         chn_change_req;
        VBIPROXY_CHN_CHANGE_CNF         chn_change_cnf;
        VBIPROXY_CHN_CHANGE_REJ         chn_change_rej;
        VBIPROXY_CHN_CHANGE_IND         chn_change_ind;

        VBIPROXY_DAEMON_PID_REQ         daemon_pid_req;
        VBIPROXY_DAEMON_PID_CNF         daemon_pid_cnf;

} VBIPROXY_MSG_BODY;

typedef struct
{
        VBIPROXY_MSG_HEADER             head;
        VBIPROXY_MSG_BODY               body;
} VBIPROXY_MSG;

/* ----------------------------------------------------------------------------
** Declaration of the IO state struct
*/
typedef struct
{
        int                     sock_fd;        /* socket file handle or -1 if closed */
        time_t                  lastIoTime;     /* timestamp of last i/o (for timeouts) */

        uint32_t                writeLen;       /* number of bytes in write buffer, including header */
        uint32_t                writeOff;       /* number of already written bytes, including header */
        VBIPROXY_MSG          * pWriteBuf;      /* data to be written */
        vbi_bool                freeWriteBuf;   /* TRUE if the buffer shall be freed by the I/O handler */

        vbi_bool                waitRead;       /* TRUE while length of incoming msg is not completely read */
        uint32_t                readLen;        /* length of incoming message (including itself) */
        uint32_t                readOff;        /* number of already read bytes */
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
vbi_bool vbi_proxy_msg_handle_io( VBIPROXY_MSG_STATE * pIO,
                                  vbi_bool * pBlocked, vbi_bool closeOnZeroRead,
                                  VBIPROXY_MSG * pReadBuf, int max_read_len );
void     vbi_proxy_msg_close_io( VBIPROXY_MSG_STATE * pIO );
void     vbi_proxy_msg_fill_magics( VBIPROXY_MAGICS * p_magic );
void     vbi_proxy_msg_write( VBIPROXY_MSG_STATE * p_io, VBIPROXY_MSG_TYPE type,
                              uint32_t msgLen, VBIPROXY_MSG * pMsg, vbi_bool freeBuf );
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
