/*
 * (C) Tom Zoerner
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <inttypes.h>
#include "../src/libzvbi.h"

// proxy routines are not yet part of the public libzvbi interface
#include "../src/proxy-msg.h"

// ----------------------------------------------------------------------------
// Resolve parity on an array in-place
// - errors are ignored, the character just replaced by a blank
//
static const signed char parityTab[256] =
{
   //0x80, 0x01, 0x02, 0x83, 0x04, 0x85, 0x86, 0x07,
   //0x08, 0x89, 0x8a, 0x0b, 0x8c, 0x0d, 0x0e, 0x8f,
   //0x10, 0x91, 0x92, 0x13, 0x94, 0x15, 0x16, 0x97,
   //0x98, 0x19, 0x1a, 0x9b, 0x1c, 0x9d, 0x9e, 0x1f,
   0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
   0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
   0x20, 0xa1, 0xa2, 0x23, 0xa4, 0x25, 0x26, 0xa7,
   0xa8, 0x29, 0x2a, 0xab, 0x2c, 0xad, 0xae, 0x2f,
   0xb0, 0x31, 0x32, 0xb3, 0x34, 0xb5, 0xb6, 0x37,
   0x38, 0xb9, 0xba, 0x3b, 0xbc, 0x3d, 0x3e, 0xbf,
   0x40, 0xc1, 0xc2, 0x43, 0xc4, 0x45, 0x46, 0xc7,
   0xc8, 0x49, 0x4a, 0xcb, 0x4c, 0xcd, 0xce, 0x4f,
   0xd0, 0x51, 0x52, 0xd3, 0x54, 0xd5, 0xd6, 0x57,
   0x58, 0xd9, 0xda, 0x5b, 0xdc, 0x5d, 0x5e, 0xdf,
   0xe0, 0x61, 0x62, 0xe3, 0x64, 0xe5, 0xe6, 0x67,
   0x68, 0xe9, 0xea, 0x6b, 0xec, 0x6d, 0x6e, 0xef,
   0x70, 0xf1, 0xf2, 0x73, 0xf4, 0x75, 0x76, 0xf7,
   0xf8, 0x79, 0x7a, 0xfb, 0x7c, 0xfd, 0xfe, 0x7f,

   //0x00, 0x81, 0x82, 0x03, 0x84, 0x05, 0x06, 0x87,
   //0x88, 0x09, 0x0a, 0x8b, 0x0c, 0x8d, 0x8e, 0x0f,
   //0x90, 0x11, 0x12, 0x93, 0x14, 0x95, 0x96, 0x17,
   //0x18, 0x99, 0x9a, 0x1b, 0x9c, 0x1d, 0x1e, 0x9f,
   0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
   0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,

   0xa0, 0x21, 0x22, 0xa3, 0x24, 0xa5, 0xa6, 0x27,
   0x28, 0xa9, 0xaa, 0x2b, 0xac, 0x2d, 0x2e, 0xaf,
   0x30, 0xb1, 0xb2, 0x33, 0xb4, 0x35, 0x36, 0xb7,
   0xb8, 0x39, 0x3a, 0xbb, 0x3c, 0xbd, 0xbe, 0x3f,
   0xc0, 0x41, 0x42, 0xc3, 0x44, 0xc5, 0xc6, 0x47,
   0x48, 0xc9, 0xca, 0x4b, 0xcc, 0x4d, 0x4e, 0xcf,
   0x50, 0xd1, 0xd2, 0x53, 0xd4, 0x55, 0x56, 0xd7,
   0xd8, 0x59, 0x5a, 0xdb, 0x5c, 0xdd, 0xde, 0x5f,
   0x60, 0xe1, 0xe2, 0x63, 0xe4, 0x65, 0x66, 0xe7,
   0xe8, 0x69, 0x6a, 0xeb, 0x6c, 0xed, 0xee, 0x6f,
   0xf0, 0x71, 0x72, 0xf3, 0x74, 0xf5, 0xf6, 0x77,
   0x78, 0xf9, 0xfa, 0x7b, 0xfc, 0x7d, 0x7e, 0xff,
};
static int UnHamParityArray( const unsigned char *pin, char *pout, int byteCount )
{
   int errCount;
   signed char c1;

   errCount = 0;
   for (; byteCount > 0; byteCount--)
   {
      c1 = (char)parityTab[*(pin++)];
      if (c1 > 0)
      {
         *(pout++) = c1;
      }
      else
      {
         *(pout++) = 0xA0;  // Latin-1 space character
         errCount += 1;
      }
   }

   return errCount;
}

// ---------------------------------------------------------------------------
// Decode a VPS data line
// - bit fields are defined in "VPS Richtlinie 8R2" from August 1995
// - called by the VBI decoder for every received VPS line
//
static void TtxDecode_AddVpsData( const unsigned char * data )
{
   uint mday, month, hour, minute;
   uint cni;

#define VPSOFF -3

   cni = ((data[VPSOFF+13] & 0x3) << 10) | ((data[VPSOFF+14] & 0xc0) << 2) |
         ((data[VPSOFF+11] & 0xc0)) | (data[VPSOFF+14] & 0x3f);

   if ((cni != 0) && (cni != 0xfff))
   {
      if (cni == 0xDC3)
      {  // special case: "ARD/ZDF Gemeinsames Vormittagsprogramm"
         cni = (data[VPSOFF+5] & 0x20) ? 0xDC1 : 0xDC2;
      }

      // decode VPS PIL
      mday   =  (data[VPSOFF+11] & 0x3e) >> 1;
      month  = ((data[VPSOFF+12] & 0xe0) >> 5) | ((data[VPSOFF+11] & 1) << 3);
      hour   =  (data[VPSOFF+12] & 0x1f);
      minute =  (data[VPSOFF+13] >> 2);

      printf("VPS %d.%d. %02d:%02d CNI 0x%04X\n", mday, month, hour, minute, cni);
   }
}

extern vbi_capture *
vbi_capture_proxy_new(const char *dev_name, int scanning,
                      unsigned int *services, int strict,
                      char **errorstr, vbi_bool trace);

#define DEVICE_PATH   "/dev/vbi0"
#define BUFFER_COUNT  5
#define USAGE_STR     "Usage: %s [-trace] [-dev path] [-api v4l|v4l2|proxy] {ttx|vps|wss} ...\n"

typedef enum
{
   TEST_API_V4L,
   TEST_API_V4L2,
   TEST_API_PROXY,
} PROXY_TEST_API;

static char * p_dev_name = DEVICE_PATH;
static PROXY_TEST_API opt_api = TEST_API_PROXY;

int main ( int argc, char ** argv )
{
   vbi_capture      *  pVbiCapt;
   vbi_raw_decoder  *  pVbiPar;
   char             *  pErr;
   int                 arg_idx;
   unsigned int        services;
   int                 trace;

   trace = FALSE;
   services = 0;
   arg_idx = 1;

   while (arg_idx < argc)
   {
      if ( (strcasecmp(argv[arg_idx], "ttx") == 0) ||
           (strcasecmp(argv[arg_idx], "teletext") == 0) )
      {
         services |= VBI_SLICED_TELETEXT_B;
         arg_idx += 1;
      }
      else if (strcasecmp(argv[arg_idx], "vps") == 0)
      {
         services |= VBI_SLICED_VPS;
         arg_idx += 1;
      }
      else if (strcasecmp(argv[arg_idx], "wss") == 0)
      {
         services |= VBI_SLICED_WSS_625 | VBI_SLICED_WSS_CPR1204;
         arg_idx += 1;
      }
      else if (strcasecmp(argv[arg_idx], "-dev") == 0)
      {
         if (arg_idx + 1 >= argc)
         {
            fprintf(stderr, "Missing device name after -dev\n" USAGE_STR, argv[0]);
            exit(1);
         }
         p_dev_name = argv[arg_idx + 1];
         arg_idx += 2;
      }
      else if (strcasecmp(argv[arg_idx], "-api") == 0)
      {
         if (arg_idx + 1 >= argc)
         {
            fprintf(stderr, "Missing API type after -api\n" USAGE_STR, argv[0]);
            exit(1);
         }
         if ( (strcasecmp(argv[arg_idx + 1], "v4l") == 0) ||
              (strcasecmp(argv[arg_idx + 1], "v4l1") == 0) )
            opt_api = TEST_API_V4L;
         else if (strcasecmp(argv[arg_idx + 1], "v4l2") == 0)
            opt_api = TEST_API_V4L2;
         else if (strcasecmp(argv[arg_idx + 1], "proxy") == 0)
            opt_api = TEST_API_PROXY;
         else
         {
            fprintf(stderr, "Invalid API type '%s'\n" USAGE_STR, argv[arg_idx + 1], argv[0]);
         exit(1);
         }
         arg_idx += 2;
      }
      else if (strcasecmp(argv[arg_idx], "-trace") == 0)
      {
         trace = TRUE;
         arg_idx += 1;
      }
      else if (strcasecmp(argv[arg_idx], "-help") == 0)
      {
         fprintf(stderr, USAGE_STR, argv[0]);
         exit(0);
      }
      else
      {
         fprintf(stderr, "Unknown argument '%s'\n" USAGE_STR, argv[arg_idx], argv[0]);
         exit(1);
      }
   }
   if (services == 0)
   {
      fprintf(stderr, "Must specify at least one argument\n" USAGE_STR, argv[0]);
      exit(1);
   }

   pVbiCapt = NULL;
   if (opt_api == TEST_API_V4L2)
      pVbiCapt = vbi_capture_v4l2_new(p_dev_name, BUFFER_COUNT, &services, 0, &pErr, trace);
   if (opt_api == TEST_API_V4L)
      pVbiCapt = vbi_capture_v4l_new(p_dev_name, BUFFER_COUNT, &services, 0, &pErr, trace);
   if (opt_api == TEST_API_PROXY)
      pVbiCapt = vbi_capture_proxy_new(p_dev_name, BUFFER_COUNT, &services, 0, &pErr, trace);

   if (pVbiCapt != NULL)
   {
      pVbiPar = vbi_capture_parameters(pVbiCapt);
      if (pVbiPar != NULL)
      {
         vbi_sliced * pVbiData;
         uint         lineCount;
         uint         lastLineCount;
         uint         line;
         double       timestamp;
         struct timeval timeout;
         int  res;

         lineCount = pVbiPar->count[0] + pVbiPar->count[1];
         fprintf(stderr, "Allocating buffer for %d lines\n", lineCount);
         pVbiData = malloc(lineCount * sizeof(*pVbiData));
         lastLineCount = 0;

         while(1)
         {
            timeout.tv_sec  = 5;
            timeout.tv_usec = 0;

            res = vbi_capture_read_sliced(pVbiCapt, pVbiData, &lineCount, &timestamp, &timeout);
            if (res == -1)
            {
               perror("VBI read");
               break;
            }
            if (lastLineCount != lineCount)
            {
               fprintf(stderr, "%d lines\n", lineCount);
               lastLineCount = lineCount;
            }

            for (line=0; line < lineCount; line++)
            {  // dump all TTX packets, even non-EPG ones
               #if 1
               if ((pVbiData[line].id & VBI_SLICED_TELETEXT_B) != 0)
               {
                  char tmparr[46];
                  UnHamParityArray(pVbiData[line].data+2, tmparr, 40);
                  tmparr[40] = 0;
                  printf("pkg %2d id=%d line %d: '%s'\n", line, pVbiData[line].id, pVbiData[line].line, tmparr);
               }
               else if (pVbiData[line].id == VBI_SLICED_VPS)
               {
                  TtxDecode_AddVpsData(pVbiData[line].data);
               }
               else
               #endif
               if (pVbiData[line].id == VBI_SLICED_WSS_625)
               {
                  printf("WSS 0x%02X%02X%02X\n", pVbiData[line].data[0], pVbiData[line].data[1], pVbiData[line].data[2]);
               }
            }
         }
      }

      vbi_capture_delete(pVbiCapt);
   }
   else
   {
      if (pErr != NULL)
      {
         fprintf(stderr, "libzvbi error: %s\n", pErr);
         free(pErr);
      }
      else
         fprintf(stderr, "error starting acquisition\n");
   }
   exit(0);
   return(0);
}

