
/* ====================================================================
 *
 * MOD_GZIP.C - Version 1.3.19.1a
 *
 * This program was developed and is currently maintained by
 * Remote Communications, Inc.
 * Home page: http://www.RemoteCommunications.com
 *
 * Original author: Kevin Kiley, CTO, Remote Communications, Inc.
 * Email: Kiley@RemoteCommunications.com
 *
 * As of this writing there is an online support forum which
 * anyone may join by following the instructions found at...
 * http://lists.over.net/mailman/listinfo/mod_gzip
 *
 * ====================================================================
 */

/* APACHE LICENSE: START
 *
 * Portions of this software are covered under the following license
 * which, as it states, must remain included in this source code
 * module and may not be altered in any way.
 */

/* ====================================================================
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 2000 The Apache Software Foundation.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The end-user documentation included with the redistribution,
 *    if any, must include the following acknowledgment:
 *       "This product includes software developed by the
 *        Apache Software Foundation (http://www.apache.org/)."
 *    Alternately, this acknowledgment may appear in the software itself,
 *    if and wherever such third-party acknowledgments normally appear.
 *
 * 4. The names "Apache" and "Apache Software Foundation" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache",
 *    nor may "Apache" appear in their name, without prior written
 *    permission of the Apache Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE APACHE SOFTWARE FOUNDATION OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 *
 * Portions of this software are based upon public domain software
 * originally written at the National Center for Supercomputing Applications,
 * University of Illinois, Urbana-Champaign.
 */

/* APACHE LICENSE: END */

#define CORE_PRIVATE

#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_log.h"
#include "http_main.h"
#include "http_protocol.h"
#include "http_request.h"
#include "util_script.h"

extern API_VAR_EXPORT char ap_server_root[ MAX_STRING_LEN ];

char mod_gzip_version[] = "1.3.19.1a";

#define MOD_GZIP_VERSION_INFO_STRING "mod_gzip/1.3.19.1a"

/*
 * Declare the NAME by which this module will be known.
 * This is the NAME that will be used in LoadModule command(s).
 */

extern module MODULE_VAR_EXPORT gzip_module;

/*
 * Turn MOD_GZIP_USES_APACHE_LOGS switch ON to include the
 * code that can update Apache logs with compression information.
 */

#define MOD_GZIP_USES_APACHE_LOGS

/*
 * Turn MOD_GZIP_DEBUG1 switch ON to include debug code.
 * This is normally OFF by default and should only be
 * used for diagnosing problems. The log output is
 * VERY detailed and the log files will be HUGE.
 */

/*
#define MOD_GZIP_DEBUG1
*/

/*
 * Turn MOD_GZIP_DEBUG1_VERBOSE1 switch ON to include
 * some VERY 'verbose' debug code such as request record
 * dumps during transactions and hex dumps of data.
 * This is normally OFF by default. MOD_GZIP_DEBUG1
 * switch must also be 'on' for this to have any effect.
 */

#ifdef  MOD_GZIP_DEBUG1
#define MOD_GZIP_DEBUG1_VERBOSE1
#endif

/*
 * Turn this 'define' on to send all log output to
 * Apache error_log instead of a flat file. "LogLevel debug"
 * must be set in httpd.conf for log output to appear in error_log.
 */

/*
#define MOD_GZIP_LOG_IS_APACHE_LOG
*/

/*
 * Other globals...
 */

#ifndef MOD_GZIP_MAX_PATH_LEN
#define MOD_GZIP_MAX_PATH_LEN 512
#endif

#ifdef WIN32
char mod_gzip_dirsep[]="\\"; 
#else 
char mod_gzip_dirsep[]="/";  
#endif 

#define MOD_GZIP_IMAP_MAXNAMES   256
#define MOD_GZIP_IMAP_MAXNAMELEN 90

#define MOD_GZIP_IMAP_ISNONE      0
#define MOD_GZIP_IMAP_ISMIME      1
#define MOD_GZIP_IMAP_ISHANDLER   2
#define MOD_GZIP_IMAP_ISFILE      3
#define MOD_GZIP_IMAP_ISURI       4
#define MOD_GZIP_IMAP_ISREQHEADER 5
#define MOD_GZIP_IMAP_ISRSPHEADER 6
#define MOD_GZIP_IMAP_ISPORT      7
#define MOD_GZIP_IMAP_ISADDRESS   8

#define MOD_GZIP_IMAP_STATIC1    9001
#define MOD_GZIP_IMAP_DYNAMIC1   9002
#define MOD_GZIP_IMAP_DYNAMIC2   9003
#define MOD_GZIP_IMAP_DECLINED1  9004

#define MOD_GZIP_REQUEST         9005
#define MOD_GZIP_RESPONSE        9006

typedef struct {
    int      include;   
    int      type;      
    int      action;    
    int      direction; 
    unsigned port;      
    int      len1;      
    regex_t *pregex;    

    char name[ MOD_GZIP_IMAP_MAXNAMELEN + 2 ];
    int  namelen; 
} mod_gzip_imap;

int mod_gzip_imap_size = (int) sizeof( mod_gzip_imap );

#define MOD_GZIP_CONFIG_MODE_SERVER      1
#define MOD_GZIP_CONFIG_MODE_DIRECTORY   2
#define MOD_GZIP_CONFIG_MODE_COMBO       3

typedef struct {

    int  cmode;

    char *loc;                   

    int  is_on;                  
    int  is_on_set;

    int  keep_workfiles;         
    int  keep_workfiles_set;

    int  dechunk;                
    int  dechunk_set;

    int  add_header_count;       
    int  add_header_count_set;

    int  min_http;               
    int  min_http_set;

    long minimum_file_size;      
    int  minimum_file_size_set;

    long maximum_file_size;      
    int  maximum_file_size_set;

    long maximum_inmem_size;     
    int  maximum_inmem_size_set;

    char temp_dir[256]; /* Length is safety-checked during startup */
    int  temp_dir_set;

    int imap_total_entries;
    int imap_total_ismime;
    int imap_total_isfile;
    int imap_total_isuri;
    int imap_total_ishandler;
    int imap_total_isreqheader;
    int imap_total_isrspheader;

    mod_gzip_imap imap[ MOD_GZIP_IMAP_MAXNAMES + 1 ];

    #define MOD_GZIP_COMMAND_VERSION_USED
    #ifdef  MOD_GZIP_COMMAND_VERSION_USED
    #define MOD_GZIP_COMMAND_VERSION 8001
    #define MOD_GZIP_COMMAND_VERSION_MAXLEN 128
    char command_version[ MOD_GZIP_COMMAND_VERSION_MAXLEN + 1 ];
    int  command_version_set;
    #endif

    #define MOD_GZIP_CAN_NEGOTIATE
    #ifdef  MOD_GZIP_CAN_NEGOTIATE
    int can_negotiate;
    int can_negotiate_set;
    #endif

} mod_gzip_conf;

long mod_gzip_iusn = 0; 

int mod_gzip_strncmp( char *s1, char *s2, int len1 );
int mod_gzip_strnicmp( char *s1, char *s2, int len1 );
int mod_gzip_strcpy( char *s1, char *s2 );
int mod_gzip_strcat( char *s1, char *s2 );
int mod_gzip_strlen( char *s1 );
int mod_gzip_stringcontains( char *source, char *substring );
int mod_gzip_strendswith( char *s1, char *s2, int ignorcase );

#ifdef MOD_GZIP_CAN_NEGOTIATE
int mod_gzip_check_for_precompressed_file( request_rec *r );
#endif

int mod_gzip_create_unique_filename(
char *prefix,
char *target,
int   targetmaxlen
);

int mod_gzip_delete_file(
request_rec   *r,
char          *filename
);

int mod_gzip_flush_and_update_counts(
request_rec   *r,
mod_gzip_conf *dconf,
long           total_header_bytes_sent,
long           total_body_bytes_sent
);

long mod_gzip_sendfile1(
request_rec *r,
char        *input_filename,
FILE        *ifh_passed,
long         starting_offset
);

int mod_gzip_sendfile2(
request_rec   *r,
mod_gzip_conf *dconf,
char          *input_filename
);

int mod_gzip_dyn1_getfdo1(
request_rec *r,
char        *filename
);

int mod_gzip_redir1_handler(
request_rec   *r,
mod_gzip_conf *dconf
);

long mod_gzip_send(
char        *buf,
long         buflen,
request_rec *r
);

int mod_gzip_encode_and_transmit(
request_rec   *r,
mod_gzip_conf *dconf,
char          *source,
int            source_is_a_file,
long           input_size,
int            nodecline,
long           header_length,
char          *result_prefix_string
);

typedef struct _GZP_CONTROL {

    int   decompress;

    int   input_ismem;          
    char *input_ismem_ibuf;     
    long  input_ismem_ibuflen;  

    char  input_filename[ MOD_GZIP_MAX_PATH_LEN + 2 ]; 

    long  input_offset;         

    int   output_ismem;         
    char *output_ismem_obuf;    
    long  output_ismem_obuflen; 

    char  output_filename[ MOD_GZIP_MAX_PATH_LEN + 2 ]; 

    int   result_code; 
    long  bytes_out;   

} GZP_CONTROL;

int gzp_main( request_rec *, GZP_CONTROL *gzp ); 

char mod_gzip_check_permissions[] =
"Make sure all named directories exist and have the correct permissions.";

#ifdef MOD_GZIP_DEBUG1

server_rec *mod_gzip_server_now = 0;

const char *npp( const char *s )
{
 /* NOTE: This 'Null Pointer Protection' call is really only */
 /* needed for the Solaris Operating System which seems to have */
 /* some kind of problem handling NULL pointers passed to certain */
 /* STDLIB calls. Sloaris will GP fault where all other OS's are OK. */

 if ( s ) return s;
 else     return "NULL";
}

#ifdef MOD_GZIP_LOG_IS_APACHE_LOG

void mod_gzip_printf( const char *fmt, ... )
{
 int   l;

 va_list ap;

 char log_line[2048]; 

 va_start( ap, fmt );

 l = vsprintf( log_line, fmt, ap );

 va_end(ap);

 ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, mod_gzip_server_now, log_line);

 return;
}

#else 

void mod_gzip_printf( const char *fmt, ... )
{
 int   l;
 char *p1;
 FILE *log;

 va_list ap;

 char logname[256];
 char log_line[4096];

 #ifdef WIN32
 long pid = GetCurrentProcessId();
 #else
 long pid = (long) getpid();
 #endif

 #ifdef WIN32
 sprintf( logname, "c:\\temp\\t%ld.log",(long)pid);
 #else
 sprintf( logname, "/tmp/t%ld.log",(long)pid);
 #endif

 log = fopen( logname,"a" );

 if ( !log ) 
   {
    return; 
   }

 va_start( ap, fmt );

 l = vsprintf(log_line, fmt, ap);

 p1=log_line;
 while((*p1!=0)&&(*p1!=13)&&(*p1!=10)) p1++;
 *p1=0;

 fprintf( log, "%s\n", log_line );

 fclose( log );

 va_end(ap); 

 return; 
}

#endif 

void mod_gzip_hexdump( char *buffer, int buflen )
{
 int i,o1,o2,o3;

 int len1;
 int len2;

 char ch1;
 char ch2;
 char s[40];
 char l1[129];
 char l2[129];
 char l3[300];

 long offset1=0L;

 o1=0;
 o2=0;
 o3=0;

 l1[0] = 0;
 l2[0] = 0;
 l3[0] = 0;

 offset1 = 0;

 for ( i=0; i<buflen; i++ )
    {
     ch1 = (char) *buffer++;

     #define DUMPIT_ASTERISK    42
     #define DUMPIT_LAPOSTROPHE 96
     #define DUMPIT_RAPOSTROPHE 39
     #define DUMPIT_PERIOD      46
     #define DUMPIT_CR          67
     #define DUMPIT_LF          76

     #ifdef MASK_ONLY_CERTAIN_CHARS
          if ( ch1 ==  0 ) ch2 = DUMPIT_PERIOD;
     else if ( ch1 == 13 ) ch2 = DUMPIT_CR;
     else if ( ch1 == 10 ) ch2 = DUMPIT_LF;
     else if ( ch1 ==  9 ) ch2 = DUMPIT_LAPOSTROPHE;
     else                  ch2 = ch1;
     #endif

     #define MASK_ALL_NON_PRINTABLE_CHARS
     #ifdef  MASK_ALL_NON_PRINTABLE_CHARS

          if ( ch1 == 13 ) ch2 = DUMPIT_CR;
     else if ( ch1 == 10 ) ch2 = DUMPIT_LF;
     else if ( ch1 <  32 ) ch2 = DUMPIT_PERIOD;
     else if ( ch1 >  126) ch2 = DUMPIT_LAPOSTROPHE;
     else if ( ch1 == 37 ) ch2 = DUMPIT_ASTERISK; 
     else if ( ch1 == 92 ) ch2 = DUMPIT_ASTERISK; 
     else                  ch2 = ch1;

     #endif

     l2[o2++] = ch2;

     sprintf( s, "%02X", ch1 );

     if ( mod_gzip_strlen(s) > 2 ) s[2]=0; 

     len1 = mod_gzip_strlen(s);
     len2 = mod_gzip_strlen(l1);

     if ( mod_gzip_strlen(l1) < (int)(sizeof(l1) - (len1+1)) )
       {
        strcat( l1, s   );
        strcat( l1, " " );
       }

     if ( o2 >= 16 )
       {
        l2[o2]=0;

        mod_gzip_printf( "%6lu| %-49.49s| %-16.16s |", offset1, l1, l2 );

        offset1 += o2;

        o1=0;
        o2=0;
        o3=0;

        l1[0] = 0;
        l2[0] = 0;
        l3[0] = 0;
       }
    }

 if ( o2 > 0  )
   {
    l2[o2]=0;

    mod_gzip_printf( "%6lu| %-49.49s| %-16.16s |", offset1, l1, l2 );

    offset1 += o2;

    o1 = o2 = o3 = 0;

    l1[0] = 0;
    l2[0] = 0;
    l3[0] = 0;
   }
}

int mod_gzip_log_comerror( request_rec *r, char *p, int error )
{
 int  i=0;      
 char b[3][90]; 

 b[0][0]=0; 
 b[1][0]=0; 
 b[2][0]=0; 

 #if defined(WIN32) || defined(NETWARE)

 if ( error == WSANOTINITIALISED )
   {
    sprintf(b[0],"%s * WSANOTINITIALISED",p);
    sprintf(b[1],"%s * A successful WSAStartup() must occur",p);
    sprintf(b[2],"%s * before using this WINSOCK API call.",p);
   }
 else if ( error == WSAENETDOWN )
   {
    sprintf(b[0],"%s * WSAENETDOWN",p);
    sprintf(b[1],"%s * The Windows Sockets implementation has detected",p);
    sprintf(b[2],"%s * that the network subsystem has failed.",p);
   }
 else if ( error == WSAENOTCONN )
   {
    sprintf(b[0],"%s * WSAENOTCONN",p);
    sprintf(b[1],"%s * The socket is not connected.",p);
   }
 else if ( error == WSAEINTR )
   {
    sprintf(b[0],"%s * WSAEINTR",p);
    sprintf(b[1],"%s * The (blocking) call was cancelled",p);
    sprintf(b[2],"%s * via WSACancelBlockingCall()",p);
   }
 else if ( error == WSAEINPROGRESS )
   {
    sprintf(b[0],"%s * WSAEINPROGRESS",p);
    sprintf(b[1],"%s * A blocking Windows Sockets operation",p);
    sprintf(b[2],"%s * is in progress.",p);
   }
 else if ( error == WSAENOTSOCK )
   {
    sprintf(b[0],"%s * WSAENOTSOCK",p);
    sprintf(b[1],"%s * The descriptor is not a socket.",p);
   }
 else if ( error == WSAEOPNOTSUPP )
   {
    sprintf(b[0],"%s * WSAEOPNOTSUPP",p);
    sprintf(b[1],"%s * MSG_OOB was specified, but the socket is",p);
    sprintf(b[2],"%s * not of type SOCK_STREAM.",p);
   }
 else if ( error == WSAESHUTDOWN )
   {
    sprintf(b[0],"%s * WSAESHUTDOWN",p);
    sprintf(b[1],"%s * The socket has been shutdown.",p);
   }
 else if ( error == WSAEWOULDBLOCK )
   {
    sprintf(b[0],"%s * WSAEWOULDBLOCK",p);
    sprintf(b[1],"%s * The socket is marked as non-blocking",p);
    sprintf(b[2],"%s * and receive operation would block.",p);
   }
 else if ( error == WSAEMSGSIZE )
   {
    sprintf(b[0],"%s * WSAEMSGSIZE",p);
    sprintf(b[1],"%s * The datagram was too large to",p);
    sprintf(b[2],"%s * fit into the specified buffer.",p);
   }
 else if ( error == WSAEINVAL )
   {
    sprintf(b[0],"%s * WSAEINVAL",p);
    sprintf(b[1],"%s * The socket has not been bound with bind().",p);
   }
 else if ( error == WSAECONNABORTED )
   {
    sprintf(b[0],"%s * WSAECONNABORTED",p);
    sprintf(b[1],"%s * The virtual circuit was aborted",p);
    sprintf(b[2],"%s * due to timeout or other failure.",p);
   }
 else if ( error == WSAECONNRESET )
   {
    sprintf(b[0],"%s * WSAECONNRESET",p);
    sprintf(b[1],"%s * The virtual circuit was reset by the remote side.",p);
   }
 else
   {
    sprintf(b[0],"%s * WSA????",p);
    sprintf(b[1],"%s * Unexpected WINSOCK error code %d",p,error);
   }

 #else 

      if ( error == EBADF       ) sprintf(b[0],"%s * EBADF", p );
 else if ( error == EAGAIN      ) sprintf(b[0],"%s * EAGAIN",p );
 else if ( error == EDQUOT      ) sprintf(b[0],"%s * EDQUOT",p );
 else if ( error == EFAULT      ) sprintf(b[0],"%s * EFAULT",p );
 else if ( error == EFBIG       ) sprintf(b[0],"%s * EFBIG", p );
 else if ( error == EINTR       ) sprintf(b[0],"%s * EINTR", p );
 else if ( error == EINVAL      ) sprintf(b[0],"%s * EINVAL",p );
 else if ( error == EIO         ) sprintf(b[0],"%s * EIO",   p );
 else if ( error == ENOSPC      ) sprintf(b[0],"%s * ENOSPC",p );
 else if ( error == ENXIO       ) sprintf(b[0],"%s * ENXIO", p );
 else if ( error == EPIPE       ) sprintf(b[0],"%s * EPIPE", p );
 else if ( error == ERANGE      ) sprintf(b[0],"%s * ERANGE",p );
 else if ( error == EINVAL      ) sprintf(b[0],"%s * EINVAL",p );
 else if ( error == EWOULDBLOCK ) sprintf(b[0],"%s * EWOULDBLOCK",p );

 else 
   {
    sprintf(b[0],"%s * E???? Unexpected error code %d",p,error);
   }

 #endif 

 for ( i=0; i<3; i++ )
    {
     if ( b[i][0] != 0 )
       {
        #ifdef MOD_GZIP_DEBUG1
        mod_gzip_printf("%s",npp(b[i]));
        #endif
       }
    }

 return( 1 );
}

int mod_gzip_translate_comerror( int error, char *buf )
{
 char bb[40]; 

 if ( buf == 0 ) 
   {
    return 0; 
   }

 #if defined(WIN32) || defined(NETWARE)

      if ( error == WSANOTINITIALISED ) sprintf(bb,"WSANOTINITIALISED");
 else if ( error == WSAENETDOWN       ) sprintf(bb,"WSAENETDOWN");
 else if ( error == WSAENOTCONN       ) sprintf(bb,"WSAENOTCONN");
 else if ( error == WSAEINTR          ) sprintf(bb,"WSAEINTR");
 else if ( error == WSAEINPROGRESS    ) sprintf(bb,"WSAEINPROGRESS");
 else if ( error == WSAENOTSOCK       ) sprintf(bb,"WSAENOTSOCK");
 else if ( error == WSAEOPNOTSUPP     ) sprintf(bb,"WSAEOPNOTSUPP");
 else if ( error == WSAESHUTDOWN      ) sprintf(bb,"WSAESHUTDOWN");
 else if ( error == WSAEWOULDBLOCK    ) sprintf(bb,"WSAEWOULDBLOCK");
 else if ( error == WSAEMSGSIZE       ) sprintf(bb,"WSAEMSGSIZE");
 else if ( error == WSAEINVAL         ) sprintf(bb,"WSAEINVAL");
 else if ( error == WSAECONNABORTED   ) sprintf(bb,"WSAECONNABORTED");
 else if ( error == WSAECONNRESET     ) sprintf(bb,"WSAECONNRESET");
 else                                   sprintf(bb,"%d=WSA????",error);

 #else 

      if ( error == EBADF       ) sprintf(bb,"EBADF"  );
 else if ( error == EAGAIN      ) sprintf(bb,"EAGAIN" );
 else if ( error == EDQUOT      ) sprintf(bb,"EDQUOT" );
 else if ( error == EFAULT      ) sprintf(bb,"EFAULT" );
 else if ( error == EFBIG       ) sprintf(bb,"EFBIG"  );
 else if ( error == EINTR       ) sprintf(bb,"EINTR"  );
 else if ( error == EINVAL      ) sprintf(bb,"EINVAL" );
 else if ( error == EIO         ) sprintf(bb,"EIO"    );
 else if ( error == ENOSPC      ) sprintf(bb,"ENOSPC" );
 else if ( error == ENXIO       ) sprintf(bb,"ENXIO"  );
 else if ( error == EPIPE       ) sprintf(bb,"EPIPE"  );
 else if ( error == ERANGE      ) sprintf(bb,"ERANGE" );
 else if ( error == EINVAL      ) sprintf(bb,"EINVAL" );
 else if ( error == EWOULDBLOCK ) sprintf(bb,"EWOULDBLOCK" );
 else                             sprintf(bb,"%d=E????",error);

 #endif 

 mod_gzip_strcpy( buf, bb ); 

 return( 1 );
}

#endif 

int mod_gzip_validate1(
request_rec   *r,
mod_gzip_conf *mgc,
char          *r__filename,
char          *r__uri,
char          *r__content_type,
char          *r__handler,
char          *fieldkey,
char          *fieldstring,
int            direction
)
{
 int   x                = 0;
 int   clen             = 0;
 int   hlen             = 0;
 int   flen             = 0;
 int   ulen             = 0;
 int   pass             = 0;
 int   passes           = 2;
 char *this_name        = 0;
 int   this_type        = 0;
 int   this_len1        = 0;
 int   this_action      = 0;
 int   this_include     = 0;
 char *checktarget      = 0;
 int   pass_result      = 0;
 int   action_value     = 0;
 int   filter_value     = 0;
 int   type_to_match    = 0;
 int   ok_to_check_it   = 0;
 int   http_field_check = 0;
 int   item_is_included = 0;
 int   item_is_excluded = 0;
 int   type_is_included = 0;

 regex_t *this_pregex   = NULL;
 int      regex_error   = 0;

 #ifdef MOD_GZIP_DEBUG1
 char cn[]="mod_gzip_validate1()";
 #endif

 if ( r__filename     ) flen = mod_gzip_strlen( (char *) r__filename );
 if ( r__uri          ) ulen = mod_gzip_strlen( (char *) r__uri );
 if ( r__content_type ) clen = mod_gzip_strlen( (char *) r__content_type );
 if ( r__handler      ) hlen = mod_gzip_strlen( (char *) r__handler );

 #ifdef MOD_GZIP_DEBUG1

 mod_gzip_printf( "%s: Entry...",cn);
 mod_gzip_printf( "%s: r__filename     = [%s]",cn,npp(r__filename));
 mod_gzip_printf( "%s: flen            = %d",  cn,flen);
 mod_gzip_printf( "%s: r__uri          = [%s]",cn,npp(r__uri));
 mod_gzip_printf( "%s: ulen            = %d",  cn,ulen);
 mod_gzip_printf( "%s: r__content_type = [%s]",cn,npp(r__content_type));
 mod_gzip_printf( "%s: clen            = %d",  cn,clen);
 mod_gzip_printf( "%s: r__handler      = [%s]",cn,npp(r__handler));
 mod_gzip_printf( "%s: hlen            = %d",  cn,hlen);
 mod_gzip_printf( "%s: fieldkey        = [%s]",cn,npp(fieldkey));
 mod_gzip_printf( "%s: fieldstring     = [%s]",cn,npp(fieldstring));
 mod_gzip_printf( "%s: direction       = %d",  cn,direction);

 #endif

 if ( ( fieldkey ) && ( fieldstring ) )
   {
    http_field_check = 1;

    passes = 1;

    if ( direction == MOD_GZIP_REQUEST )
      {
       type_to_match = MOD_GZIP_IMAP_ISREQHEADER;
      }
    else if ( direction == MOD_GZIP_RESPONSE )
      {
       type_to_match = MOD_GZIP_IMAP_ISRSPHEADER;
      }
    else
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Invalid 'direction' value.",cn);
       mod_gzip_printf( "%s: Must be MOD_GZIP_REQUEST or MOD_GZIP_RESPONSE",cn);
       mod_gzip_printf( "%s: Exit > return( MOD_GZIP_IMAP_DECLINED1 ) >",cn);
       #endif

       return( MOD_GZIP_IMAP_DECLINED1 );
      }
   }

 else if ( ( hlen == 0 ) && ( clen == 0 ) && ( flen == 0 ) )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: hlen = 0 = No handler name passed...",cn);
    mod_gzip_printf( "%s: clen = 0 = No valid content-type passed",cn);
    mod_gzip_printf( "%s: flen = 0 = No valid filename passed",cn);
    mod_gzip_printf( "%s: There is nothing we can use to search",cn);
    mod_gzip_printf( "%s: for a match in the inclusion/exclusion list.",cn);
    mod_gzip_printf( "%s: Exit > return( MOD_GZIP_IMAP_DECLINED1 ) >",cn);
    #endif

    return( MOD_GZIP_IMAP_DECLINED1 );
   }

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: passes = %d", cn,
                 (int) passes );
 mod_gzip_printf( "%s: http_field_check = %d", cn,
                 (int) http_field_check );
 mod_gzip_printf( "%s: mgc->imap_total_entries = %d", cn,
                 (int) mgc->imap_total_entries );
 #endif

 for ( pass=0; pass<passes; pass++ )
    {
     pass_result = 0;

     #ifdef MOD_GZIP_DEBUG1_VALIDATE1_VERBOSE1
     #ifdef MOD_GZIP_DEBUG1
     mod_gzip_printf( "%s: ",cn);
     #endif
     #endif

     filter_value = pass;

     #ifdef MOD_GZIP_DEBUG1_VALIDATE1_VERBOSE1
     #ifdef MOD_GZIP_DEBUG1
     mod_gzip_printf( "%s: pass         = %d", cn, pass );
     mod_gzip_printf( "%s: filter_value = %d", cn, filter_value );
     mod_gzip_printf( "%s: mgc->imap_total_entries = %d", cn,
                     (int) mgc->imap_total_entries );
     #endif
     #endif

     for ( x=0; x < mgc->imap_total_entries; x++ )
        {
         this_include = mgc->imap[x].include;
         this_type    = mgc->imap[x].type;
         this_action  = mgc->imap[x].action;

         #ifdef MOD_GZIP_DEBUG1_VALIDATE1_VERBOSE1
         #ifdef MOD_GZIP_DEBUG1

         mod_gzip_printf( "%s: --------------------------------------------",cn);
         mod_gzip_printf( "%s: http_field_check = %d",  cn,http_field_check );

         if ( http_field_check )
           {
            mod_gzip_printf( "%s: fieldkey         = [%s]",cn,npp(fieldkey));
            mod_gzip_printf( "%s: fieldstring      = [%s]",cn,npp(fieldstring));
           }
         else
           {
            mod_gzip_printf( "%s: r__filename      = [%s]",cn,npp(r__filename));
            mod_gzip_printf( "%s: r__uri           = [%s]",cn,npp(r__uri));
            mod_gzip_printf( "%s: r__content_type  = [%s]",cn,npp(r__content_type));
            mod_gzip_printf( "%s: r__handler       = [%s]",cn,npp(r__handler));
           }

         if ( this_include == 0 )
           {
            mod_gzip_printf( "%s: mgc->imap[%3.3d].include = %d EXCLUDE",cn,x,this_include);
           }
         else if ( this_include == 1 )
           {
            mod_gzip_printf( "%s: mgc->imap[%3.3d].include = %d INCLUDE",cn,x,this_include);
           }
         else
           {
            mod_gzip_printf( "%s: mgc->imap[%3.3d].include = %d ??? UNKNOWN VALUE",cn,x,this_include);
           }

         if ( mgc->imap[x].type == MOD_GZIP_IMAP_ISMIME )
           {
            mod_gzip_printf( "%s: mgc->imap[%3.3d].type    = %d MOD_GZIP_IMAP_ISMIME",
                              cn,x,this_type);
           }
         else if ( mgc->imap[x].type == MOD_GZIP_IMAP_ISHANDLER )
           {
            mod_gzip_printf( "%s: mgc->imap[%3.3d].type    = %d MOD_GZIP_IMAP_ISHANDLER",
                              cn,x,this_type);
           }
         else if ( mgc->imap[x].type == MOD_GZIP_IMAP_ISFILE )
           {
            mod_gzip_printf( "%s: mgc->imap[%3.3d].type    = %d MOD_GZIP_IMAP_ISFILE",
                              cn,x,this_type);
           }
         else if ( mgc->imap[x].type == MOD_GZIP_IMAP_ISURI )
           {
            mod_gzip_printf( "%s: mgc->imap[%3.3d].type    = %d MOD_GZIP_IMAP_ISURI",
                              cn,x,this_type);
           }
         else if ( mgc->imap[x].type == MOD_GZIP_IMAP_ISREQHEADER )
           {
            mod_gzip_printf( "%s: mgc->imap[%3.3d].type    = %d MOD_GZIP_IMAP_ISREQHEADER",
                              cn,x,this_type);
           }
         else if ( mgc->imap[x].type == MOD_GZIP_IMAP_ISRSPHEADER )
           {
            mod_gzip_printf( "%s: mgc->imap[%3.3d].type    = %d MOD_GZIP_IMAP_ISRSPHEADER",
                              cn,x,this_type);
           }
         else
           {
            mod_gzip_printf( "%s: mgc->imap[%3.3d].type    = %d MOD_GZIP_IMAP_IS??? Unknown type",
                              cn,x,this_type);
           }

         if ( mgc->imap[x].action == MOD_GZIP_IMAP_STATIC1 )
           {
            mod_gzip_printf( "%s: mgc->imap[%3.3d].action  = %d MOD_GZIP_IMAP_STATIC1",
                              cn,x,this_action);
           }
         else if ( mgc->imap[x].action == MOD_GZIP_IMAP_DYNAMIC1 )
           {
            mod_gzip_printf( "%s: mgc->imap[%3.3d].action  = %d MOD_GZIP_IMAP_DYNAMIC1",
                              cn,x,this_action);
           }
         else if ( mgc->imap[x].action == MOD_GZIP_IMAP_DYNAMIC2 )
           {
            mod_gzip_printf( "%s: mgc->imap[%3.3d].action  = %d MOD_GZIP_IMAP_DYNAMIC2",
                              cn,x,this_action);
           }
         else
           {
            mod_gzip_printf( "%s: mgc->imap[%3.3d].action  = %d MOD_GZIP_IMAP_??? Unknown action",
                              cn,x,this_action);
           }

         mod_gzip_printf( "%s: mgc->imap[%3.3d].name    = [%s]",cn,x,npp(mgc->imap[x].name));
         mod_gzip_printf( "%s: mgc->imap[%3.3d].namelen = %d",  cn,x,mgc->imap[x].namelen);

         #endif
         #endif

         if ( this_include == filter_value )
           {
            #ifdef MOD_GZIP_DEBUG1_VALIDATE1_VERBOSE1
            #ifdef MOD_GZIP_DEBUG1
            mod_gzip_printf( "%s: This record matches filter_value %d",
                              cn, filter_value );
            mod_gzip_printf( "%s: The record will be checked...",cn);
            #endif
            #endif

            type_is_included = 0;
            checktarget      = 0;

            if ( http_field_check )
              {
               if ( this_type == type_to_match )
                 {
                  type_is_included = 1;

                  checktarget = (char *) fieldstring;
                 }
              }
            else
              {
               if ( ( this_type == MOD_GZIP_IMAP_ISMIME ) &&
                    ( clen > 0 ) )
                 {
                  type_is_included = 1;

                  checktarget = r__content_type;
                 }
               else if ( ( this_type == MOD_GZIP_IMAP_ISFILE ) &&
                         ( flen > 0 ) )
                 {
                  type_is_included = 1;

                  checktarget = r__filename;
                 }
               else if ( ( this_type == MOD_GZIP_IMAP_ISURI ) &&
                         ( ulen > 0 ) )
                 {
                  type_is_included = 1;

                  checktarget = r__uri;
                 }
               else if ( ( this_type == MOD_GZIP_IMAP_ISHANDLER ) &&
                         ( hlen > 0 ) )
                 {
                  type_is_included = 1;

                  checktarget = r__handler;
                 }
              }

            if ( type_is_included )
              {
               #ifdef MOD_GZIP_DEBUG1_VALIDATE1_VERBOSE1
               #ifdef MOD_GZIP_DEBUG1
               mod_gzip_printf( "%s: type_is_included = %d = YES",cn,type_is_included);
               #endif
               #endif

               this_name   = mgc->imap[x].name;
               this_len1   = mgc->imap[x].len1;
               this_pregex = mgc->imap[x].pregex;

               ok_to_check_it = 1;

               if ( http_field_check )
                 {
                  #ifdef MOD_GZIP_DEBUG1_VALIDATE1_VERBOSE1
                  #ifdef MOD_GZIP_DEBUG1
                  mod_gzip_printf( "%s: fieldkey  = [%s]",cn,npp(fieldkey));
                  mod_gzip_printf( "%s: this_name = [%s]",cn,npp(this_name));
                  mod_gzip_printf( "%s: this_len1 = %d",  cn,this_len1);
                  mod_gzip_printf( "%s: Call mod_gzip_strnicmp(fieldkey,this_name,this_len1)...",cn);
                  #endif
                  #endif

                  if ( mod_gzip_strnicmp( fieldkey, this_name, this_len1 )==0)
                    {
                     #ifdef MOD_GZIP_DEBUG1_VALIDATE1_VERBOSE1
                     #ifdef MOD_GZIP_DEBUG1
                     mod_gzip_printf( "%s: .... mod_gzip_strnicmp() = TRUE",cn);
                     mod_gzip_printf( "%s: .... Field key name MATCHES",cn);
                     #endif
                     #endif
                    }
                  else
                    {
                     #ifdef MOD_GZIP_DEBUG1_VALIDATE1_VERBOSE1
                     #ifdef MOD_GZIP_DEBUG1
                     mod_gzip_printf( "%s: .... mod_gzip_strnicmp() = FALSE",cn);
                     mod_gzip_printf( "%s: .... Field key name does NOT MATCH",cn);
                     #endif
                     #endif

                     ok_to_check_it = 0;
                    }
                 }

               if ( ok_to_check_it )
                 {
                  #ifdef MOD_GZIP_DEBUG1_VALIDATE1_VERBOSE1
                  #ifdef MOD_GZIP_DEBUG1
                  mod_gzip_printf( "%s: ok_to_check_it = %d = YES",cn,ok_to_check_it);
                  #endif
                  #endif

                  if ( ( this_pregex ) && ( checktarget ) )
                    {
                     #ifdef MOD_GZIP_DEBUG1
                     mod_gzip_printf( "%s: 'this_pregex' is NON-NULL",cn);
                     mod_gzip_printf( "%s: Performing regular expression check...",cn);
                     mod_gzip_printf( "%s: Call ap_regexec( this_name=[%s], checktarget=[%s] )",
                                       cn, npp(this_name), npp(checktarget) );
                     #endif

                     regex_error =
                     ap_regexec(
                     this_pregex, checktarget, 0, (regmatch_t *) NULL, 0 );

                     if ( regex_error == 0 )
                       {
                        #ifdef MOD_GZIP_DEBUG1
                        mod_gzip_printf( "%s: YYYY regex_error = %d = MATCH!",cn,regex_error);
                        #endif

                        pass_result  = 1;
                        action_value = this_action;
                        break;
                       }
                     else
                       {
                        #ifdef MOD_GZIP_DEBUG1
                        mod_gzip_printf( "%s: NNNN regex_error = %d = NO MATCH!",cn,regex_error);
                        #endif
                       }
                    }
                  else
                    {
                     #ifdef MOD_GZIP_DEBUG1

                     if ( !this_pregex )
                       {
                        mod_gzip_printf( "%s: 'this_pregex' is NULL",cn);
                       }
                     if ( !checktarget )
                       {
                        mod_gzip_printf( "%s: 'checktarget' is NULL",cn);
                       }

                     mod_gzip_printf( "%s: No regular expression check performed",cn);

                     #endif
                    }
                 }
               else
                 {
                  #ifdef MOD_GZIP_DEBUG1_VALIDATE1_VERBOSE1
                  #ifdef MOD_GZIP_DEBUG1
                  mod_gzip_printf( "%s: ok_to_check_it = %d = NO",cn,ok_to_check_it);
                  mod_gzip_printf( "%s: The record has been SKIPPED...",cn);
                  #endif
                  #endif
                 }
              }
            else
              {
               #ifdef MOD_GZIP_DEBUG1_VALIDATE1_VERBOSE1
               #ifdef MOD_GZIP_DEBUG1
               mod_gzip_printf( "%s: type_is_included = %d = NO",cn,type_is_included);
               mod_gzip_printf( "%s: The record has been SKIPPED...",cn);
               #endif
               #endif
              }
           }
         else
           {
            #ifdef MOD_GZIP_DEBUG1_VALIDATE1_VERBOSE1
            #ifdef MOD_GZIP_DEBUG1
            mod_gzip_printf( "%s: This record does NOT match filter_value %d",
                              cn, filter_value );
            mod_gzip_printf( "%s: The record has been SKIPPED...",cn);
            #endif
            #endif
           }
        }

     #ifdef MOD_GZIP_DEBUG1_VALIDATE1_VERBOSE1
     #ifdef MOD_GZIP_DEBUG1
     mod_gzip_printf( "%s: --------------------------------------------",cn);
     mod_gzip_printf( "%s: pass_result = %d",cn,pass_result);
     #endif
     #endif

     if ( pass_result )
       {
        if ( pass == 0 ) item_is_excluded = 1;
        else             item_is_included = 1;

        break;
       }

    }/* End 'for ( pass=0; pass<passes; pass++ )' */
       
 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: item_is_excluded = %d",cn,item_is_excluded);
 mod_gzip_printf( "%s: item_is_included = %d",cn,item_is_included);
 #endif

 if ( item_is_excluded )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: The item is EXCLUDED...",cn);
    mod_gzip_printf( "%s: Exit > return( MOD_GZIP_IMAP_DECLINED1 ) >",cn);
    #endif

    return( MOD_GZIP_IMAP_DECLINED1 );
   }

 else if ( item_is_included )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: The item is INCLUDED...",cn);
    mod_gzip_printf( "%s: Exit > return( 1 ) >",cn);
    #endif

    return action_value;
   }

 if ( http_field_check )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: ?? Status unknown ?? Default is to 'accept'...",cn);
    mod_gzip_printf( "%s: Exit > return( MOD_GZIP_IMAP_STATIC1 ) >",cn);
    #endif

    return MOD_GZIP_IMAP_STATIC1;
   }

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Exit > return( MOD_GZIP_IMAP_DECLINED1 ) >",cn);
 #endif

 return( MOD_GZIP_IMAP_DECLINED1 );
}


/* NOTE: If API_VAR_EXPORT prefix is not used for 'top_module' */
/* declaration then MSVC 6.0 will give 'incosistent DLL linkage' */
/* warning during WIN32 compile... */

extern API_VAR_EXPORT module *top_module;

struct _table {
    array_header a;
#ifdef MAKE_TABLE_PROFILE
    void *creator;
#endif
};
typedef struct _table _table;

#ifdef MOD_GZIP_DEBUG1

int mod_gzip_dump_a_table( request_rec *r, _table *t )
{
 table_entry *elts = (table_entry *) t->a.elts;
 int i;

 char cn[]="mod_gzip_dump_a_table()";

 for ( i = 0; i < t->a.nelts; i++ )
    {
     mod_gzip_printf( "%s: %3.3d key=[%s] val=[%s]",
     cn,i,npp(elts[i].key),npp(elts[i].val));
    }

 return 0;
}

#endif 

#define MOD_GZIP_RUN_TYPE_CHECKERS      1
#define MOD_GZIP_RUN_TRANSLATE_HANDLERS 2

int mod_gzip_run_handlers( request_rec *r, int flag1 )
{
 int rc;
 int count=0;
 int runit=0;
 int handler_is_present=0;

 module *modp;

 #ifdef MOD_GZIP_DEBUG1
 #ifdef MOD_GZIP_DEBUG1_RUN_HANDLERS1
 const handler_rec *handp;
 char cn[]="mod_gzip_run_handlers()";
 #endif
 #endif

 #ifdef MOD_GZIP_DEBUG1
 #ifdef MOD_GZIP_DEBUG1_RUN_HANDLERS1
 mod_gzip_server_now = r->server;
 mod_gzip_printf( "%s: Entry...",cn);
 mod_gzip_printf( "%s: *IN: r->uri          =[%s]", cn, npp(r->uri));
 mod_gzip_printf( "%s: *IN: r->unparsed_uri =[%s]", cn, npp(r->unparsed_uri));
 mod_gzip_printf( "%s: *IN: r->filename     =[%s]", cn, npp(r->filename));
 mod_gzip_printf( "%s: *IN: r->handler      =[%s]", cn, npp(r->handler));
 mod_gzip_printf( "%s: *IN: r->content_type =[%s]", cn, npp(r->content_type));
 #endif
 #endif

 if ( flag1 == MOD_GZIP_RUN_TYPE_CHECKERS )
   {
    #ifdef MOD_GZIP_DEBUG1
    #ifdef MOD_GZIP_DEBUG1_RUN_HANDLERS1
    mod_gzip_printf( "%s: flag1 = %d = MOD_GZIP_RUN_TYPE_CHECKERS",cn,flag1);
    #endif
    #endif
   }
 else if ( flag1 == MOD_GZIP_RUN_TRANSLATE_HANDLERS )
   {
    #ifdef MOD_GZIP_DEBUG1
    #ifdef MOD_GZIP_DEBUG1_RUN_HANDLERS1
    mod_gzip_printf( "%s: flag1 = %d = MOD_GZIP_RUN_TRANSLATE_HANDLERS",cn,flag1);
    #endif
    #endif
   }
 else
   {
    #ifdef MOD_GZIP_DEBUG1
    #ifdef MOD_GZIP_DEBUG1_RUN_HANDLERS1
    mod_gzip_printf( "%s: flag1 = %d = MOD_GZIP_RUN_??? Unknown value",cn,flag1);
    mod_gzip_printf( "%s: ERROR: Exit > return( DECLINED ) >",cn);
    #endif
    #endif

    return( DECLINED );
   }

 for ( modp = top_module; modp; modp = modp->next )
    {
     #ifdef MOD_GZIP_DEBUG1
     #ifdef MOD_GZIP_DEBUG1_RUN_HANDLERS1
     mod_gzip_printf( "%s: count=%4.4d modp = %10.10ld modp->name=[%s]",
     cn,count,(long)modp,npp(modp->name));
     #endif
     #endif

     runit = 0;

     if ( modp )
       {
        runit = 1;

        if ( modp == &gzip_module )
          {
           runit = 0;
          }

        #ifdef FUTURE_USE
        if ( mod_gzip_strnicmp( (char *) modp->name, "mod_gzip.c",10)==0)
          {
           runit = 0;
          }
        #endif
       }

     if ( runit )
       {
        #ifdef MOD_GZIP_DEBUG1
        #ifdef MOD_GZIP_DEBUG1_RUN_HANDLERS1

        mod_gzip_printf( "%s: ++++++++++ MODULE FOUND!...",cn);
        mod_gzip_printf( "%s: ++++++++++ modp->module_index = %d",cn,(int)modp->module_index);

        #ifdef REFERENCE

        typedef struct {
        const char *content_type;
        int (*handler) (request_rec *);
        } handler_rec;

        typedef struct module_struct {
        ...
        const handler_rec *handlers;
        ...
        }module;

        #endif

        mod_gzip_printf( "%s: ++++++++++ METHODS",cn);
        mod_gzip_printf( "%s: ++++++++++ modp->translate_handler = %ld",cn,(long)modp->translate_handler);
        mod_gzip_printf( "%s: ++++++++++ modp->ap_check_user_id  = %ld",cn,(long)modp->ap_check_user_id);
        mod_gzip_printf( "%s: ++++++++++ modp->auth_checker      = %ld",cn,(long)modp->auth_checker);
        mod_gzip_printf( "%s: ++++++++++ modp->access_checker    = %ld",cn,(long)modp->access_checker);
        mod_gzip_printf( "%s: ++++++++++ modp->type_checker      = %ld",cn,(long)modp->type_checker);
        mod_gzip_printf( "%s: ++++++++++ modp->fixer_upper       = %ld",cn,(long)modp->fixer_upper);
        mod_gzip_printf( "%s: ++++++++++ modp->logger            = %ld",cn,(long)modp->logger);
        mod_gzip_printf( "%s: ++++++++++ modp->header_parser     = %ld",cn,(long)modp->header_parser);
        mod_gzip_printf( "%s: .......... CONTENT HANDLERS",cn);

        if ( !modp->handlers )
          {
           mod_gzip_printf( "%s: .......... NO CONTENT HANDLERS!",cn);
          }
        else
          {
           for ( handp = modp->handlers; handp->content_type; ++handp )
              {
               mod_gzip_printf( "%s: .......... handp->content_type = [%s]",
               cn,npp(handp->content_type));
               mod_gzip_printf( "%s: .......... handp->handler      = %ld",
               cn,(long)handp->handler);
              }
          }

        #endif
        #endif

        handler_is_present = 0;

        if ( flag1 == MOD_GZIP_RUN_TYPE_CHECKERS )
          {
           if ( modp->type_checker ) handler_is_present = 1;
          }
        else if ( flag1 == MOD_GZIP_RUN_TRANSLATE_HANDLERS )
          {
           if ( modp->translate_handler ) handler_is_present = 1;
          }

        #ifdef MOD_GZIP_DEBUG1
        #ifdef MOD_GZIP_DEBUG1_RUN_HANDLERS1
        mod_gzip_printf( "%s: handler_is_present = %d ",
                          cn, handler_is_present);
        #endif
        #endif

        if ( handler_is_present )
          {
           #ifdef MOD_GZIP_DEBUG1
           #ifdef MOD_GZIP_DEBUG1_RUN_HANDLERS1

           mod_gzip_printf( "%s: 'handler_is_present' is TRUE...",cn);

           mod_gzip_printf( "%s: r->filename     = [%s]",cn,npp(r->filename));
           mod_gzip_printf( "%s: r->uri          = [%s]",cn,npp(r->uri));
           mod_gzip_printf( "%s: r->handler      = [%s]",cn,npp(r->handler));
           mod_gzip_printf( "%s: r->content_type = [%s]",cn,npp(r->content_type));

           if ( flag1 == MOD_GZIP_RUN_TYPE_CHECKERS )
             {
              mod_gzip_printf( "%s: Call (modp->type_checker)(r)...",cn);
             }
           else if ( flag1 == MOD_GZIP_RUN_TRANSLATE_HANDLERS )
             {
              mod_gzip_printf( "%s: Call (modp->translate_handler)(r)...",cn);
             }

           #endif
           #endif

           if ( flag1 == MOD_GZIP_RUN_TYPE_CHECKERS )
             {
              rc = (modp->type_checker)( (request_rec *) r );
             }
           else if ( flag1 == MOD_GZIP_RUN_TRANSLATE_HANDLERS )
             {
              rc = (modp->translate_handler)( (request_rec *) r );
             }

           #ifdef MOD_GZIP_DEBUG1
           #ifdef MOD_GZIP_DEBUG1_RUN_HANDLERS1

           if ( flag1 == MOD_GZIP_RUN_TYPE_CHECKERS )
             {
              mod_gzip_printf( "%s: Back (modp->type_checker)(r)...",cn);
             }
           else if ( flag1 == MOD_GZIP_RUN_TRANSLATE_HANDLERS )
             {
              mod_gzip_printf( "%s: Back (modp->translate_handler)(r)...",cn);
             }

           mod_gzip_printf( "%s: r->filename     = [%s]",cn,npp(r->filename));
           mod_gzip_printf( "%s: r->uri          = [%s]",cn,npp(r->uri));
           mod_gzip_printf( "%s: r->handler      = [%s]",cn,npp(r->handler));
           mod_gzip_printf( "%s: r->content_type = [%s]",cn,npp(r->content_type));

           if ( rc == OK )
             {
              mod_gzip_printf( "%s: rc = %d = OK",cn, rc );
             }
           else if ( rc == DECLINED )
             {
              mod_gzip_printf( "%s: rc = %d = DECLINED",cn, rc );
             }
           else if ( rc == DONE )
             {
              mod_gzip_printf( "%s: rc = %d = DONE",cn, rc );
             }
           else
             {
              mod_gzip_printf( "%s: rc = %d = HTTP_ERROR?",cn, rc );
             }

           #endif
           #endif

           if ( rc == OK )
             {
              #ifdef MOD_GZIP_DEBUG1
              #ifdef MOD_GZIP_DEBUG1_RUN_HANDLERS1
              mod_gzip_printf( "%s: Call SUCCEEDED",cn );
              mod_gzip_printf( "%s: Exit > return( rc=%d ) >",cn,rc);
              #endif
              #endif

              return rc;
             }
           else
             {
              #ifdef MOD_GZIP_DEBUG1
              #ifdef MOD_GZIP_DEBUG1_RUN_HANDLERS1
              mod_gzip_printf( "%s: Call FAILED",cn );
              #endif
              #endif

              if ( rc != DECLINED )
                {
                 #ifdef MOD_GZIP_DEBUG1
                 #ifdef MOD_GZIP_DEBUG1_RUN_HANDLERS1
                 mod_gzip_printf( "%s: Something other than 'DECLINED' was returned.",cn);
                 mod_gzip_printf( "%s: Exit > return( rc=%d ) >",cn,rc);
                 #endif
                 #endif

                 return rc;
                }
              else
                {
                 #ifdef MOD_GZIP_DEBUG1
                 #ifdef MOD_GZIP_DEBUG1_RUN_HANDLERS1
                 mod_gzip_printf( "%s: 'DECLINED' was returned... Continuing chain...",cn);
                 #endif
                 #endif
                }
             }
          }
        else
          {
           #ifdef MOD_GZIP_DEBUG1
           #ifdef MOD_GZIP_DEBUG1_RUN_HANDLERS1
           mod_gzip_printf( "%s: 'handler_is_present' is FALSE...",cn);
           #endif
           #endif
          }
       }
     else
       {
        #ifdef MOD_GZIP_DEBUG1
        #ifdef MOD_GZIP_DEBUG1_RUN_HANDLERS1
        mod_gzip_printf( "%s: 'runit' is FALSE...",cn);
        mod_gzip_printf( "%s: SKIPPING THIS MODULE",cn);
        #endif
        #endif
       }

     count++;
    }

 #ifdef MOD_GZIP_DEBUG1
 #ifdef MOD_GZIP_DEBUG1_RUN_HANDLERS1
 mod_gzip_printf( "%s: Exit > return( DECLINED ) >",cn);
 #endif
 #endif

 return DECLINED;
}


int mod_gzip_delete_file(
request_rec   *r,
char          *filename
)
{
 #ifdef MOD_GZIP_DEBUG1
 char cn[]="mod_gzip_delete_file()";
 #endif

 int final_rc = 0; 

 #ifdef WIN32
 BOOL rc;
 #else
 int rc;
 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Entry...",cn);
 mod_gzip_printf( "%s: filename = [%s]",cn,npp(filename));
 #endif

 #ifdef WIN32

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Call DeleteFile(%s)...",cn,npp(filename));
 #endif

 rc = (BOOL) DeleteFile( filename );

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Back DeleteFile(%s)...",cn,npp(filename));
 #endif

 if ( rc == FALSE ) 
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: .... DeleteFile() FAILED",cn);
    #endif
   }
 else 
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: .... DeleteFile() SUCCEEDED",cn);
    #endif

    final_rc = 1; 
   }

 #else 

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Call unlink(%s)...",cn,npp(filename));
 #endif

 rc = (int) unlink( filename );

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Back unlink(%s)...",cn,npp(filename));
 #endif

 if ( rc < 0 ) 
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: .... unlink() FAILED",cn);
    #endif
   }
 else 
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: .... unlink() SUCCEEDED",cn);
    #endif

    final_rc = 1; 
   }

 #endif 

 #ifdef MOD_GZIP_DEBUG1

 if ( final_rc == 1 ) 
   {
    mod_gzip_printf( "%s: Exit > return( final_rc = %d ) SUCCESS >",cn,final_rc);
   }
 else 
   {
    mod_gzip_printf( "%s: Exit > return( final_rc = %d ) FAILURE >",cn,final_rc);
   }

 #endif 

 return( final_rc );
}

static void mod_gzip_init( server_rec *server, pool *p )
{
 int add_version_info=1;

 #ifdef MOD_GZIP_DEBUG1
 char cn[]="mod_gzip_init()";
 #endif

 mod_gzip_conf *mgc;

 #ifdef MOD_GZIP_DEBUG1

 mod_gzip_server_now = server;

 mod_gzip_printf( " " );
 mod_gzip_printf( "%s: Entry...", cn );

 #endif

 mgc = ( mod_gzip_conf * )
 ap_get_module_config( server->module_config, &gzip_module );

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: MODULE_MAGIC_NUMBER = %ld", cn, (long) MODULE_MAGIC_NUMBER );
 #endif

 if ( add_version_info )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: add_version_info = TRUE", cn );
    #endif

    #if MODULE_MAGIC_NUMBER >= 19980507

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: MODULE_MAGIC_NUMBER is >= 19980507",cn);
    mod_gzip_printf( "%s: Call ap_add_version_component(%s)...",
    cn, npp(MOD_GZIP_VERSION_INFO_STRING));
    #endif

    ap_add_version_component( MOD_GZIP_VERSION_INFO_STRING );

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Back ap_add_version_component(%s)...",
    cn, npp(MOD_GZIP_VERSION_INFO_STRING));
    #endif

    #else

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: MODULE_MAGIC_NUMBER is NOT >= 19980507", cn );
    mod_gzip_printf( "%s: ap_add_version_component() NOT called.", cn );
    #endif

    #endif
   }
 else
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: add_version_info = FALSE", cn );
    mod_gzip_printf( "%s: ap_add_version_component() NOT called.", cn );
    #endif
   }

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Initialization completed...", cn );
 mod_gzip_printf( "%s: Exit > return( void ) >", cn );
 mod_gzip_printf( " " );
 #endif
}

int mod_gzip_strncmp( char *s1, char *s2, int len1 )
{
 int  i;
 char ch1;
 char ch2;

 if ( ( s1 == 0 ) || ( s2 == 0 ) )
   {
    return 1;
   }

 for ( i=0; i<len1; i++ )
    {
     if ( ( *s1 == 0 ) || ( *s2 == 0 ) ) return( 1 ); 

     ch1 = *s1;
     ch2 = *s2;

     if ( ch1 == '/' ) ch1 = '\\';
     if ( ch2 == '/' ) ch2 = '\\';

     if ( ch1 != ch2 ) return 1;

     s1++;
     s2++;
    }

 return 0;
}

int mod_gzip_strnicmp( char *s1, char *s2, int len1 )
{
 int  i;
 char ch1;
 char ch2;

 if ( ( s1 == 0 ) || ( s2 == 0 ) )
   {
    return 1;
   }

 for ( i=0; i<len1; i++ )
    {
     if ( ( *s1 == 0 ) || ( *s2 == 0 ) ) return( 1 ); 

     ch1 = *s1;
     ch2 = *s2;

     if ( ch1 > 96 ) ch1 -= 32;
     if ( ch2 > 96 ) ch2 -= 32;

     if ( ch1 == '/' ) ch1 = '\\';
     if ( ch2 == '/' ) ch2 = '\\';

     if ( ch1 != ch2 ) return 1;

     s1++;
     s2++;
    }

 return 0;
}

int mod_gzip_strendswith( char *s1, char *s2, int ignorcase )
{
 int len1;
 int len2;

 if ( ( s1 == 0 ) || ( s2 == 0 ) )
   {
    return 0;
   }

 len1 = mod_gzip_strlen( s1 );
 len2 = mod_gzip_strlen( s2 );

 if ( len1 < len2 )
   {
    /* Source string is shorter than search string */
    /* so no match is possible */

    return 0;
   }

 s1 += ( len1 - len2 );

 if ( ignorcase )
   {
    if ( mod_gzip_strnicmp( s1, s2, len2 ) == 0 ) return 1; /* TRUE */
   }
 else
   {
    if ( mod_gzip_strncmp(  s1, s2, len2 ) == 0 ) return 1; /* TRUE */
   }

 return 0; /* FALSE */
}

int mod_gzip_strlen( char *s1 )
{
 int len = 0;

 if ( s1 != 0 )
   {
    while( *s1 != 0 ) { s1++; len++; }
   }

 return len;
}

int mod_gzip_strcpy( char *s1, char *s2 )
{
 int len = 0;

 if ( ( s1 != 0 )&&( s2 != 0 ) )
   {
    while( *s2 != 0 ) { *s1++ = *s2++; len++; }
    *s1=0; 
   }

 return len;
}

int mod_gzip_strcat( char *s1, char *s2 )
{
 int len = 0;

 if ( s1 != 0 )
   {
    while( *s1 != 0 ) { s1++; len++; }
    if ( s2 != 0 )
      {
       while( *s2 != 0 ) { *s1++ = *s2++; len++; }
       *s1 = 0; 
      }
   }

 return len;
}

int mod_gzip_stringcontains( char *source, char *substring )
{
 int i;
 int len1;
 int len2;
 int len3;
 int offset=1; 

 char *source1;    
 char *substring1; 

 if ( source == NULL )
   {
    return 0;
   }

 if ( substring == NULL )
   {
    return 0;
   }

 source1    = source;    
 substring1 = substring; 

 len1 = mod_gzip_strlen( source    );
 len2 = mod_gzip_strlen( substring );

 if ( len1 < len2 )
   {
    return 0;
   }

 len3 = len1 - len2;

 for ( i=0; i<=len3; i++ )
    {
     if ( mod_gzip_strnicmp( source, substring, len2 ) == 0 )
       {
        source    = source1;    
        substring = substring1; 

        return offset;
       }

     source++;
     offset++;
    }

 source    = source1;    
 substring = substring1; 

 return 0;
}

int mod_gzip_show_request_record(
request_rec *r,
char        *cn
)
{
 #ifdef MOD_GZIP_DEBUG1

 mod_gzip_printf( "%s: -------------------------------------------------------------",cn);
 mod_gzip_printf( "%s: REQUEST RECORD SNAPSHOT",cn);
 mod_gzip_printf( "%s: -------------------------------------------------------------",cn);
 mod_gzip_printf( "%s: r = %ld", cn, (long) r );
 mod_gzip_printf( "%s: r->connection->keepalive=%d", cn, r->connection->keepalive );
 mod_gzip_printf( "%s: r->the_request=[%s]", cn, npp(r->the_request) );
 mod_gzip_printf( "%s: -------------------------------------------------------------",cn);
 mod_gzip_printf( "%s: r->next = %ld",cn,(long)r->next);
 mod_gzip_printf( "%s: r->prev = %ld",cn,(long)r->prev);
 mod_gzip_printf( "%s: r->main = %ld",cn,(long)r->main);
 mod_gzip_printf( "%s: -------------------------------------------------------------",cn);
 mod_gzip_printf( "%s: r->sent_bodyct  = %ld",cn,(long)r->sent_bodyct);
 mod_gzip_printf( "%s: r->bytes_sent   = %ld",cn,(long)r->bytes_sent);
 mod_gzip_printf( "%s: r->chunked      = %ld",cn,(long)r->chunked);
 mod_gzip_printf( "%s: r->byterange    = %ld",cn,(long)r->byterange);
 mod_gzip_printf( "%s: r->clength      = %ld (The 'real' content length )",cn,(long)r->clength);
 mod_gzip_printf( "%s: r->byterange    = %ld",cn,(long)r->remaining);
 mod_gzip_printf( "%s: r->read_length  = %ld",cn,(long)r->read_length);
 mod_gzip_printf( "%s: r->read_body    = %ld",cn,(long)r->read_body);
 mod_gzip_printf( "%s: r->read_chunked = %ld",cn,(long)r->read_chunked);
 mod_gzip_printf( "%s: -------------------------------------------------------------",cn);
 mod_gzip_printf( "%s: *IN: r->uri                 =[%s]", cn, npp(r->uri));
 mod_gzip_printf( "%s: *IN: r->unparsed_uri        =[%s]", cn, npp(r->unparsed_uri));
 mod_gzip_printf( "%s: *IN: r->filename            =[%s]", cn, npp(r->filename));
 mod_gzip_printf( "%s: *IN: r->path_info           =[%s]", cn, npp(r->path_info));
 mod_gzip_printf( "%s: *IN: r->args                =[%s]", cn, npp(r->args));
 mod_gzip_printf( "%s: *IN: r->header_only         =%d",   cn, r->header_only );
 mod_gzip_printf( "%s: *IN: r->protocol            =[%s]", cn, npp(r->protocol));
 mod_gzip_printf( "%s: *IN: r->proto_num           =%d",   cn, r->proto_num );
 mod_gzip_printf( "%s: *IN: r->hostname            =[%s]", cn, npp(r->hostname));
 mod_gzip_printf( "%s: *IN: r->the_request         =[%s]", cn, npp(r->the_request));
 mod_gzip_printf( "%s: *IN: r->assbackwards        =%d",   cn, r->assbackwards );
 mod_gzip_printf( "%s: *IN: r->status_line         =[%s]", cn, npp(r->status_line));
 mod_gzip_printf( "%s: *IN: r->status              =%d",   cn, r->status );
 mod_gzip_printf( "%s: *IN: r->method              =[%s]", cn, npp(r->method));
 mod_gzip_printf( "%s: *IN: r->method_number       =%d",   cn, r->method_number );
 mod_gzip_printf( "%s: *IN: r->content_type        =[%s]", cn, npp(r->content_type));
 mod_gzip_printf( "%s: *IN: r->handler             =[%s]", cn, npp(r->handler));
 mod_gzip_printf( "%s: *IN: r->content_encoding    =[%s]", cn, npp(r->content_encoding));
 mod_gzip_printf( "%s: *IN: r->content_language    =[%s]", cn, npp(r->content_language));
 mod_gzip_printf( "%s: -------------------------------------------------------------",cn);
 mod_gzip_printf( "%s: *IN: r->parsed_uri.scheme   =[%s]", cn, npp(r->parsed_uri.scheme));
 mod_gzip_printf( "%s: *IN: r->parsed_uri.hostinfo =[%s]", cn, npp(r->parsed_uri.hostinfo));
 mod_gzip_printf( "%s: *IN: r->parsed_uri.user     =[%s]", cn, npp(r->parsed_uri.user));
 mod_gzip_printf( "%s: *IN: r->parsed_uri.password =[%s]", cn, npp(r->parsed_uri.password));
 mod_gzip_printf( "%s: *IN: r->parsed_uri.hostname =[%s]", cn, npp(r->parsed_uri.hostname));
 mod_gzip_printf( "%s: *IN: r->parsed_uri.port_str =[%s]", cn, npp(r->parsed_uri.port_str));
 mod_gzip_printf( "%s: *IN: r->parsed_uri.port     =%u",   cn, r->parsed_uri.port );
 mod_gzip_printf( "%s: *IN: r->parsed_uri.path     =[%s]", cn, npp(r->parsed_uri.path));
 mod_gzip_printf( "%s: *IN: r->parsed_uri.query    =[%s]", cn, npp(r->parsed_uri.query));
 mod_gzip_printf( "%s: *IN: r->parsed_uri.fragment =[%s]", cn, npp(r->parsed_uri.fragment));
 mod_gzip_printf( "%s: -------------------------------------------------------------",cn);
 mod_gzip_printf( "%s: 'r->headers_in'...",cn);
 mod_gzip_printf( "%s: -------------------------------------------------------------",cn);
 mod_gzip_dump_a_table( r, (_table *) r->headers_in );
 mod_gzip_printf( "%s: -------------------------------------------------------------",cn);
 mod_gzip_printf( "%s: 'r->headers_out'...",cn);
 mod_gzip_printf( "%s: -------------------------------------------------------------",cn);
 mod_gzip_dump_a_table( r, (_table *) r->headers_out );
 mod_gzip_printf( "%s: -------------------------------------------------------------",cn);
 mod_gzip_printf( "%s: 'r->err_headers_out'...",cn);
 mod_gzip_printf( "%s: -------------------------------------------------------------",cn);
 mod_gzip_dump_a_table( r, (_table *) r->err_headers_out );
 mod_gzip_printf( "%s: -------------------------------------------------------------",cn);
 mod_gzip_printf( "%s: 'r->subprocess_env'...",cn);
 mod_gzip_printf( "%s: -------------------------------------------------------------",cn);
 mod_gzip_dump_a_table( r, (_table *) r->subprocess_env );
 mod_gzip_printf( "%s: -------------------------------------------------------------",cn);
 mod_gzip_printf( "%s: 'r->notes'...",cn);
 mod_gzip_printf( "%s: -------------------------------------------------------------",cn);
 mod_gzip_dump_a_table( r, (_table *) r->notes );
 mod_gzip_printf( "%s: -------------------------------------------------------------",cn);

 if ( r->next )
   {
    mod_gzip_printf( "%s: r->next is valid... showing 'r->next' record...",cn);

    mod_gzip_show_request_record( r->next, cn );
   }

 #endif

 return 1;
}

#ifndef WIN32
long fake_tid = 99;
#endif

int mod_gzip_create_unique_filename(
char *prefix,
char *target,
int   targetmaxlen
)
{
 long  process_id = 0;  
 long  thread_id  = 0;  

 #ifdef MOD_GZIP_DEBUG1
 char cn[]="mod_gzip_create_unique_filename()";
 #endif

 int prefixlen = 0;
 char slash[4];

 #ifdef WIN32
 process_id = (long) GetCurrentProcessId();
 thread_id  = (long) GetCurrentThreadId();
 #else 

 process_id = (long) getpid();
 thread_id  = (long) fake_tid;
 fake_tid++;
 if ( fake_tid > 9999999 ) fake_tid = 99; 
 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Entry...",cn );
 mod_gzip_printf( "%s: prefix            = [%s]",cn,npp(prefix));
 mod_gzip_printf( "%s: target            = %ld", cn,(long)target);
 mod_gzip_printf( "%s: targetmaxlen      = %ld", cn,(long)targetmaxlen);
 mod_gzip_printf( "%s: process_id        = %ld", cn,(long)process_id );
 mod_gzip_printf( "%s: thread_id         = %ld", cn,(long)thread_id  );
 mod_gzip_printf( "%s: mod_gzip_iusn     = %ld", cn,(long)mod_gzip_iusn );
 #endif

 if ( ( !target ) || ( targetmaxlen == 0 ) )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Invalid target or targetmaxlen value.",cn);
    mod_gzip_printf( "%s: Exit > return( 1 ) > ERROR >",cn );
    #endif

    return 1;
   }

 if ( prefix ) 
   {
    prefixlen = mod_gzip_strlen( prefix );
   }

 if ( prefixlen > 0 ) 
   {
    slash[0]=0; 

    if ( ( *(prefix+(prefixlen-1)) != '\\' ) &&
         ( *(prefix+(prefixlen-1)) != '/'  ) )
      {
       #ifdef WIN32
       slash[0]='\\';
       #else
       slash[0]='/';
       #endif
       slash[1]=0;
      }

    sprintf(
    target,
    "%s%s_%ld_%ld_%ld.wrk",
    prefix,              
    slash,               
    (long) process_id,   
    (long) thread_id,    
    (long) mod_gzip_iusn 
    );
   }
 else 
   {
    sprintf(
    target,
    "_%ld_%ld_%ld.wrk",
    (long) process_id,   
    (long) thread_id,    
    (long) mod_gzip_iusn 
    );
   }

 mod_gzip_iusn++; 

 if ( mod_gzip_iusn > 999999999L ) mod_gzip_iusn = 1; 

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: target = [%s]",cn,npp(target));
 mod_gzip_printf( "%s: Exit > return( 0 ) >",cn );
 #endif

 return 0;
}

int mod_gzip_type_checker( request_rec *r )
{
 int         i           = 0;
 int         rc          = 0;
 int         field_ok    = 0;
 int         action_flag = 0;
 const char *tablekey    = 0;
 const char *tablestring = 0;
 int         accept_encoding_gzip_seen = 0;

 _table *t = 0;
 table_entry *elts = 0;

 mod_gzip_conf *dconf = 0;

 #ifdef MOD_GZIP_DEBUG1
 char cn[]="mod_gzip_type_checker()";
 mod_gzip_conf *sconf = 0;
 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( " " );
 mod_gzip_printf( "%s: ''''Entry...",cn);
 mod_gzip_printf( "%s: r                = %ld", cn,(long)r);
 mod_gzip_printf( "%s: r->main          = %ld", cn,(long)r->main);
 mod_gzip_printf( "%s: r->next          = %ld", cn,(long)r->next);
 mod_gzip_printf( "%s: r->prev          = %ld", cn,(long)r->prev);
 mod_gzip_printf( "%s: r->header_only   = %ld", cn,(long)r->header_only );
 mod_gzip_printf( "%s: r->method_number = %ld", cn,(long)r->method_number );
 mod_gzip_printf( "%s: r->method        = [%s]",cn,npp(r->method));
 mod_gzip_printf( "%s: r->unparsed_uri  = [%s]",cn,npp(r->unparsed_uri));
 mod_gzip_printf( "%s: r->uri           = [%s]",cn,npp(r->uri));
 mod_gzip_printf( "%s: r->filename      = [%s]",cn,npp(r->filename));
 mod_gzip_printf( "%s: r->handler       = [%s]",cn,npp(r->handler));
 mod_gzip_printf( "%s: r->content_type  = [%s]",cn,npp(r->content_type));
 #endif

 #ifdef MOD_GZIP_USES_APACHE_LOGS

 if ( r->main )
   {
    ap_table_setn( r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"DECLINED:UNHANDLED_SUBREQ"));
   }
 else if ( r->prev )
   {
    ap_table_setn( r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"DECLINED:UNHANDLED_REDIR"));
   }
 else
   {
    ap_table_setn( r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"DECLINED:INIT1"));
   }

 ap_table_setn( r->notes,"mod_gzip_input_size", ap_pstrdup(r->pool,"0"));
 ap_table_setn( r->notes,"mod_gzip_output_size",ap_pstrdup(r->pool,"0"));
 ap_table_setn( r->notes,"mod_gzip_compression_ratio",ap_pstrdup(r->pool,"0"));

 #endif

 if ( r->filename )
   {
    if ( mod_gzip_strendswith( r->filename, ".gz", 1 ) )
      {
       #ifdef MOD_GZIP_USES_APACHE_LOGS
       if ( r->prev )
         {
          /* This is a mod_gzip negotiated .gz static file transmit... */
          ap_table_setn( r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"DECLINED:STATIC_GZ_FOUND"));
         }
       else
         {
          /* This is a direct request from client for a .gz file... */
          ap_table_setn( r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"DECLINED:FEXT_GZ"));
         }
       #endif

       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: r->filename ends with '.gz'...",cn);
       mod_gzip_printf( "%s: Exit > return( DECLINED ) >",cn);
       #endif

       return DECLINED;
      }
    #ifdef MOD_GZIP_DEBUG1
    else
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: r->filename does NOT end with '.gz'...",cn);
       mod_gzip_printf( "%s: OK to continue...",cn);
       #endif
      }
    #endif
   }

 dconf = ( mod_gzip_conf * )
 ap_get_module_config( r->per_dir_config, &gzip_module );

 #ifdef MOD_GZIP_DEBUG1

 sconf = ( mod_gzip_conf * )
 ap_get_module_config( r->server->module_config, &gzip_module );

 mod_gzip_printf( "%s: r->server->server_hostname = [%s]", cn,npp(r->server->server_hostname));
 mod_gzip_printf( "%s: sconf        = %ld", cn,(long)sconf);
 mod_gzip_printf( "%s: sconf->loc   = [%s]",cn,npp(sconf->loc));
 mod_gzip_printf( "%s: sconf->is_on = %ld", cn,(long)sconf->is_on);
 mod_gzip_printf( "%s: dconf        = %ld", cn,(long)dconf);
 mod_gzip_printf( "%s: dconf->loc   = [%s]",cn,npp(dconf->loc));
 mod_gzip_printf( "%s: dconf->is_on = %ld", cn,(long)dconf->is_on);

 #endif

 if ( !dconf )
   {
    #ifdef MOD_GZIP_USES_APACHE_LOGS
    ap_table_setn( r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"DECLINED:NO_DCONF"));
    #endif

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: 'dconf' is NULL. Unable to continue.",cn);
    mod_gzip_printf( "%s: Exit > return( DECLINED ) >", cn);
    #endif

    return DECLINED;
   }

 if ( !dconf->is_on )
   {
    #ifdef MOD_GZIP_USES_APACHE_LOGS
    ap_table_setn( r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"DECLINED:OFF"));
    #endif

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: 'dconf->is_on' is FALSE",cn);
    mod_gzip_printf( "%s: mod_gzip is not turned ON for this location...",cn);
    mod_gzip_printf( "%s: This transaction will be ignored...",cn);
    mod_gzip_printf( "%s: Exit > return( DECLINED ) >",cn);
    #endif

    return DECLINED;
   }

 if ( ( r->method_number != M_GET  ) &&
      ( r->method_number != M_POST ) )
   {
    #ifdef MOD_GZIP_USES_APACHE_LOGS
    ap_table_setn( r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"DECLINED:NOT_GET_OR_POST"));
    #endif

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: r->method_number is NOT M_GET or M_POST",cn);
    mod_gzip_printf( "%s: Ignoring this request...",cn);
    mod_gzip_printf( "%s: Exit > return( DECLINED ) >",cn);
    #endif

    return DECLINED;
   }

 if ( r->header_only  )
   {
    #ifdef MOD_GZIP_USES_APACHE_LOGS
    ap_table_setn( r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"DECLINED:HEAD_REQUEST"));
    #endif

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: r->header_only is TRUE...",cn);
    mod_gzip_printf( "%s: Ignoring this HEAD request...",cn);
    mod_gzip_printf( "%s: Exit > return( DECLINED ) >",cn);
    #endif

    return DECLINED;
   }

 if ( r->main ) /* SUBREQUEST */
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: r->main is TRUE - This is a subrequest...",cn);
    mod_gzip_printf( "%s: Exit > return( DECLINED ) >",cn);
    #endif

    return DECLINED;
   }
 else
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: r->main is FALSE",cn);
    mod_gzip_printf( "%s: This is NOT a subrequest in progress...",cn);
    mod_gzip_printf( "%s: OK to continue...",cn);
    #endif
   }

 if ( r->prev ) /* REDIRECT */
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: r->prev is NON-NULL... This is a redirection",cn);
    #ifdef MOD_GZIP_DEBUG1_SHOW_REQUEST_RECORD1
    mod_gzip_printf( "%s: Showing contents of r->prev now...",cn);
    mod_gzip_show_request_record( r->prev, cn );
    #endif
    #endif

    tablestring = ap_table_get(r->prev->notes, "mod_gzip_running");

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: r->prev->notes->mod_gzip_running = [%s]",
                      cn,npp(tablestring));
    #endif

    if ( tablestring )
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: 'mod_gzip_running' note FOUND",cn);
       #endif

       if ( *tablestring == '1' )
         {
          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: 'mod_gzip_running' note value is '1'...",cn);
          #endif

          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: ************************************************",cn);
          mod_gzip_printf( "%s: mod_gzip is currently 'running' so we ",cn);
          mod_gzip_printf( "%s: must return DECLINED so that other type_checker",cn);
          mod_gzip_printf( "%s: hooks can fire...",cn);
          mod_gzip_printf( "%s: ************************************************",cn);
          mod_gzip_printf( "%s: IMPORTANT: We must now 'replicate' the note flag",cn);
          mod_gzip_printf( "%s: on this record or next time around r->prev",cn);
          mod_gzip_printf( "%s: note flag check will be FALSE.",cn);
          mod_gzip_printf( "%s: ************************************************",cn);
          #endif

          ap_table_setn(r->notes,"mod_gzip_running",ap_pstrdup(r->pool,"1"));

          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: r->notes 'mod_gzip_running' set with value = '1'",cn);
          mod_gzip_printf( "%s: ************************************************",cn);
          mod_gzip_printf( "%s: Exit > return( DECLINED ) >",cn);
          #endif

          return DECLINED;
         }
       else
         {
          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: 'mod_gzip_running' note value is NOT '1'...",cn);
          #endif
         }
      }
    else
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: 'mod_gzip_running' note NOT FOUND",cn);
       #endif
      }
   }
 #ifdef MOD_GZIP_DEBUG1
 else
   {
    mod_gzip_printf( "%s: r->prev is NULL... This is NOT a redirection",cn);
   }
 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: dconf->min_http = %ld", cn, (long) dconf->min_http );
 mod_gzip_printf( "%s: r->proto_num    = %ld", cn, (long) r->proto_num );
 #endif

 if ( ( dconf->min_http > 0 ) && ( r->proto_num > 0 ) )
   {
    if ( r->proto_num < dconf->min_http )
      {
       #ifdef MOD_GZIP_USES_APACHE_LOGS
       ap_table_setn( r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"DECLINED:HTTP_LEVEL_TOO_LOW"));
       #endif

       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: HTTP protocol version level is TOO LOW", cn);
       mod_gzip_printf( "%s: Exit > return( DECLINED ) >", cn);
       #endif

       return DECLINED;
      }
    else
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: HTTP protocol version level is OK", cn);
       #endif
      }
   }

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Checking for [Accept-Encoding: gzip]", cn );
 #endif

 tablestring = ap_table_get(r->headers_in, "Accept-Encoding");

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: r->headers_in->Accept-Encoding = [%s]",
                   cn,npp(tablestring));
 #endif

 if ( tablestring )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: 'Accept-Encoding' field seen...", cn);
    mod_gzip_printf( "%s: Checking for 'gzip' value...", cn);
    #endif

    if ( mod_gzip_stringcontains( (char *)tablestring, "gzip" ) )
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: 'gzip' value seen...", cn);
       #endif

       accept_encoding_gzip_seen = 1;
      }
    else
      {
       #ifdef MOD_GZIP_USES_APACHE_LOGS
       ap_table_setn( r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"DECLINED:NO_GZIP"));
       #endif

       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: 'gzip' value NOT seen...", cn);
       mod_gzip_printf( "%s: Exit > return( DECLINED ) >", cn);
       #endif

       return DECLINED;
      }
   }
 else
   {
    #ifdef MOD_GZIP_USES_APACHE_LOGS
    ap_table_setn( r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"DECLINED:NO_ACCEPT_ENCODING"));
    #endif

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: 'Accept-Encoding' field NOT seen...", cn);
    mod_gzip_printf( "%s: Exit > return( DECLINED ) >", cn);
    #endif

    return DECLINED;
   }

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: accept_encoding_gzip_seen = %ld",
                   cn, accept_encoding_gzip_seen );
 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: dconf->imap_total_entries = %d", cn,
                 (int) dconf->imap_total_entries );
 #endif

 if ( dconf->imap_total_entries < 1 )
   {
    #ifdef MOD_GZIP_USES_APACHE_LOGS
    ap_table_setn( r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"DECLINED:NO_ITEMS_DEFINED"));
    #endif

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: There are no IMAP entries. Unable to include/exclude",cn);
    mod_gzip_printf( "%s: Exit > return( DECLINED ) >", cn);
    #endif

    return DECLINED;
   }

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: dconf->imap_total_isreqheader = %d", cn,
                 (int) dconf->imap_total_isreqheader );
 #endif

 if ( dconf->imap_total_isreqheader > 0 )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Checking inbound REQUEST header fields...", cn );
    #endif

    t    = (_table      *) r->headers_in;
    elts = (table_entry *) t->a.elts;

    for ( i = 0; i < t->a.nelts; i++ )
       {
        #ifdef MOD_GZIP_DEBUG1
        mod_gzip_printf( "%s: %3.3d key=[%s] val=[%s]",
        cn,i,npp(elts[i].key),npp(elts[i].val));
        #endif

        tablekey    = elts[i].key;
        tablestring = elts[i].val;

        if (( tablekey && tablestring ))
          {
           #ifdef MOD_GZIP_DEBUG1
           mod_gzip_printf( "%s: Checking key[%s] string[%s]",
                                 cn,npp(tablekey),npp(tablestring));
           mod_gzip_printf( "%s: Call mod_gzip_validate1()...",cn);
           #endif

           field_ok =
           mod_gzip_validate1(
           (request_rec   *) r,
           (mod_gzip_conf *) dconf,
           NULL, /* r->filename     (Not used here) */
           NULL, /* r->uri          (Not used here) */
           NULL, /* r->content_type (Not used here) */
           NULL, /* r->handler      (Not used here) */
           (char *) tablekey,    /* (Field key    ) */
           (char *) tablestring, /* (Field string ) */
           MOD_GZIP_REQUEST      /* (Direction    ) */
           );

           #ifdef MOD_GZIP_DEBUG1
           mod_gzip_printf( "%s: Back mod_gzip_validate1()...",cn);
           mod_gzip_printf( "%s: field_ok = %d",cn,field_ok);
           #endif

           if ( field_ok == MOD_GZIP_IMAP_DECLINED1 )
             {
              #ifdef MOD_GZIP_USES_APACHE_LOGS
              ap_table_setn( r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"DECLINED:REQ_HEADER_FIELD_EXCLUDED"));
              #endif

              #ifdef MOD_GZIP_DEBUG1
              mod_gzip_printf( "%s: This request is EXCLUDED...",cn);
              mod_gzip_printf( "%s: Exit > return( DECLINED ) >", cn);
              #endif

              return DECLINED;
             }
          }
       }
   }
 #ifdef MOD_GZIP_DEBUG1
 else
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: NO CHECK required on inbound REQUEST header fields...", cn );
    #endif
   }
 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: 1 ***: r->uri         =[%s]", cn, npp(r->uri ));
 mod_gzip_printf( "%s: 1 ***: r->unparsed_uri=[%s]", cn, npp(r->unparsed_uri ));
 mod_gzip_printf( "%s: 1 ***: r->filename    =[%s]", cn, npp(r->filename ));
 mod_gzip_printf( "%s: 1 ***: r->content_type=[%s]", cn, npp(r->content_type ));
 mod_gzip_printf( "%s: 1 ***: r->handler     =[%s]", cn, npp(r->handler ));
 #endif

 if ( !r->content_type )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: 'r->content_type' is NULL...",cn);
    mod_gzip_printf( "%s: Performing 'quick lookup'...",cn);
    mod_gzip_printf( "%s: Call mod_gzip_run_handlers(r,RUN_TYPE_CHECKERS)...",cn);
    #endif

    rc = mod_gzip_run_handlers( r, MOD_GZIP_RUN_TYPE_CHECKERS );

    #ifdef MOD_GZIP_DEBUG1

    mod_gzip_printf( "%s: Back mod_gzip_run_handlers(r,RUN_TYPE_CHECKERS)...",cn);

    if ( rc == OK )
      {
       mod_gzip_printf( "%s: rc = %d = OK",cn, rc );
      }
    else if ( rc == DECLINED )
      {
       mod_gzip_printf( "%s: rc = %d = DECLINED",cn, rc );
      }
    else if ( rc == DONE )
      {
       mod_gzip_printf( "%s: rc = %d = DONE",cn, rc );
      }
    else
      {
       mod_gzip_printf( "%s: rc = %d = HTTP_ERROR?",cn, rc );
      }

    #endif
   }
 #ifdef MOD_GZIP_DEBUG1
 else
   {
    mod_gzip_printf( "%s: 'r->content_type' is VALID already...",cn);
    mod_gzip_printf( "%s: No 'quick lookup' was performed...",cn);
   }
 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: 2 ***: r->uri         =[%s]", cn, npp(r->uri ));
 mod_gzip_printf( "%s: 2 ***: r->unparsed_uri=[%s]", cn, npp(r->unparsed_uri ));
 mod_gzip_printf( "%s: 2 ***: r->filename    =[%s]", cn, npp(r->filename ));
 mod_gzip_printf( "%s: 2 ***: r->content_type=[%s]", cn, npp(r->content_type ));
 mod_gzip_printf( "%s: 2 ***: r->handler     =[%s]", cn, npp(r->handler ));
 mod_gzip_printf( "%s: Call mod_gzip_validate1()...",cn);
 #endif

 action_flag =
 mod_gzip_validate1(
 (request_rec   *) r,
 (mod_gzip_conf *) dconf,
 (char *) r->filename,
 (char *) r->uri,
 (char *) r->content_type,
 (char *) r->handler,
 NULL, /* Field key    (Not used here) */
 NULL, /* Field string (Not used here) */
 0     /* Direction    (Not used here) */
 );

 #ifdef MOD_GZIP_DEBUG1

 mod_gzip_printf( "%s: Back mod_gzip_validate1()...",cn);
 mod_gzip_printf( "%s: action_flag  = %d",cn,action_flag);

 if ( action_flag == MOD_GZIP_IMAP_DYNAMIC1 )
   {
    mod_gzip_printf( "%s: action_flag  = MOD_GZIP_IMAP_DYNAMIC1",cn);
   }
 else if ( action_flag == MOD_GZIP_IMAP_DYNAMIC2 )
   {
    mod_gzip_printf( "%s: action_flag  = MOD_GZIP_IMAP_DYNAMIC2",cn);
   }
 else if ( action_flag == MOD_GZIP_IMAP_STATIC1 )
   {
    mod_gzip_printf( "%s: action_flag  = MOD_GZIP_IMAP_STATIC1",cn);
   }
 else if ( action_flag == MOD_GZIP_IMAP_DECLINED1 )
   {
    mod_gzip_printf( "%s: action_flag  = MOD_GZIP_IMAP_DECLINED1",cn);
   }
 else
   {
    mod_gzip_printf( "%s: action_flag  = MOD_GZIP_IMAP_??? Unknown action",cn);
   }

 #endif

 if ( action_flag != MOD_GZIP_IMAP_DECLINED1 )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: This transaction is a valid candidate...",cn);
    mod_gzip_printf( "%s: Saving current r->handler value [%s] to mod_gzip_r_handler note...",
                      cn, npp(r->handler) );
    #endif

    if ( r->handler )
      {
       ap_table_setn( r->notes,"mod_gzip_r_handler",ap_pstrdup(r->pool,r->handler));
      }
    else
      {
       ap_table_setn( r->notes,"mod_gzip_r_handler",ap_pstrdup(r->pool,"0"));
      }

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Forcing r->handler to be 'mod_gzip_handler'...", cn );
    #endif

    r->handler = "mod_gzip_handler";

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: r->handler is now = [%s]", cn, npp(r->handler) );
    mod_gzip_printf( "%s: Exit > return( OK ) >", cn );
    mod_gzip_printf( " " );
    #endif

    return OK;
   }
 else
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: This transaction is NOT a valid candidate...",cn);
    #endif

    #ifdef MOD_GZIP_USES_APACHE_LOGS
    ap_table_setn( r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"DECLINED:EXCLUDED"));
    #endif

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Exit > return( DECLINED ) >", cn );
    mod_gzip_printf( " " );
    #endif

    return DECLINED;
   }
}

#ifdef MOD_GZIP_COMMAND_VERSION_USED

int mod_gzip_do_command(
int            this_command, /* MOD_GZIP_COMMAND_XXXX */
request_rec   *r,            /* Request record */
mod_gzip_conf *dconf         /* Directory config pointer */
)
{
 /* Generic command response transmitter... */

 char tmpbuf[2048]; /* Fill/flush as needed. Don't overflow */
 char *tmp=tmpbuf;
 int  tmplen=0;
 char s1[90];

 #ifdef USE_MOD_GZIP_DEBUG1
 char cn[]="mod_gzip.c: mod_gzip_send_html_command_response()";
 #endif

 /* Start... */

 if ( this_command == MOD_GZIP_COMMAND_VERSION )
   {
    mod_gzip_strcpy(s1,"No");

    if ( dconf )
      {
       if ( dconf->is_on == 1 ) mod_gzip_strcpy(s1,"Yes");
      }

    sprintf( tmp,
    "<html><body>"
    "mod_gzip is available...<br>\r\n"
    "mod_gzip_version = %s<br>\r\n"
    "mod_gzip_on = %s<br>\r\n"
    "</body></html>",
    mod_gzip_version,
    s1
    );

    #ifdef MOD_GZIP_USES_APACHE_LOGS
    ap_table_setn( r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"COMMAND:VERSION"));
    #endif
   }
 else
   {
    #ifdef MOD_GZIP_USES_APACHE_LOGS
    ap_table_setn( r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"DECLINED:INVALID_COMMAND"));
    #endif

    return( DECLINED );
   }

 /* Add the length of the response to the output header... */
 /* The third parameter to ap_table_set() MUST be a string. */

 tmplen = strlen( tmp );

 sprintf( s1, "%d", tmplen );

 ap_table_set( r->headers_out, "Content-Length", s1 );

 /* Make sure the content type matches this response... */

 r->content_type = "text/html";

 /* Start a timer for this return trip... */

 ap_soft_timeout( "mod_gzip: mod_gzip_do_command", r );

 #ifdef MOD_GZIP_COMMANDS_USE_LAST_MODIFIED

 /* Set the 'Last modified' stamp to current time/date... */

 ap_set_last_modified(r);

 /* TODO? Add 'no-cache' option(s) to mod_gzip command responses */
 /* so user doesn't have hit reload to get fresh data? This might */
 /* be necessary for static files that are subject to an Apache */
 /* lookup but mod_gzip command results as sent 'fresh' each */
 /* time no matter what so there doesn't seem to be a need for */
 /* any 'Last modified' information. Just pump a 200 + data and */
 /* then turn and burn... */

 #endif /* MOD_GZIP_COMMANDS_USE_LAST_MODIFIED */

 /* Send the HTTP response header... */

 ap_send_http_header(r);

 /* Send the response BODY... */

 ap_send_mmap( tmp, r, 0, tmplen );

 /* Clean up and exit... */

 ap_kill_timeout(r);

 return OK;

}/* End of mod_gzip_send_html_command_response() */

#endif /* MOD_GZIP_COMMAND_VERSION_USED */


static int mod_gzip_handler( request_rec *r )
{
 #ifdef MOD_GZIP_DEBUG1
 char cn[]="mod_gzip_handler()";
 #endif

 int  rc = DECLINED;
 int  action_flag = 0;

 request_rec *r__next=0;

 mod_gzip_conf *sconf = 0;
 mod_gzip_conf *dconf = 0;

 const char *tablestring;

 const char *s1;
 const char *s2;
 const char *s3;
 const char *s4;

 #ifdef MOD_GZIP_CAN_NEGOTIATE
 struct stat statbuf;
 char  *new_uri;
 char  *new_name;
 #endif

 #ifdef MOD_GZIP_DEBUG1

 mod_gzip_server_now = r->server;

 mod_gzip_printf( " " );

 mod_gzip_printf( "%s: ''''Entry...",cn);
 mod_gzip_printf( "%s: r               = %ld", cn,(long)r);
 mod_gzip_printf( "%s: r->main         = %ld", cn,(long)r->main);
 mod_gzip_printf( "%s: r->next         = %ld", cn,(long)r->next);
 mod_gzip_printf( "%s: r->prev         = %ld", cn,(long)r->prev);
 mod_gzip_printf( "%s: r->unparsed_uri = [%s]",cn,npp(r->unparsed_uri));
 mod_gzip_printf( "%s: r->uri          = [%s]",cn,npp(r->uri));
 mod_gzip_printf( "%s: r->filename     = [%s]",cn,npp(r->filename));
 mod_gzip_printf( "%s: r->handler      = [%s]",cn,npp(r->handler));

 #endif

 if ( r->main ) /* SUBREQUEST */
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: r->main is TRUE - This is a subrequest...",cn);
    mod_gzip_printf( "%s: Exit > return( DECLINED ) >",cn);
    #endif

    return DECLINED;
   }
 #ifdef MOD_GZIP_DEBUG1
 else
   {
    mod_gzip_printf( "%s: r->main is FALSE",cn);
    mod_gzip_printf( "%s: This is NOT a subrequest in progress...",cn);
    mod_gzip_printf( "%s: OK to continue...",cn);
   }
 #endif

 if ( r->filename )
   {
    if ( mod_gzip_strendswith( r->filename, ".gz", 1 ) )
      {
       #ifdef MOD_GZIP_USES_APACHE_LOGS
       if ( r->prev )
         {
          /* This is a mod_gzip negotiated .gz static file transmit... */
          ap_table_setn( r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"DECLINED:STATIC_GZ_FOUND"));
         }
       else
         {
          /* This is a direct request from client for a .gz file... */
          ap_table_setn( r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"DECLINED:FEXT_GZ"));
         }
       #endif

       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: r->filename ends with '.gz'...",cn);
       mod_gzip_printf( "%s: Exit > return( DECLINED ) >",cn);
       #endif

       return DECLINED;
      }
    #ifdef MOD_GZIP_DEBUG1
    else
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: r->filename does NOT end with '.gz'...",cn);
       mod_gzip_printf( "%s: OK to continue...",cn);
       #endif
      }
    #endif
   }

 if ( r->prev ) /* REDIRECT */
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: r->prev is TRUE - This is a redirection...",cn);
    #endif

    /* This might be a 'directory' index lookup... */

    tablestring = ap_table_get(r->prev->notes, "mod_gzip_running");

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: r->prev->notes->mod_gzip_running = [%s]",
                      cn,npp(tablestring));
    #endif

    if ( tablestring )
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: 'mod_gzip_running' note FOUND",cn);
       #endif

       if ( *tablestring == '1' )
         {
          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: 'mod_gzip_running' note value is '1'...",cn);
          #endif

          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: ************************************************",cn);
          mod_gzip_printf( "%s: mod_gzip is currently 'running' so we ",cn);
          mod_gzip_printf( "%s: must return DECLINED.",cn);
          mod_gzip_printf( "%s: ************************************************",cn);
          mod_gzip_printf( "%s: IMPORTANT: We must now 'replicate' the note flag",cn);
          mod_gzip_printf( "%s: on this record or next time around r->prev",cn);
          mod_gzip_printf( "%s: note flag check will be FALSE.",cn);
          mod_gzip_printf( "%s: ************************************************",cn);
          #endif

          ap_table_setn(r->notes,"mod_gzip_running",ap_pstrdup(r->pool,"1"));

          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: r->notes 'mod_gzip_running' set with value = '1'",cn);
          mod_gzip_printf( "%s: ************************************************",cn);
          #endif

          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: Exit > return( DECLINED ) >",cn);
          #endif

          return DECLINED;
         }
       #ifdef MOD_GZIP_DEBUG1
       else
         {
          mod_gzip_printf( "%s: 'mod_gzip_running' note value is NOT '1'...",cn);
         }
       #endif
      }
    #ifdef MOD_GZIP_DEBUG1
    else
      {
       mod_gzip_printf( "%s: 'mod_gzip_running' note NOT FOUND",cn);
      }
    #endif
   }
 #ifdef MOD_GZIP_DEBUG1
 else
   {
    mod_gzip_printf( "%s: r->prev is FALSE",cn);
    mod_gzip_printf( "%s: This is NOT a redirection in progress...",cn);
    mod_gzip_printf( "%s: OK to continue...",cn);
   }
 #endif

 sconf = ( mod_gzip_conf * )
 ap_get_module_config( r->server->module_config, &gzip_module );

 dconf = ( mod_gzip_conf * )
 ap_get_module_config( r->per_dir_config, &gzip_module );

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: r->server->server_hostname = [%s]", cn,npp(r->server->server_hostname));
 mod_gzip_printf( "%s: sconf        = %ld", cn,(long)sconf);
 mod_gzip_printf( "%s: sconf->loc   = [%s]",cn,npp(sconf->loc));
 mod_gzip_printf( "%s: sconf->is_on = %ld", cn,(long)sconf->is_on);
 mod_gzip_printf( "%s: dconf        = %ld", cn,(long)dconf);
 mod_gzip_printf( "%s: dconf->loc   = [%s]",cn,npp(dconf->loc));
 mod_gzip_printf( "%s: dconf->is_on = %ld", cn,(long)dconf->is_on);
 #endif

 #ifdef MOD_GZIP_COMMAND_VERSION_USED

 /* NOTE: Certain mod_gzip 'commands' should return a response */
 /* even if mod_gzip is OFF in the current location. Make sure */
 /* the checks for these commands take place BEFORE checking */
 /* the actual mod_gzip on/off status... */

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: dconf->command_version = [%s]",cn,npp(dconf->command_version));
 #endif

 /* Check for mod_gzip commands in the 'r->unparsed_uri' request */
 /* line so the commands can actually be part of query parms that */
 /* follow the '?'. 'r->uri' is simply he URI itself with any/all */
 /* additional query arguments removed already... */

 if ( dconf->command_version[0] != 0 )
   {
    if ( mod_gzip_stringcontains(r->unparsed_uri,dconf->command_version))
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Call mod_gzip_do_command( MOD_GZIP_COMMAND_VERSION, r )...",cn);
       #endif

       /* mod_gzip_do_command() returns the correct command */
       /* response page and (normally) just returns 'OK'... */

       return(
       mod_gzip_do_command(
       MOD_GZIP_COMMAND_VERSION,
       r,
       dconf
       ));
      }
   }

 #endif /* MOD_GZIP_COMMAND_VERSION_USED */

 tablestring = ap_table_get(r->notes, "mod_gzip_r_handler");

 if ( !tablestring )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: r->notes->mod_gzip_r_handler = NOT FOUND",cn);
    mod_gzip_printf( "%s: This transaction will be ignored...",cn);
    mod_gzip_printf( "%s: Exit > return( DECLINED ) >",cn);
    #endif

    return DECLINED;
   }

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: r->notes->mod_gzip_r_handler = FOUND",cn);
 mod_gzip_printf( "%s: r->notes->mod_gzip_r_handler = [%s]",cn,npp(tablestring));
 #endif

 #ifdef MOD_GZIP_USES_APACHE_LOGS
 ap_table_setn( r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"INIT2"));
 #endif

 if ( !dconf->is_on )
   {
    #ifdef MOD_GZIP_USES_APACHE_LOGS
    ap_table_setn( r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"DECLINED:OFF2"));
    #endif

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: 'dconf->is_on' is FALSE",cn);
    mod_gzip_printf( "%s: mod_gzip is not turned ON for this location...",cn);
    mod_gzip_printf( "%s: This transaction will be ignored...",cn);
    mod_gzip_printf( "%s: Exit > return( DECLINED ) >",cn);
    #endif

    return DECLINED;
   }

 if ( *tablestring == '0' )
   {
    r->handler = 0;
   }
 else
   {
    r->handler = tablestring;
   }

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: r->handler set back to = [%s]",cn,npp(r->handler));
 #endif

 /* Verify it (again) in case names have changed... */

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Call mod_gzip_validate1()...",cn);
 #endif

 action_flag =
 mod_gzip_validate1(
 (request_rec   *) r,
 (mod_gzip_conf *) dconf,
 (char *) r->filename,
 (char *) r->uri,
 (char *) r->content_type,
 (char *) r->handler,
 NULL, /* Field key    (Not used here  */
 NULL, /* Field string (Not used here) */
 0     /* Direction    (Not used here) */
 );

 #ifdef MOD_GZIP_DEBUG1

 mod_gzip_printf( "%s: Back mod_gzip_validate1()...",cn);
 mod_gzip_printf( "%s: action_flag  = %d",cn,action_flag);

 if ( action_flag == MOD_GZIP_IMAP_DYNAMIC1 )
   {
    mod_gzip_printf( "%s: action_flag  = MOD_GZIP_IMAP_DYNAMIC1",cn);
   }
 else if ( action_flag == MOD_GZIP_IMAP_DYNAMIC2 )
   {
    mod_gzip_printf( "%s: action_flag  = MOD_GZIP_IMAP_DYNAMIC2",cn);
   }
 else if ( action_flag == MOD_GZIP_IMAP_STATIC1 )
   {
    mod_gzip_printf( "%s: action_flag  = MOD_GZIP_IMAP_STATIC1",cn);
   }
 else if ( action_flag == MOD_GZIP_IMAP_DECLINED1 )
   {
    mod_gzip_printf( "%s: action_flag  = MOD_GZIP_IMAP_DECLINED1",cn);
   }
 else
   {
    mod_gzip_printf( "%s: action_flag  = MOD_GZIP_IMAP_??? Unknown action",cn);
   }

 #endif

 if ( action_flag == MOD_GZIP_IMAP_DECLINED1 )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: This transaction is NOT a valid candidate...",cn);
    mod_gzip_printf( "%s: This transaction will be ignored...",cn);
    #endif

    #ifdef MOD_GZIP_USES_APACHE_LOGS
    ap_table_setn( r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"DECLINED:EXCLUDED_BY_HANDLER"));
    #endif

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Exit > return( DECLINED ) >",cn);
    #endif

    return DECLINED;
   }

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: This transaction is a valid candidate...",cn);
 #endif

 #ifdef MOD_GZIP_CAN_NEGOTIATE

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: dconf->can_negotiate = %d",cn,(int)dconf->can_negotiate);
 #endif

 if ( dconf->can_negotiate == 1 )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: dconf->can_negotiate is TRUE...",cn);
    #endif

    /* Check for a static compressed version of the file requested... */

    new_name = ap_pstrcat(r->pool, r->filename, ".gz", NULL);

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Call stat(new_name=[%s])...",cn,npp(new_name));
    #endif

    if ( stat( new_name, &statbuf) != 0 )
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: .... stat() call FAILED",cn);
       mod_gzip_printf( "%s: OK to continue...",cn);
       #endif
      }
    else
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: .... stat() call SUCCEEDED",cn);
       mod_gzip_printf( "%s: Sending precompressed version of file...",cn);
       mod_gzip_printf( "%s: GZ_REDIRECT: START...",cn);
       #endif

       new_name = ap_pstrcat(r->pool, r->uri, ".gz", NULL);

       if ( r->args != NULL )
         {
          new_uri = ap_pstrcat(r->pool, new_name, "?", r->args, NULL);
         }
       else
         {
          new_uri = ap_pstrcat(r->pool, new_name, NULL );
         }

       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Call ap_internal_redirect(new_uri=[%s])...",
                         cn,npp(new_uri));
       mod_gzip_printf( " " );
       #endif

       ap_internal_redirect( new_uri, r );

       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( " " );
       mod_gzip_printf( "%s: Back ap_internal_redirect(new_uri=[%s])...",
                         cn,npp(new_uri));
       mod_gzip_printf( "%s: GZ_REDIRECT: FINISHED...",cn);
       #endif

       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Exit > return( OK ) >",cn);
       #endif

       /* We are about to return OK to end the transaction but go */
       /* ahead and make the mod_gzip final result a DECLINED */
       /* condition since that's what actually happened... */

       #ifdef MOD_GZIP_USES_APACHE_LOGS
       ap_table_setn( r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"DECLINED:STATIC_GZ_FOUND"));
       #endif

       return OK;
      }
   }
 #ifdef MOD_GZIP_DEBUG1
 else
   {
    mod_gzip_printf( "%s: dconf->can_negotiate is FALSE",cn);
   }
 #endif

 #endif /* MOD_GZIP_CAN_NEGOTIATE */

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Call mod_gzip_redir1_handler( r, dconf )...",cn);
 #endif

 rc = (int) mod_gzip_redir1_handler( r, dconf );

 #ifdef MOD_GZIP_DEBUG1

 mod_gzip_printf( "%s: Back mod_gzip_redir1_handler( r, dconf )...",cn);

 if ( rc == OK )
   {
    mod_gzip_printf( "%s: rc = %d OK", cn, (int) rc);
   }
 else if ( rc == DECLINED )
   {
    mod_gzip_printf( "%s: rc = %d DECLINED", cn, (int) rc );
   }
 else
   {
    mod_gzip_printf( "%s: rc = %d ( HTTP ERROR CODE? )", cn, (int) rc );
   }

 #endif

 if ( rc != OK )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: mod_gzip_redir1_handler() call FAILED...",cn);
    #endif

    #ifdef MOD_GZIP_USES_APACHE_LOGS

    ap_table_setn(
    r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"RECOVERY"));
    #endif

    ap_log_error( APLOG_MARK,APLOG_NOERRNO|APLOG_WARNING, r->server,
    "mod_gzip: RECOVERY [%s]", r->the_request );

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: RECOVERY_REDIRECT: START...",cn);
    mod_gzip_printf( "%s: Call ap_internal_redirect(r->unparsed_uri=[%s])...",
                      cn,npp(r->unparsed_uri));
    mod_gzip_printf( " " );
    #endif

    ap_internal_redirect( r->unparsed_uri, r );

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( " " );
    mod_gzip_printf( "%s: Back ap_internal_redirect(r->unparsed_uri=[%s])...",
                      cn,npp(r->unparsed_uri));
    mod_gzip_printf( "%s: RECOVERY_REDIRECT: FINISHED...",cn);
    #endif

    rc = OK;
   }

 #ifdef MOD_GZIP_USES_APACHE_LOGS

 if ( r->next )
   {
    r__next = r->next;

    s1 = ap_table_get( r->notes, "mod_gzip_result" );
    s2 = ap_table_get( r->notes, "mod_gzip_input_size" );
    s3 = ap_table_get( r->notes, "mod_gzip_output_size" );
    s4 = ap_table_get( r->notes, "mod_gzip_compression_ratio" );

    while( r__next )
      {
       if ( s1 ) ap_table_setn( r__next->notes,"mod_gzip_result",ap_pstrdup(r__next->pool,s1));
       if ( s2 ) ap_table_setn( r__next->notes,"mod_gzip_input_size",ap_pstrdup(r__next->pool,s2));
       if ( s3 ) ap_table_setn( r__next->notes,"mod_gzip_output_size",ap_pstrdup(r__next->pool,s3));
       if ( s4 ) ap_table_setn( r__next->notes,"mod_gzip_compression_ratio",ap_pstrdup(r__next->pool,s4));

       r__next = r__next->next;
      }
   }

 #endif

 #ifdef MOD_GZIP_DEBUG1
 #ifdef MOD_GZIP_DEBUG1_SHOW_REQUEST_RECORD2
 mod_gzip_show_request_record( r, cn );
 #endif
 #endif

 #ifdef MOD_GZIP_DEBUG1

 mod_gzip_printf( "%s: 1 r->connection->client->outcnt     = %ld",
                   cn,   r->connection->client->outcnt );
 mod_gzip_printf( "%s: 1 r->connection->client->bytes_sent = %ld",
                   cn,   r->connection->client->bytes_sent );
 mod_gzip_printf( "%s: 1 Sum of the 2......................= %ld",
                   cn,   r->connection->client->outcnt +
                         r->connection->client->bytes_sent );
 if ( rc == OK )
   {
    mod_gzip_printf( "%s: Exit > return ( rc = %d OK ) >",cn,(int)rc);
   }
 else if ( rc == DECLINED )
   {
    mod_gzip_printf( "%s: Exit > return ( rc = %d DECLINED ) >",cn,(int)rc);
   }
 else
   {
    mod_gzip_printf( "%s: Exit > return ( rc = %d HTTP_ERROR ) >",cn,(int)rc);
   }

 #endif

 return rc;
}

int mod_gzip_set_defaults1( mod_gzip_conf *cfg )
{
 int i;

 cfg->is_on                  = 0;    
 cfg->is_on_set              = 0;

 cfg->keep_workfiles         = 0;    
 cfg->keep_workfiles_set     = 0;

 cfg->add_header_count       = 0;    
 cfg->add_header_count_set   = 0;

 cfg->dechunk                = 0;    
 cfg->dechunk_set            = 0;

 cfg->min_http               = 0;    
 cfg->min_http_set           = 0;

 cfg->minimum_file_size      = 300;  
 cfg->minimum_file_size_set  = 0;

 cfg->maximum_file_size      = 0;    
 cfg->maximum_file_size_set  = 0;

 cfg->maximum_inmem_size     = 0;    
 cfg->maximum_inmem_size_set = 0;

 #ifdef WIN32
 mod_gzip_strcpy( cfg->temp_dir, "c:\\temp\\" );
 #else
 mod_gzip_strcpy( cfg->temp_dir, "/tmp/" );
 #endif
 cfg->temp_dir_set           = 0;

 cfg->imap_total_entries     = 0;
 cfg->imap_total_ismime      = 0;
 cfg->imap_total_isfile      = 0;
 cfg->imap_total_isuri       = 0;
 cfg->imap_total_ishandler   = 0;
 cfg->imap_total_isreqheader = 0;
 cfg->imap_total_isrspheader = 0;

 for ( i=0; i<MOD_GZIP_IMAP_MAXNAMES; i++ )
    {
     memset( &(cfg->imap[i]), 0, mod_gzip_imap_size );
    }

 #ifdef MOD_GZIP_COMMAND_VERSION_USED
 memset(
 cfg->command_version, 0, MOD_GZIP_COMMAND_VERSION_MAXLEN );
 cfg->command_version_set = 0;
 #endif

 #ifdef MOD_GZIP_CAN_NEGOTIATE
 cfg->can_negotiate     = 0;
 cfg->can_negotiate_set = 0;
 #endif

 return 0;
}

#ifdef REFERENCE
static void *mod_gzip_merge_dconfig(
pool *p,
void *parent_conf,
void *newloc_conf
)
{
 mod_gzip_conf *merged_config = (mod_gzip_conf *) ap_pcalloc(p, sizeof(mod_gzip_conf));
 mod_gzip_conf *pconf         = (mod_gzip_conf *) parent_conf;
 mod_gzip_conf *nconf         = (mod_gzip_conf *) newloc_conf;

 mod_gzip_merge1(
 ( pool          * ) p,
 ( mod_gzip_conf * ) merged_config,
 ( mod_gzip_conf * ) pconf,
 ( mod_gzip_conf * ) nconf
 );

 return (void *) merged_config;
}
#endif

int mod_gzip_merge1(
pool          *p,
mod_gzip_conf *merged_config,
mod_gzip_conf *pconf,
mod_gzip_conf *nconf )
{
 #ifdef MOD_GZIP_DEBUG1
 char cn[]="mod_gzip_merge1():::::::";
 char ch1 = 0; 
 #endif

 char *p1    = 0; 
 char *p2    = 0; 
 int   i     = 0; 
 int   ii    = 0; 
 int   l1    = 0; 
 int   l2    = 0; 
 int   match = 0; 

 int   total             = 0; 
 int   total_ismime      = 0;
 int   total_isfile      = 0;
 int   total_isuri       = 0;
 int   total_ishandler   = 0;
 int   total_isreqheader = 0;
 int   total_isrspheader = 0;

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: ",cn); 
 #endif

 #ifdef MOD_GZIP_DEBUG1

 if ( nconf->is_on_set ) 
   {
    merged_config->is_on = nconf->is_on;

    ch1='!'; 
   }
 else 
   {
    merged_config->is_on = pconf->is_on;

    ch1='='; 
   }

 mod_gzip_printf( "%s: pconf %c= nconf : merged_config->is_on              = %ld", cn, ch1, (long) merged_config->is_on );

 #else 

 merged_config->is_on =
 ( nconf->is_on_set) ? nconf->is_on : pconf->is_on;

 #endif 

 #ifdef MOD_GZIP_DEBUG1

 if ( pconf->cmode == nconf->cmode )
   {
    merged_config->cmode = pconf->cmode;

    ch1='='; 
   }
 else 
   {
    merged_config->cmode = MOD_GZIP_CONFIG_MODE_COMBO;

    ch1='!'; 
   }

 mod_gzip_printf( "%s: pconf %c= nconf : merged_config->cmode              = %ld", cn, ch1, (long) merged_config->cmode );

 #else 

 merged_config->cmode =
 (pconf->cmode == nconf->cmode) ? pconf->cmode : MOD_GZIP_CONFIG_MODE_COMBO;

 #endif 

 merged_config->loc = ap_pstrdup( p, nconf->loc );

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: .............. : merged_config->loc                = [%s]", cn, npp(merged_config->loc));
 #endif

 #ifdef MOD_GZIP_DEBUG1
 if ( !nconf->add_header_count_set ) 
      { merged_config->add_header_count = pconf->add_header_count; ch1='='; }
 else { merged_config->add_header_count = nconf->add_header_count; ch1='!'; }
 mod_gzip_printf( "%s: pconf %c= nconf : merged_config->add_header_count   = %ld", cn, ch1, (long) merged_config->add_header_count );
 #else
 merged_config->add_header_count = ( !nconf->add_header_count_set ) ? pconf->add_header_count : nconf->add_header_count;
 #endif

 #ifdef MOD_GZIP_DEBUG1
 if ( !nconf->keep_workfiles_set ) 
      { merged_config->keep_workfiles = pconf->keep_workfiles; ch1='='; }
 else { merged_config->keep_workfiles = nconf->keep_workfiles; ch1='!'; }
 mod_gzip_printf( "%s: pconf %c= nconf : merged_config->keep_workfiles     = %ld", cn, ch1, (long) merged_config->keep_workfiles );
 #else
 merged_config->keep_workfiles = ( !nconf->keep_workfiles_set ) ? pconf->keep_workfiles : nconf->keep_workfiles;
 #endif

 #ifdef MOD_GZIP_CAN_NEGOTIATE
 #ifdef MOD_GZIP_DEBUG1
 if ( !nconf->can_negotiate_set )
      { merged_config->can_negotiate = pconf->can_negotiate; ch1='='; }
 else { merged_config->can_negotiate = nconf->can_negotiate; ch1='!'; }
 mod_gzip_printf( "%s: pconf %c= nconf : merged_config->can_negotiate      = %ld", cn, ch1, (long) merged_config->can_negotiate );
 #else
 merged_config->can_negotiate = ( !nconf->can_negotiate_set ) ? pconf->can_negotiate : nconf->can_negotiate;
 #endif
 #endif

 #ifdef MOD_GZIP_DEBUG1
 if ( !nconf->dechunk_set ) 
      { merged_config->dechunk = pconf->dechunk; ch1='='; }
 else { merged_config->dechunk = nconf->dechunk; ch1='!'; }
 mod_gzip_printf( "%s: pconf %c= nconf : merged_config->dechunk            = %ld", cn, ch1, (long) merged_config->dechunk );
 #else
 merged_config->dechunk = ( !nconf->dechunk_set ) ? pconf->dechunk : nconf->dechunk;
 #endif

 #ifdef MOD_GZIP_DEBUG1
 if ( !nconf->min_http_set ) 
      { merged_config->min_http = pconf->min_http; ch1='='; }
 else { merged_config->min_http = nconf->min_http; ch1='!'; }
 mod_gzip_printf( "%s: pconf %c= nconf : merged_config->min_http           = %ld", cn, ch1, (long) merged_config->min_http );
 #else
 merged_config->min_http = ( !nconf->min_http_set ) ? pconf->min_http : nconf->min_http;
 #endif

 #ifdef MOD_GZIP_DEBUG1
 if ( !nconf->minimum_file_size_set ) 
      { merged_config->minimum_file_size = pconf->minimum_file_size; ch1='='; }
 else { merged_config->minimum_file_size = nconf->minimum_file_size; ch1='!'; }
 mod_gzip_printf( "%s: pconf %c= nconf : merged_config->minimum_file_size  = %ld", cn, ch1, (long) merged_config->minimum_file_size );
 #else
 merged_config->minimum_file_size = ( !nconf->minimum_file_size_set ) ? pconf->minimum_file_size : nconf->minimum_file_size;
 #endif

 #ifdef MOD_GZIP_DEBUG1
 if ( !nconf->maximum_file_size_set ) 
      { merged_config->maximum_file_size = pconf->maximum_file_size; ch1='='; }
 else { merged_config->maximum_file_size = nconf->maximum_file_size; ch1='!'; }
 mod_gzip_printf( "%s: pconf %c= nconf : merged_config->maximum_file_size  = %ld", cn, ch1, (long) merged_config->maximum_file_size );
 #else
 merged_config->maximum_file_size = ( !nconf->maximum_file_size_set ) ? pconf->maximum_file_size : nconf->maximum_file_size;
 #endif

 #ifdef MOD_GZIP_DEBUG1
 if ( !nconf->maximum_inmem_size_set ) 
      { merged_config->maximum_inmem_size = pconf->maximum_inmem_size; ch1='='; }
 else { merged_config->maximum_inmem_size = nconf->maximum_inmem_size; ch1='!'; }
 mod_gzip_printf( "%s: pconf %c= nconf : merged_config->maximum_inmem_size = %ld", cn, ch1, (long) merged_config->maximum_inmem_size );
 #else
 merged_config->maximum_inmem_size = ( !nconf->maximum_inmem_size_set ) ? pconf->maximum_inmem_size : nconf->maximum_inmem_size;
 #endif

 #ifdef MOD_GZIP_DEBUG1
 if ( !nconf->temp_dir_set ) 
      { mod_gzip_strcpy(merged_config->temp_dir,pconf->temp_dir); ch1='='; }
 else { mod_gzip_strcpy(merged_config->temp_dir,nconf->temp_dir); ch1='!'; }
 mod_gzip_printf( "%s: pconf %c= nconf : merged_config->temp_dir           = [%s]", cn, ch1,npp(merged_config->temp_dir));
 #else
 if ( !nconf->temp_dir_set ) 
      { mod_gzip_strcpy(merged_config->temp_dir,pconf->temp_dir); }
 else { mod_gzip_strcpy(merged_config->temp_dir,nconf->temp_dir); }
 #endif

 #ifdef MOD_GZIP_COMMAND_VERSION_USED
 #ifdef MOD_GZIP_DEBUG1
 if ( !nconf->command_version_set )
      { mod_gzip_strcpy(merged_config->command_version,pconf->command_version); ch1='='; }
 else { mod_gzip_strcpy(merged_config->command_version,nconf->command_version); ch1='!'; }
 mod_gzip_printf( "%s: pconf %c= nconf : merged_config->command_version    = [%s]", cn, ch1,npp(merged_config->command_version));
 #else
 if ( !nconf->command_version_set )
      { mod_gzip_strcpy(merged_config->command_version,pconf->command_version); }
 else { mod_gzip_strcpy(merged_config->command_version,nconf->command_version); }
 #endif
 #endif

 total = 0; 

 for ( i=0; i<nconf->imap_total_entries; i++ )
    {
     memcpy(
     &(merged_config->imap[i]),
     &(nconf->imap[i]),
     mod_gzip_imap_size
     );

     total++; 

     if ( nconf->imap[i].type == MOD_GZIP_IMAP_ISMIME )
       {
        total_ismime++;
       }
     else if ( nconf->imap[i].type == MOD_GZIP_IMAP_ISFILE )
       {
        total_isfile++;
       }
     else if ( nconf->imap[i].type == MOD_GZIP_IMAP_ISURI )
       {
        total_isuri++;
       }
     else if ( nconf->imap[i].type == MOD_GZIP_IMAP_ISHANDLER )
       {
        total_ishandler++;
       }
     else if ( nconf->imap[i].type == MOD_GZIP_IMAP_ISREQHEADER )
       {
        total_isreqheader++;
       }
     else if ( nconf->imap[i].type == MOD_GZIP_IMAP_ISRSPHEADER )
       {
        total_isrspheader++;
       }
    }

 for ( i=0; i<pconf->imap_total_entries; i++ )
    {
     p1 = pconf->imap[i].name;
     l1 = mod_gzip_strlen( p1 );

     match = -1; 

     for ( ii=0; ii<nconf->imap_total_entries; ii++ )
        {
         p2 = nconf->imap[ii].name;
         l2 = nconf->imap[ii].namelen;

         if ( l1 == l2 ) 
           {
            if ( mod_gzip_strncmp( p1, p2, l1 ) == 0 )
              {
               match = ii; 

               break; 
              }
           }
        }

     if ( match != -1 )
       {
       }
     else 
       {
        if ( total < MOD_GZIP_IMAP_MAXNAMES ) 
          {
           memcpy(
           &(merged_config->imap[ total ]),
           &(pconf->imap[i]),
           mod_gzip_imap_size
           );

           total++; 

           if ( pconf->imap[i].type == MOD_GZIP_IMAP_ISMIME )
             {
              total_ismime++;
             }
           else if ( pconf->imap[i].type == MOD_GZIP_IMAP_ISFILE )
             {
              total_isfile++;
             }
           else if ( pconf->imap[i].type == MOD_GZIP_IMAP_ISURI )
             {
              total_isuri++;
             }
           else if ( pconf->imap[i].type == MOD_GZIP_IMAP_ISHANDLER )
             {
              total_ishandler++;
             }
           else if ( pconf->imap[i].type == MOD_GZIP_IMAP_ISREQHEADER )
             {
              total_isreqheader++;
             }
           else if ( pconf->imap[i].type == MOD_GZIP_IMAP_ISRSPHEADER )
             {
              total_isrspheader++;
             }
          }
       }
    }

 merged_config->imap_total_entries     = total;
 merged_config->imap_total_ismime      = total_ismime;
 merged_config->imap_total_isfile      = total_isfile;
 merged_config->imap_total_isuri       = total_isuri;
 merged_config->imap_total_ishandler   = total_ishandler;
 merged_config->imap_total_isreqheader = total_isreqheader;
 merged_config->imap_total_isrspheader = total_isrspheader;

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: pconf += nconf : merged_config->imap_total_entries     = %ld", cn, (long) merged_config->imap_total_entries );
 mod_gzip_printf( "%s: pconf += nconf : merged_config->imap_total_ismime      = %ld", cn, (long) merged_config->imap_total_ismime );
 mod_gzip_printf( "%s: pconf += nconf : merged_config->imap_total_isfile      = %ld", cn, (long) merged_config->imap_total_isfile );
 mod_gzip_printf( "%s: pconf += nconf : merged_config->imap_total_isuri       = %ld", cn, (long) merged_config->imap_total_isuri );
 mod_gzip_printf( "%s: pconf += nconf : merged_config->imap_total_ishandler   = %ld", cn, (long) merged_config->imap_total_ishandler );
 mod_gzip_printf( "%s: pconf += nconf : merged_config->imap_total_isreqheader = %ld", cn, (long) merged_config->imap_total_isreqheader );
 mod_gzip_printf( "%s: pconf += nconf : merged_config->imap_total_isrspheader = %ld", cn, (long) merged_config->imap_total_isrspheader );
 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: ",cn); 
 #endif

 return 0;
}

static const char *
mod_gzip_set_is_on( cmd_parms *parms, void *cfg, char *arg )
{
 mod_gzip_conf *mgc;

 #ifdef MOD_GZIP_DEBUG1
 server_rec *srv = parms->server;
 char cn[]="mod_gzip_set_is_on()";
 #endif

 #ifdef MOD_GZIP_DEBUG1

 mod_gzip_server_now = srv;

 mod_gzip_printf( " ");
 mod_gzip_printf( "%s: Entry", cn );
 mod_gzip_printf( "%s: arg=[%s]", cn, npp(arg) );

 #endif

 mgc = ( mod_gzip_conf * ) cfg;

 #ifdef MOD_GZIP_ALTERNATIVE1

 mgc = ( mod_gzip_conf * )
 ap_get_module_config(parms->server->module_config, &gzip_module);
 #endif

 if ( ( arg[0] == 'Y' )||( arg[0] == 'y' ) )
   {
    mgc->is_on = 1;
   }
 else
   {
    mgc->is_on = 0;
   }

 mgc->is_on_set = 1;

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: mgc->loc             = [%s]", cn, npp(mgc->loc));
 mod_gzip_printf( "%s: srv->is_virtual      = %ld",  cn, (long)srv->is_virtual );
 mod_gzip_printf( "%s: srv->server_hostname = [%s]", cn, npp(srv->server_hostname));
 mod_gzip_printf( "%s: mgc->cmode           = %ld",  cn, (long) mgc->cmode );
 mod_gzip_printf( "%s: mgc->is_on           = %ld",  cn, (long) mgc->is_on );
 mod_gzip_printf( "%s: mgc->is_on_set       = %ld",  cn, (long) mgc->is_on_set );
 #endif

 return NULL;
}

static const char *
mod_gzip_set_add_header_count( cmd_parms *parms, void *cfg, char *arg )
{
 mod_gzip_conf *mgc;

 #ifdef MOD_GZIP_DEBUG1
 server_rec *srv = parms->server;
 char cn[]="mod_gzip_set_add_header_count()";
 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_server_now = srv;
 mod_gzip_printf( " ");
 mod_gzip_printf( "%s: Entry", cn );
 mod_gzip_printf( "%s: arg=[%s]", cn, npp(arg) );
 #endif

 mgc = ( mod_gzip_conf * ) cfg;

 if ( ( arg[0] == 'Y' )||( arg[0] == 'y' ) )
   {
    mgc->add_header_count = 1;
   }
 else
   {
    mgc->add_header_count = 0;
   }

 mgc->add_header_count_set = 1;

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: mgc->loc                  = [%s]",cn, npp(mgc->loc));
 mod_gzip_printf( "%s: mgc->cmode                = %ld", cn, (long) mgc->cmode );
 mod_gzip_printf( "%s: srv->is_virtual           = %ld", cn, (long)srv->is_virtual );
 mod_gzip_printf( "%s: srv->server_hostname      = [%s]",cn, npp(srv->server_hostname));
 mod_gzip_printf( "%s: mgc->add_header_count     = %ld", cn,
                (long) mgc->add_header_count );
 mod_gzip_printf( "%s: mgc->add_header_count_set = %ld", cn,
                (long) mgc->add_header_count_set );
 #endif

 return NULL;
}

static const char *
mod_gzip_set_keep_workfiles( cmd_parms *parms, void *cfg, char *arg )
{
 mod_gzip_conf *mgc;

 #ifdef MOD_GZIP_DEBUG1
 server_rec *srv = parms->server;
 char cn[]="mod_gzip_set_keep_workfiles()";
 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_server_now = srv;
 mod_gzip_printf( " ");
 mod_gzip_printf( "%s: Entry", cn );
 mod_gzip_printf( "%s: arg=[%s]", cn, npp(arg) );
 #endif

 mgc = ( mod_gzip_conf * ) cfg;

 if ( ( arg[0] == 'Y' )||( arg[0] == 'y' ) )
   {
    mgc->keep_workfiles = 1;
   }
 else
   {
    mgc->keep_workfiles = 0;
   }

 mgc->keep_workfiles_set = 1;

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: mgc->loc                = [%s]",cn, npp(mgc->loc) );
 mod_gzip_printf( "%s: mgc->cmode              = %ld", cn, (long) mgc->cmode );
 mod_gzip_printf( "%s: srv->is_virtual         = %ld", cn, (long)srv->is_virtual );
 mod_gzip_printf( "%s: srv->server_hostname    = [%s]",cn, npp(srv->server_hostname) );
 mod_gzip_printf( "%s: mgc->keep_workfiles     = %ld", cn,
                (long) mgc->keep_workfiles );
 mod_gzip_printf( "%s: mgc->keep_workfiles_set = %ld", cn,
                (long) mgc->keep_workfiles_set );
 #endif

 return NULL;
}

#ifdef MOD_GZIP_CAN_NEGOTIATE
static const char *
mod_gzip_set_can_negotiate( cmd_parms *parms, void *cfg, char *arg )
{
 mod_gzip_conf *mgc;

 #ifdef MOD_GZIP_DEBUG1
 server_rec *srv = parms->server;
 char cn[]="mod_gzip_set_can_negotiate()";
 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_server_now = srv;
 mod_gzip_printf( " ");
 mod_gzip_printf( "%s: Entry", cn );
 mod_gzip_printf( "%s: arg=[%s]", cn, npp(arg) );
 #endif

 mgc = ( mod_gzip_conf * ) cfg;

 if ( ( arg[0] == 'Y' )||( arg[0] == 'y' ) )
   {
    mgc->can_negotiate = 1;
   }
 else
   {
    mgc->can_negotiate = 0;
   }

 mgc->can_negotiate_set = 1;

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: mgc->loc                = [%s]",cn, npp(mgc->loc) );
 mod_gzip_printf( "%s: mgc->cmode              = %ld", cn, (long) mgc->cmode );
 mod_gzip_printf( "%s: srv->is_virtual         = %ld", cn, (long)srv->is_virtual );
 mod_gzip_printf( "%s: srv->server_hostname    = [%s]",cn, npp(srv->server_hostname) );
 mod_gzip_printf( "%s: mgc->can_negotiate      = %ld", cn,
                (long) mgc->can_negotiate );
 mod_gzip_printf( "%s: mgc->can_negotiate_set  = %ld", cn,
                (long) mgc->can_negotiate_set );
 #endif

 return NULL;
}
#endif

static const char *
mod_gzip_set_dechunk( cmd_parms *parms, void *cfg, char *arg )
{
 mod_gzip_conf *mgc;

 #ifdef MOD_GZIP_DEBUG1
 server_rec *srv = parms->server;
 char cn[]="mod_gzip_set_dechunk()";
 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_server_now = srv;
 mod_gzip_printf( " ");
 mod_gzip_printf( "%s: Entry", cn );
 mod_gzip_printf( "%s: arg=[%s]", cn, npp(arg) );
 #endif

 mgc = ( mod_gzip_conf * ) cfg;

 if ( ( arg[0] == 'Y' )||( arg[0] == 'y' ) )
   {
    mgc->dechunk = 1;
   }
 else
   {
    mgc->dechunk = 0;
   }

 mgc->dechunk_set = 1;

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: mgc->loc                = [%s]",cn, npp(mgc->loc) );
 mod_gzip_printf( "%s: mgc->cmode              = %ld", cn, (long) mgc->cmode );
 mod_gzip_printf( "%s: srv->is_virtual         = %ld", cn, (long)srv->is_virtual );
 mod_gzip_printf( "%s: srv->server_hostname    = [%s]",cn, npp(srv->server_hostname) );
 mod_gzip_printf( "%s: mgc->dechunk            = %ld", cn,
                (long) mgc->dechunk );
 mod_gzip_printf( "%s: mgc->dechunk_set        = %ld", cn,
                (long) mgc->dechunk_set );
 #endif

 return NULL;
}

static const char *
mod_gzip_set_min_http( cmd_parms *parms, void *cfg, char *arg )
{
 mod_gzip_conf *mgc;

 #ifdef MOD_GZIP_DEBUG1
 server_rec *srv = parms->server;
 char cn[]="mod_gzip_set_min_http()";
 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_server_now = srv;
 mod_gzip_printf( " ");
 mod_gzip_printf( "%s: Entry", cn );
 mod_gzip_printf( "%s: arg=[%s]", cn, npp(arg) );
 #endif

 mgc = ( mod_gzip_conf * ) cfg;

 mgc->min_http     = (int) atoi( arg );
 mgc->min_http_set = 1;

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: mgc->loc             = [%s]",cn, npp(mgc->loc));
 mod_gzip_printf( "%s: mgc->cmode           = %ld", cn, (long) mgc->cmode );
 mod_gzip_printf( "%s: srv->is_virtual      = %ld", cn, (long)srv->is_virtual );
 mod_gzip_printf( "%s: srv->server_hostname = [%s]",cn, npp(srv->server_hostname));
 mod_gzip_printf( "%s: mgc->min_http        = %ld", cn,
                (long) mgc->min_http );
 mod_gzip_printf( "%s: mgc->min_http        = %ld", cn,
                (long) mgc->min_http_set );
 #endif

 return NULL;
}

static const char *
mod_gzip_set_minimum_file_size( cmd_parms *parms, void *cfg, char *arg )
{
 mod_gzip_conf *mgc;

 #ifdef MOD_GZIP_DEBUG1
 server_rec *srv = parms->server;
 char cn[]="mod_gzip_set_minimum_file_size()";
 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_server_now = srv;
 mod_gzip_printf( " ");
 mod_gzip_printf( "%s: Entry", cn );
 mod_gzip_printf( "%s: arg=[%s]", cn, npp(arg));
 #endif

 mgc = ( mod_gzip_conf * ) cfg;

 mgc->minimum_file_size     = (long) atol( arg );
 mgc->minimum_file_size_set = 1;

 if ( mgc->minimum_file_size < 300 )
   {
    mgc->minimum_file_size = 300;
   }

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: mgc->loc                = [%s]",cn, npp(mgc->loc));
 mod_gzip_printf( "%s: mgc->cmode              = %ld", cn, (long) mgc->cmode );
 mod_gzip_printf( "%s: srv->is_virtual         = %ld", cn, (long)srv->is_virtual );
 mod_gzip_printf( "%s: srv->server_hostname    = [%s]",cn, npp(srv->server_hostname));
 mod_gzip_printf( "%s: mgc->minimum_file_size  = %ld", cn,
                (long) mgc->minimum_file_size );
 mod_gzip_printf( "%s: mgc->minimum_file_size  = %ld", cn,
                (long) mgc->minimum_file_size_set );
 #endif

 return NULL;
}

static const char *
mod_gzip_set_maximum_file_size( cmd_parms *parms, void *cfg, char *arg )
{
 mod_gzip_conf *mgc;

 #ifdef MOD_GZIP_DEBUG1
 server_rec *srv = parms->server;
 char cn[]="mod_gzip_set_maximum_file_size()";
 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_server_now = srv;
 mod_gzip_printf( " ");
 mod_gzip_printf( "%s: Entry", cn );
 mod_gzip_printf( "%s: arg=[%s]", cn, npp(arg));
 #endif

 mgc = ( mod_gzip_conf * ) cfg;

 mgc->maximum_file_size     = (long) atol( arg );
 mgc->maximum_file_size_set = 1;

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: mgc->loc                = [%s]",cn, npp(mgc->loc));
 mod_gzip_printf( "%s: mgc->cmode              = %ld", cn, (long) mgc->cmode );
 mod_gzip_printf( "%s: srv->is_virtual         = %ld", cn, (long)srv->is_virtual );
 mod_gzip_printf( "%s: srv->server_hostname    = [%s]",cn, npp(srv->server_hostname));
 mod_gzip_printf( "%s: mgc->maximum_file_size  = %ld", cn,
                (long) mgc->maximum_file_size );
 mod_gzip_printf( "%s: mgc->maximum_file_size  = %ld", cn,
                (long) mgc->maximum_file_size_set );
 #endif

 return NULL;
}

static const char *
mod_gzip_set_maximum_inmem_size( cmd_parms *parms, void *cfg, char *arg )
{
 mod_gzip_conf *mgc;

 #ifdef MOD_GZIP_DEBUG1
 server_rec *srv = parms->server;
 char cn[]="mod_gzip_set_maximum_inmem_size()";
 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_server_now = srv;
 mod_gzip_printf( " ");
 mod_gzip_printf( "%s: Entry", cn );
 mod_gzip_printf( "%s: arg=[%s]", cn, npp(arg) );
 #endif

 mgc = ( mod_gzip_conf * ) cfg;

 mgc->maximum_inmem_size     = (long) atol(arg);
 mgc->maximum_inmem_size_set = 1;

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: mgc->loc                = [%s]",cn, npp(mgc->loc));
 mod_gzip_printf( "%s: mgc->cmode              = %ld", cn, (long) mgc->cmode );
 mod_gzip_printf( "%s: srv->is_virtual         = %ld", cn, (long)srv->is_virtual );
 mod_gzip_printf( "%s: srv->server_hostname    = [%s]",cn, npp(srv->server_hostname));
 mod_gzip_printf( "%s: mgc->maximum_inmem_size = %ld", cn,
                (long) mgc->maximum_inmem_size );
 mod_gzip_printf( "%s: mgc->maximum_inmem_size = %ld", cn,
                (long) mgc->maximum_inmem_size_set );
 #endif

 return NULL;
}

static const char *
mod_gzip_set_temp_dir( cmd_parms *parms, void *cfg, char *arg )
{
 mod_gzip_conf *mgc;

 #ifdef MOD_GZIP_DEBUG1
 server_rec *srv = parms->server;
 char cn[]="mod_gzip_set_temp_dir()";
 #endif

 int arglen = 0;
 int rc     = 0;

 struct stat sbuf;

 #ifdef WIN32
 char dirsep[]="\\";
 #else
 char dirsep[]="/";
 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_server_now = srv;
 mod_gzip_printf( " ");
 mod_gzip_printf( "%s: Entry", cn );
 #endif

 if ( !arg )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: ERROR: 'arg' is NULL...", cn );
    mod_gzip_printf( "%s: ERROR: No valid directory supplied.", cn );
    #endif

    return "mod_gzip_temp_dir: ERROR: No valid directory supplied.";
   }

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: arg=[%s]", cn, npp(arg) );
 #endif

 mgc = ( mod_gzip_conf * ) cfg;

 arglen = mod_gzip_strlen( arg );

 if ( arglen < 256 )
   {
    mod_gzip_strcpy( mgc->temp_dir, arg );
    mgc->temp_dir_set = 1;

    if ( arglen > 0 )
      {
       if (( arglen == 1 ) && ( *arg == 32 ))
         {
          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: Special ONE SPACE pickup seen.", cn );
          mod_gzip_printf( "%s: temp_dir set to NOTHING.", cn );
          #endif

          mod_gzip_strcpy( mgc->temp_dir, "" );
         }
       else
         {
          if ( ( *(mgc->temp_dir+(arglen-1)) != '\\' ) &&
               ( *(mgc->temp_dir+(arglen-1)) != '/'  ) )
            {
             mod_gzip_strcat( mgc->temp_dir, dirsep );
            }

          rc = stat( mgc->temp_dir, &sbuf );

          if ( rc )
            {
             #ifdef MOD_GZIP_DEBUG1
             mod_gzip_printf( "%s: .... stat() call FAILED",cn);
             mod_gzip_printf( "%s: Directory name is not valid.",cn);
             #endif

             return "mod_gzip_temp_dir: ERROR: Directory does not exist.";
            }
          else
            {
             #ifdef MOD_GZIP_DEBUG1
             mod_gzip_printf( "%s: .... stat() call SUCCEEDED",cn);
             mod_gzip_printf( "%s: Directory name appears to be valid.",cn);
             #endif
            }
         }
      }

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: mgc->loc             = [%s]", cn, npp(mgc->loc));
    mod_gzip_printf( "%s: srv->is_virtual      = %ld",  cn, (long)srv->is_virtual );
    mod_gzip_printf( "%s: srv->server_hostname = [%s]", cn, npp(srv->server_hostname));
    mod_gzip_printf( "%s: mgc->cmode           = %ld",  cn, (long) mgc->cmode);
    mod_gzip_printf( "%s: mgc->temp_dir        = [%s]", cn, npp(mgc->temp_dir));
    mod_gzip_printf( "%s: mgc->temp_dir_set    = %d",   cn, mgc->temp_dir_set);
    #endif

    return NULL;
   }
 else
   {
    return "mod_gzip_temp_dir pathname must be less than 256 characters.";
   }
}

#ifdef MOD_GZIP_COMMAND_VERSION_USED
static const char *
mod_gzip_set_command_version( cmd_parms *parms, void *cfg, char *arg )
{
 mod_gzip_conf *mgc;

 #ifdef MOD_GZIP_DEBUG1
 server_rec *srv = parms->server;
 char cn[]="mod_gzip_set_command_version()";
 #endif

 int arglen = 0;

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_server_now = srv;
 mod_gzip_printf( " ");
 mod_gzip_printf( "%s: Entry", cn );
 #endif

 if ( !arg )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: ERROR: 'arg' is NULL...", cn );
    mod_gzip_printf( "%s: ERROR: No valid string supplied.", cn );
    #endif

    return "mod_gzip_command_version: ERROR: No valid string supplied.";
   }

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: arg=[%s]", cn, npp(arg) );
 #endif

 mgc = ( mod_gzip_conf * ) cfg;

 arglen = mod_gzip_strlen( arg );

 if ( arglen < MOD_GZIP_COMMAND_VERSION_MAXLEN )
   {
    mod_gzip_strcpy( mgc->command_version, arg );
    mgc->command_version_set = 1;

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: mgc->loc                 = [%s]", cn, npp(mgc->loc));
    mod_gzip_printf( "%s: srv->is_virtual          = %ld",  cn, (long)srv->is_virtual );
    mod_gzip_printf( "%s: srv->server_hostname     = [%s]", cn, npp(srv->server_hostname));
    mod_gzip_printf( "%s: mgc->cmode               = %ld",  cn, (long) mgc->cmode);
    mod_gzip_printf( "%s: mgc->command_version     = [%s]", cn, npp(mgc->command_version));
    mod_gzip_printf( "%s: mgc->command_version_set = %d",   cn, mgc->command_version_set);
    #endif

    return NULL;
   }
 else
   {
    return "mod_gzip_command_version string must be less than 128 characters.";
   }
}
#endif

static const char *
mod_gzip_imap_add_item(
cmd_parms     *parms,
mod_gzip_conf *mgc,
char          *a1,
char          *a2,
int            flag1
)
{
 int      x;
 char    *p1;
 int      ignorecase=0;

 int      this_include=flag1;
 int      this_type=0;
 int      this_action=0;
 int      this_direction=0;
 unsigned this_port=0;
 int      this_len1=0;
 regex_t *this_pregex=NULL;

 char    *regex;

 /* Diagnostic only
 #define MOD_GZIP_TEST_REGEX1
 */
 #ifdef  MOD_GZIP_TEST_REGEX1
 char string[129];
 int  regex_error;
 #endif

 #ifdef MOD_GZIP_DEBUG1
 char cn[]="mod_gzip_imap_add_item()";
 #endif

 #ifdef MOD_GZIP_DEBUG1

 mod_gzip_printf( "%s: Entry", cn );
 mod_gzip_printf( "%s: 1 a1=[%s]", cn, npp(a1));
 mod_gzip_printf( "%s: 1 a2=[%s]", cn, npp(a2));

 mod_gzip_printf( "%s: mgc->imap_total_entries     = %d",
                   cn, mgc->imap_total_entries );
 mod_gzip_printf( "%s: mgc->imap_total_ismime      = %d",
                   cn, mgc->imap_total_ismime );
 mod_gzip_printf( "%s: mgc->imap_total_isfile      = %d",
                   cn, mgc->imap_total_isfile );
 mod_gzip_printf( "%s: mgc->imap_total_isuri       = %d",
                   cn, mgc->imap_total_isuri );
 mod_gzip_printf( "%s: mgc->imap_total_ishandler   = %d",
                   cn, mgc->imap_total_ishandler );
 mod_gzip_printf( "%s: mgc->imap_total_isreqheader = %d",
                   cn, mgc->imap_total_isreqheader );
 mod_gzip_printf( "%s: mgc->imap_total_isrspheader = %d",
                   cn, mgc->imap_total_isrspheader );

 if ( flag1 == 1 )
   {
    mod_gzip_printf( "%s: flag1 = %d = INCLUDE", cn, flag1 );
   }
 else if ( flag1 == 0 )
   {
    mod_gzip_printf( "%s: flag1 = %d = EXCLUDE", cn, flag1 );
   }
 else
   {
    mod_gzip_printf( "%s: flag1 = %d = ??? Unknown value", cn, flag1 );
   }

 #endif

 this_type = MOD_GZIP_IMAP_ISNONE;

 if ( mod_gzip_strnicmp( a1, "mime", 4 ) == 0 )
   {
    this_type = MOD_GZIP_IMAP_ISMIME;

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: this_type = MOD_GZIP_IMAP_ISMIME", cn);
    #endif
   }
 else if ( mod_gzip_strnicmp( a1, "file", 4 ) == 0 )
   {
    this_type = MOD_GZIP_IMAP_ISFILE;

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: this_type = MOD_GZIP_IMAP_ISFILE",cn);
    #endif
   }
 else if ( mod_gzip_strnicmp( a1, "ur", 2 ) == 0 )
   {
    /* Allow user to specify EITHER 'uri' or 'url' for this 'type' */

    this_type = MOD_GZIP_IMAP_ISURI;

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: this_type = MOD_GZIP_IMAP_ISURI",cn);
    #endif
   }
 else if ( mod_gzip_strnicmp( a1, "hand", 4 ) == 0 )
   {
    this_type = MOD_GZIP_IMAP_ISHANDLER;

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: this_type = MOD_GZIP_IMAP_ISHANDLER",cn);
    #endif
   }
 else if ( mod_gzip_strnicmp( a1, "reqh", 4 ) == 0 )
   {
    this_type      = MOD_GZIP_IMAP_ISREQHEADER;
    this_direction = MOD_GZIP_REQUEST;

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: this_type      = MOD_GZIP_IMAP_ISREQHEADER",cn);
    #endif
   }
 else if ( mod_gzip_strnicmp( a1, "rsph", 4 ) == 0 )
   {
    this_type      = MOD_GZIP_IMAP_ISRSPHEADER;
    this_direction = MOD_GZIP_RESPONSE;

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: this_type      = MOD_GZIP_IMAP_ISRSPHEADER",cn);
    #endif
   }

 if ( this_type == MOD_GZIP_IMAP_ISNONE )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: this_type = ?? UNKNOWN ??",cn);
    mod_gzip_printf( "%s: Exit > return( ERRORSTRING ) >",cn);
    #endif

    return "mod_gzip: ERROR: Valid item types are mime,file,uri,handler,reqheader or rspheader";
   }

 p1 = a2;

 if ( ( this_type == MOD_GZIP_IMAP_ISREQHEADER ) ||
      ( this_type == MOD_GZIP_IMAP_ISRSPHEADER ) )
   {
    while((*p1!=0)&&(*p1!=':')) { p1++; this_len1++; }

    if (*p1==':')
      {
       if ( this_len1 < 1 )
         {
          return "mod_gzip: ERROR: Missing HTTP field name.";
         }

       p1++;
      }
    else
      {
       return "mod_gzip: ERROR: Missing HTTP field name. No colon found.";
      }

    while((*p1!=0)&&(*p1<33)) p1++;
   }

 regex = p1;

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: regex = [%s]", cn, npp(regex) );
 #endif

 if ( !*regex )
   {
    return "mod_gzip: ERROR: Missing regular expression string.";
   }

 ignorecase = 1;

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: ignorecase = %d",cn,ignorecase);
 mod_gzip_printf( "%s: Call ap_pregcomp(%s)...",cn,npp(regex));
 #endif

 this_pregex =
 ap_pregcomp(parms->pool, regex,
 (REG_EXTENDED | REG_NOSUB
 | (ignorecase ? REG_ICASE : 0)));

 if ( this_pregex == NULL )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: .... ap_pregcomp(%s) FAILED...",cn,npp(regex));
    mod_gzip_printf( "%s: regex 'pre-compile' FAILED...", cn );
    mod_gzip_printf( "%s: Exit > return( ERRORSTRING ) >",cn);
    #endif

    return "mod_gzip: ERROR: Regular expression compile failed.";
   }

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: .... ap_pregcomp(%s) SUCCEEDED...",cn,npp(regex));
 mod_gzip_printf( "%s: regex 'pre-compiled' OK", cn );
 #endif

 #ifdef MOD_GZIP_TEST_REGEX1

 if ( ( this_type == MOD_GZIP_IMAP_ISREQHEADER ) ||
      ( this_type == MOD_GZIP_IMAP_ISRSPHEADER ) )
   {
    mod_gzip_strcpy(
    string,
    "Mozilla/4.0 (compatible; MSIE 5.0; Windows NT; DigExt; TUCOWS)"
    );

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Call ap_regexec( regex=[%s], string=[%s] )",
                   cn, npp(regex), npp(string) );
    #endif

    regex_error = ap_regexec(this_pregex, string, 0, (regmatch_t *)NULL,0);

    #ifdef MOD_GZIP_DEBUG1
    if ( regex_error != 0 )
    mod_gzip_printf( "%s: regex_error = %d = NO MATCH!", cn, regex_error );
    else
    mod_gzip_printf( "%s: regex_error = %d = MATCH!", cn, regex_error );
    #endif
   }

 #endif /* MOD_GZIP_TEST_REGEX1 */

 this_action = MOD_GZIP_IMAP_STATIC1;

 #ifdef FUTURE_USE

 if ( ( this_action != MOD_GZIP_IMAP_DYNAMIC1 ) &&
      ( this_action != MOD_GZIP_IMAP_DYNAMIC2 ) &&
      ( this_action != MOD_GZIP_IMAP_STATIC1  ) )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: this_action = %d = MOD_GZIP_IMAP_??? Unknown action",cn,this_action);
    mod_gzip_printf( "%s: return( mod_gzip: ERROR: Unrecognized item 'action'",cn);
    #endif

    return( "mod_gzip: ERROR: Unrecognized item 'action'" );
   }

 #endif

 if ( mgc->imap_total_entries < MOD_GZIP_IMAP_MAXNAMES )
   {
    if ( mod_gzip_strlen( a2 ) < MOD_GZIP_IMAP_MAXNAMELEN )
      {
       x = mgc->imap_total_entries;

       p1 = a2;

       mod_gzip_strcpy( mgc->imap[x].name, p1 );

       mgc->imap[x].namelen = mod_gzip_strlen( mgc->imap[x].name );

       mgc->imap[x].include   = this_include;
       mgc->imap[x].type      = this_type;
       mgc->imap[x].action    = this_action;
       mgc->imap[x].direction = this_direction;
       mgc->imap[x].port      = this_port;
       mgc->imap[x].len1      = this_len1;
       mgc->imap[x].pregex    = this_pregex;

       mgc->imap_total_entries++;

       if ( this_type == MOD_GZIP_IMAP_ISMIME )
         {
          mgc->imap_total_ismime++;
         }
       else if ( this_type == MOD_GZIP_IMAP_ISFILE )
         {
          mgc->imap_total_isfile++;
         }
       else if ( this_type == MOD_GZIP_IMAP_ISURI )
         {
          mgc->imap_total_isuri++;
         }
       else if ( this_type == MOD_GZIP_IMAP_ISHANDLER )
         {
          mgc->imap_total_ishandler++;
         }
       else if ( this_type == MOD_GZIP_IMAP_ISREQHEADER )
         {
          mgc->imap_total_isreqheader++;
         }
       else if ( this_type == MOD_GZIP_IMAP_ISRSPHEADER )
         {
          mgc->imap_total_isrspheader++;
         }
      }
    else
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: return( mod_gzip: ERROR: Item name is too long",cn);
       #endif

       return( "mod_gzip: ERROR: Item name is too long" );
      }
   }
 else
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: return( mod_gzip: ERROR: Item index is full",cn);
    #endif

    return( "mod_gzip: ERROR: Item index is full" );
   }

 #ifdef MOD_GZIP_DEBUG1

 mod_gzip_printf( "%s: mgc->imap_total_entries     = %d",
                   cn, mgc->imap_total_entries );
 mod_gzip_printf( "%s: mgc->imap_total_ismime      = %d",
                   cn, mgc->imap_total_ismime );
 mod_gzip_printf( "%s: mgc->imap_total_isfile      = %d",
                   cn, mgc->imap_total_isfile );
 mod_gzip_printf( "%s: mgc->imap_total_isuri       = %d",
                   cn, mgc->imap_total_isuri );
 mod_gzip_printf( "%s: mgc->imap_total_ishandler   = %d",
                   cn, mgc->imap_total_ishandler );
 mod_gzip_printf( "%s: mgc->imap_total_isreqheader = %d",
                   cn, mgc->imap_total_isreqheader );
 mod_gzip_printf( "%s: mgc->imap_total_isrspheader = %d",
                   cn, mgc->imap_total_isrspheader );

 mod_gzip_printf( "%s: Exit > return( NULL ) >",cn);

 #endif

 return NULL;
}

static const char *
mod_gzip_set_item_include( cmd_parms *parms, void *cfg, char *a1, char *a2 )
{
 mod_gzip_conf *mgc;
 char *arg;

 #ifdef MOD_GZIP_DEBUG1
 server_rec *srv = parms->server;
 char cn[]="mod_gzip_set_item_include()";
 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_server_now = srv;
 mod_gzip_printf( " ");
 mod_gzip_printf( "%s: Entry", cn );
 mod_gzip_printf( "%s: a1=[%s]", cn, npp(a1) );
 mod_gzip_printf( "%s: a2=[%s]", cn, npp(a2) );
 #endif

 arg = a1;

 mgc = ( mod_gzip_conf * ) cfg;

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: mgc->loc             = [%s]", cn, npp(mgc->loc));
 mod_gzip_printf( "%s: srv->is_virtual      = %ld",  cn, (long)srv->is_virtual );
 mod_gzip_printf( "%s: srv->server_hostname = [%s]", cn, npp(srv->server_hostname));
 mod_gzip_printf( "%s: mgc->cmode           = %ld",  cn, (long) mgc->cmode );
 #endif

 return( mod_gzip_imap_add_item( parms, mgc, a1, a2, 1 ) );
}

static const char *
mod_gzip_set_item_exclude( cmd_parms *parms, void *cfg, char *a1, char *a2 )
{
 mod_gzip_conf *mgc;
 char *arg;

 #ifdef MOD_GZIP_DEBUG1
 server_rec *srv = parms->server;
 char cn[]="mod_gzip_set_item_exclude()";
 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_server_now = srv;
 mod_gzip_printf( " ");
 mod_gzip_printf( "%s: Entry", cn );
 mod_gzip_printf( "%s: a1=[%s]", cn, npp(a1));
 mod_gzip_printf( "%s: a2=[%s]", cn, npp(a2));
 #endif

 arg = a1;

 mgc = ( mod_gzip_conf * ) cfg;

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: mgc->loc             = [%s]", cn, npp(mgc->loc));
 mod_gzip_printf( "%s: srv->is_virtual      = %ld",  cn, (long)srv->is_virtual );
 mod_gzip_printf( "%s: srv->server_hostname = [%s]", cn, npp(srv->server_hostname));
 mod_gzip_printf( "%s: mgc->cmode           = %ld",  cn, (long) mgc->cmode );
 #endif

 return( mod_gzip_imap_add_item( parms, mgc, a1, a2, 0 ) );
}

static void *mod_gzip_create_dconfig(
pool *p,
char *dirspec
)
{
 mod_gzip_conf *cfg;
 char *dname = dirspec;

 cfg = (mod_gzip_conf *) ap_pcalloc(p, sizeof(mod_gzip_conf));

 cfg->cmode = MOD_GZIP_CONFIG_MODE_DIRECTORY;

 dname = (dname != NULL) ? dname : "";

 cfg->loc = ap_pstrcat(p, "DIR(", dname, ")", NULL);

 mod_gzip_set_defaults1( (mod_gzip_conf *) cfg );

 return (void *) cfg;
}

static void *mod_gzip_merge_dconfig(
pool *p,
void *parent_conf,
void *newloc_conf
)
{
 mod_gzip_conf *merged_config = (mod_gzip_conf *) ap_pcalloc(p, sizeof(mod_gzip_conf));
 mod_gzip_conf *pconf         = (mod_gzip_conf *) parent_conf;
 mod_gzip_conf *nconf         = (mod_gzip_conf *) newloc_conf;

 mod_gzip_merge1(
 ( pool          * ) p,
 ( mod_gzip_conf * ) merged_config,
 ( mod_gzip_conf * ) pconf,
 ( mod_gzip_conf * ) nconf
 );

 return (void *) merged_config;
}

static void *mod_gzip_create_sconfig(
pool       *p,
server_rec *s
)
{
 mod_gzip_conf *cfg;
 char *sname = s->server_hostname;

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_server_now = s;
 #endif

 cfg = (mod_gzip_conf *) ap_pcalloc(p, sizeof(mod_gzip_conf));

 cfg->cmode = MOD_GZIP_CONFIG_MODE_SERVER;

 sname = (sname != NULL) ? sname : "";

 cfg->loc = ap_pstrcat(p, "SVR(", sname, ")", NULL);

 mod_gzip_set_defaults1( (mod_gzip_conf *) cfg );

 return (void *) cfg;
}

static void *mod_gzip_merge_sconfig(
pool *p,
void *parent_conf,
void *newloc_conf
)
{
 mod_gzip_conf *merged_config = (mod_gzip_conf *) ap_pcalloc(p, sizeof(mod_gzip_conf));
 mod_gzip_conf *pconf         = (mod_gzip_conf *) parent_conf;
 mod_gzip_conf *nconf         = (mod_gzip_conf *) newloc_conf;

 mod_gzip_merge1(
 ( pool          * ) p,
 ( mod_gzip_conf * ) merged_config,
 ( mod_gzip_conf * ) pconf,
 ( mod_gzip_conf * ) nconf
 );

 return (void *) merged_config;
}

char mod_gzip_command_no_longer_supported[] =
"Configuration directive no longer supported.";

static const char *
mod_gzip_obsolete_command( cmd_parms *parms, void *cfg, char *arg )
{
 return mod_gzip_command_no_longer_supported;
}

static const command_rec mod_gzip_cmds[] =
{
 {"mod_gzip_on", mod_gzip_set_is_on, NULL, OR_OPTIONS, TAKE1,
  "Yes=mod_gzip will handle requests. No=mod_gzip is disabled."},
 {"mod_gzip_add_header_count", mod_gzip_set_add_header_count, NULL, OR_OPTIONS, TAKE1,
  "Yes=Add header byte counts to Common Log Format output total(s)."},
 {"mod_gzip_keep_workfiles", mod_gzip_set_keep_workfiles, NULL, OR_OPTIONS, TAKE1,
  "Yes=Keep any work files used. No=Automatically delete any work files used."},
 {"mod_gzip_dechunk", mod_gzip_set_dechunk, NULL, OR_OPTIONS, TAKE1,
  "Yes=Allow removal of 'Transfer-encoding: chunked' when necessary."},
 {"mod_gzip_min_http", mod_gzip_set_min_http, NULL, OR_OPTIONS, TAKE1,
  "Minimum HTTP protocol value to support. 1000 = HTTP/1.0  1001 = HTTP/1.1"},
 {"mod_gzip_minimum_file_size", mod_gzip_set_minimum_file_size, NULL, OR_OPTIONS, TAKE1,
  "Minimum size ( bytes ) of a file eligible for compression"},
 {"mod_gzip_maximum_file_size", mod_gzip_set_maximum_file_size, NULL, OR_OPTIONS, TAKE1,
  "Maximum size ( bytes ) of a file eligible for compression"},
 {"mod_gzip_maximum_inmem_size", mod_gzip_set_maximum_inmem_size, NULL, OR_OPTIONS, TAKE1,
  "Maximum size ( bytes ) to use for in-memory compression."},
 {"mod_gzip_temp_dir", mod_gzip_set_temp_dir, NULL, OR_OPTIONS, TAKE1,
  "The directory to use for work files and compression cache"},
 {"mod_gzip_item_include", mod_gzip_set_item_include, NULL, OR_OPTIONS, TAKE2,
  "\r\nARG1=[mime,handler,file,uri,reqheader,rspheader] \r\nARG2=[Name of item to INCLUDE in list of things that should be compressed]"},
 {"mod_gzip_item_exclude", mod_gzip_set_item_exclude, NULL, OR_OPTIONS, TAKE2,
  "\r\nARG1=[mime,handler,file,uri,reqheader,rspheader] \r\nARG2=[Name of item to EXCLUDE from list of things that should be compressed]"},
 {"mod_gzip_add_vinfo", mod_gzip_obsolete_command, NULL, OR_OPTIONS, TAKE1,
  mod_gzip_command_no_longer_supported },
 {"mod_gzip_do_static_files", mod_gzip_obsolete_command, NULL, OR_OPTIONS, TAKE1,
  mod_gzip_command_no_longer_supported },
 {"mod_gzip_do_cgi", mod_gzip_obsolete_command, NULL, OR_OPTIONS, TAKE1,
  mod_gzip_command_no_longer_supported },
 {"mod_gzip_verbose_debug", mod_gzip_obsolete_command, NULL, OR_OPTIONS, TAKE1,
  mod_gzip_command_no_longer_supported },
 {"mod_gzip_post_on", mod_gzip_obsolete_command, NULL, OR_OPTIONS, TAKE1,
  mod_gzip_command_no_longer_supported },
 #ifdef MOD_GZIP_COMMAND_VERSION_USED
 {"mod_gzip_command_version", mod_gzip_set_command_version, NULL, OR_OPTIONS, TAKE1,
  "User defined pickup string to use for mod_gzip version command."},
 #endif
 #ifdef MOD_GZIP_CAN_NEGOTIATE
 {"mod_gzip_can_negotiate", mod_gzip_set_can_negotiate, NULL, OR_OPTIONS, TAKE1,
  "Yes=Negotiate/send static compressed versions of files  No=Do not negotiate."},
 #endif
 {NULL}
};

static const handler_rec mod_gzip_handlers[] =
{
 {"mod_gzip_handler", mod_gzip_handler},
 {CGI_MAGIC_TYPE,     mod_gzip_handler},
 {"cgi-script",       mod_gzip_handler},
 {"*",                mod_gzip_handler},
 {NULL}
};

module MODULE_VAR_EXPORT gzip_module =
{
 STANDARD_MODULE_STUFF,
 mod_gzip_init,
 mod_gzip_create_dconfig,
 mod_gzip_merge_dconfig,
 mod_gzip_create_sconfig,
 mod_gzip_merge_sconfig,
 mod_gzip_cmds,
 mod_gzip_handlers,
 NULL,
 NULL,
 NULL,
 NULL,
 mod_gzip_type_checker,
 NULL,
 NULL,
 NULL,
 NULL,
 NULL,
 NULL
};

#ifdef NETWARE
int main(int argc, char *argv[]) 
{
    ExitThread(TSR_THREAD, 0);
}
#endif

long mod_gzip_send(
char        *buf,
long         buflen,
request_rec *r
)
{
 #ifdef MOD_GZIP_DEBUG1
 char cn[]="mod_gzip_send()";
 #endif

 int  bytes_to_send    = 0;
 int  bytes_sent       = 0;
 long bytes_left       = buflen;
 long total_bytes_sent = 0;

 char *p1=buf;
 int   p1maxlen=4096; 

 #ifdef MOD_GZIP_DEBUG1
 #ifdef MOD_GZIP_DEBUG1_SEND1
 mod_gzip_printf("%s: Entry...",cn);
 mod_gzip_printf("%s: buf    = %ld",cn,(long)buf);
 mod_gzip_printf("%s: buflen = %ld",cn,(long)buflen);
 mod_gzip_printf("%s: r      = %ld",cn,(long)r);
 #endif
 #endif

 if ( !buf    ) return 0;
 if ( !buflen ) return 0;
 if ( !r      ) return 0;

 for (;;) 
    {
     if ( bytes_left <= 0 )
       {
        break;
       }

     bytes_to_send = p1maxlen; 

     if ( bytes_to_send > bytes_left )
       {
        bytes_to_send = bytes_left; 
       }

     #ifdef MOD_GZIP_DEBUG1
     #ifdef MOD_GZIP_DEBUG1_SEND1
     mod_gzip_printf("%s: Call ap_rwrite(bytes_to_send=%d)...",cn,bytes_to_send);
     #endif
     #endif

     bytes_sent =
     ap_rwrite(
     p1,
     bytes_to_send,
     (request_rec *) r
     );

     #ifdef MOD_GZIP_DEBUG1
     #ifdef MOD_GZIP_DEBUG1_SEND1
     mod_gzip_printf("%s: Back ap_rwrite(bytes_to_send=%d)...",cn,bytes_to_send);
     mod_gzip_printf("%s: bytes_sent = %d",cn,bytes_sent);
     #endif
     #endif

     if ( bytes_sent > 0 )
       {
        total_bytes_sent += bytes_sent;
        bytes_left       -= bytes_sent;
        p1               += bytes_sent;
       }
     else
       {
        break;
       }
    }

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf("%s: Done > return( total_bytes_sent = %ld ) >",
                          cn, (long) total_bytes_sent);
 #endif

 return( total_bytes_sent );
}

int mod_gzip_redir1_handler(
request_rec   *r,
mod_gzip_conf *dconf
)
{
 #ifdef MOD_GZIP_DEBUG1
 char cn[]="mod_gzip_redir1_handler()";
 #endif

 int rc          = 0;
 int status      = 0;
 int pid         = 0;
 int save_socket = 0;

 int   dconf__keep_workfiles = 0;
 char *dconf__temp_dir       = 0;

 char tempfile_redir1[ MOD_GZIP_MAX_PATH_LEN + 2 ];

 #ifdef WIN32
 int save_flags = 0;
 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( " " );
 mod_gzip_printf( "%s: Entry...",cn);
 #endif

 tempfile_redir1[0] = 0;

 dconf__keep_workfiles = dconf->keep_workfiles;
 dconf__temp_dir       = dconf->temp_dir;

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: r->proxyreq           = %d",   cn, (int) r->proxyreq );
 mod_gzip_printf( "%s: dconf__keep_workfiles = %d",   cn, (int) dconf__keep_workfiles );
 mod_gzip_printf( "%s: dconf__temp_dir       = [%s]", cn, npp(dconf__temp_dir));
 #endif

 ap_table_setn( r->notes,
 "mod_gzip_running",ap_pstrdup(r->pool,"1"));

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: r->notes->mod_gzip_running set to '1'",cn);
 #endif

 pid = getpid();

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Saving 'r->connection->client->fd' value to 'save_socket' stack variable...",cn);
 #endif

 save_socket = (int) r->connection->client->fd;

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: 'r->connection->client->fd' saved to 'save_socket' stack variable",cn);
 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Creating 'tempfile_redir1' string now...",cn);
 #endif

 mod_gzip_create_unique_filename(
 dconf__temp_dir,
 (char *) tempfile_redir1,
 MOD_GZIP_MAX_PATH_LEN
 );

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: tempfile_redir1 = [%s]",cn,npp(tempfile_redir1));
 mod_gzip_printf( "%s: Call mod_gzip_dyn1_getfdo1(%s)...",cn,npp(tempfile_redir1));
 #endif

 status = mod_gzip_dyn1_getfdo1( r, tempfile_redir1 );

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Back mod_gzip_dyn1_getfdo1(%s)...",cn,npp(tempfile_redir1));
 #endif

 if ( status != OK )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Back mod_gzip_dyn1_getfdo1(%s)...",cn,npp(tempfile_redir1));
    mod_gzip_printf( "%s: .... mod_gzip_dyn1_getfdo1() call FAILED",cn);
    mod_gzip_printf( "%s: status = %d",cn,status);
    #endif

    #ifdef WIN32
    ap_log_error( "",0,APLOG_NOERRNO|APLOG_WARNING, r->server,
    "mod_gzip: ERROR: CreateFile(%s) in dyn1_getfdo1",tempfile_redir1);
    #else
    ap_log_error( "",0,APLOG_NOERRNO|APLOG_WARNING, r->server,
    "mod_gzip: ERROR: fopen(%s) in dyn1_getfdo1",tempfile_redir1);
    #endif

    ap_log_error( "",0,APLOG_NOERRNO|APLOG_WARNING, r->server,
    "mod_gzip: ERROR: %s",mod_gzip_check_permissions);

    #ifdef MOD_GZIP_USES_APACHE_LOGS
    ap_table_setn( r->notes,
    "mod_gzip_result",ap_pstrdup(r->pool,"DECLINED:DYN1_OPENFAIL_BODY"));
    #endif

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Exit > return( DECLINED ) >",cn);
    #endif

    return DECLINED;
   }
 else
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Back mod_gzip_dyn1_getfdo1(%s)...",cn,npp(tempfile_redir1));
    mod_gzip_printf( "%s: .... mod_gzip_dyn1_getfdo1() call SUCCEEDED",cn);
    #endif
   }

 #ifdef WIN32

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: WIN32: Saves/restores B_SOCKET flag as well...",cn);
 mod_gzip_printf( "%s: WIN32: Saving 'r->connection->client->flags' value to 'save_flags' stack variable...",cn);
 #endif

 save_flags = (int) r->connection->client->flags;

 #ifdef MOD_GZIP_DEBUG1

 mod_gzip_printf( "%s: WIN32: r->connection->client->flags = %ld",  cn, (long) r->connection->client->flags );

 if ( r->connection->client->flags & B_SOCKET )
   {
    mod_gzip_printf( "%s: WIN32: r->connection->client->flags & B_SOCKET is TRUE",cn);
   }
 else
   {
    mod_gzip_printf( "%s: WIN32: r->connection->client->flags & B_SOCKET is FALSE",cn);
   }

 #endif

 if ( r->connection->client->flags & B_SOCKET )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: WIN32: Clearing B_SOCKET flag now....", cn);
    mod_gzip_printf( "%s: WIN32: r->connection->client->flags &= ~B_SOCKET", cn);
    #endif

    r->connection->client->flags &= ~B_SOCKET;
   }
 else
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: WIN32: B_SOCKET flag not present...", cn);
    mod_gzip_printf( "%s: WIN32: No action was taken...", cn);
    #endif
   }

 #ifdef MOD_GZIP_DEBUG1

 mod_gzip_printf( "%s: WIN32: r->connection->client->flags = %ld",  cn, (long) r->connection->client->flags );

 if ( r->connection->client->flags & B_SOCKET )
   {
    mod_gzip_printf( "%s: WIN32: r->connection->client->flags & B_SOCKET is TRUE",cn);
   }
 else
   {
    mod_gzip_printf( "%s: WIN32: r->connection->client->flags & B_SOCKET is FALSE",cn);
   }

 #endif

 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Call ap_internal_redirect(r->unparsed_uri=[%s])...",
                   cn,npp(r->unparsed_uri));
 mod_gzip_printf( " " );
 #endif

 ap_internal_redirect( r->unparsed_uri, r );

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( " " );
 mod_gzip_printf( "%s: Back ap_internal_redirect(r->unparsed_uri=[%s])...",
                   cn,npp(r->unparsed_uri));
 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Before... ap_rflush()...",cn);
 mod_gzip_printf( "%s: r->connection->client->outcnt     = %ld",
             cn,(long) r->connection->client->outcnt );
 mod_gzip_printf( "%s: r->connection->client->bytes_sent = %ld",
             cn,(long) r->connection->client->bytes_sent );
 mod_gzip_printf( "%s: Call..... ap_rflush()...",cn);
 #endif

 ap_rflush(r);

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Back..... ap_rflush()...",cn);
 mod_gzip_printf( "%s: After.... ap_rflush()...",cn);
 mod_gzip_printf( "%s: r->connection->client->outcnt     = %ld",
             cn,(long) r->connection->client->outcnt );
 mod_gzip_printf( "%s: r->connection->client->bytes_sent = %ld",
             cn,(long) r->connection->client->bytes_sent );
 #endif

 #ifdef WIN32

 if ( r->connection->client->hFH )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: WIN32: Call CloseHandle( r->connection->client->hFH )...",cn);
    #endif

    CloseHandle( r->connection->client->hFH );

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: WIN32: Back CloseHandle( r->connection->client->hFH )...",cn);
    #endif

    r->connection->client->hFH = 0;

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: WIN32: r->connection->client->hFH reset to ZERO",cn);
    #endif
   }
 else
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: WIN32: r->connection->client->hFH already closed.",cn);
    mod_gzip_printf( "%s: WIN32: No CloseFile() call was necessary at this point.",cn);
    #endif
   }

 #else

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: UNIX: Call close( r->connection->client->fd )...",cn);
 #endif

 close( r->connection->client->fd );

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: UNIX: Back close( r->connection->client->fd )...",cn);
 #endif

 #endif

 r->connection->client->fd = (int) save_socket;

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: 'r->connection->client->fd' RESTORED from 'save_socket' stack variable",cn);
 #endif

 #ifdef WIN32

 r->connection->client->flags = (int) save_flags;

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: WIN32: r->connection->client->flags RESTORED from 'save_flags' stack variable...",cn);
 #endif

 #ifdef MOD_GZIP_DEBUG1

 mod_gzip_printf( "%s: WIN32: r->connection->client->flags = %ld",  cn, (long) r->connection->client->flags );

 if ( r->connection->client->flags & B_SOCKET )
   {
    mod_gzip_printf( "%s: WIN32: r->connection->client->flags & B_SOCKET is TRUE",cn);
   }
 else
   {
    mod_gzip_printf( "%s: WIN32: r->connection->client->flags & B_SOCKET is FALSE",cn);
   }

 #endif

 #endif

 r->connection->client->bytes_sent = 0;
 r->connection->client->outcnt     = 0;

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Connection byte counts have been RESET...",cn);
 mod_gzip_printf( "%s: r->connection->client->outcnt     = %ld",
                   cn, r->connection->client->outcnt );
 mod_gzip_printf( "%s: r->connection->client->bytes_sent = %ld",
                   cn, r->connection->client->bytes_sent );

 mod_gzip_printf( "%s: r->bytes_sent = %ld",cn,(long)r->bytes_sent);

 if ( r->next )
   {
    mod_gzip_printf( "%s: r->next->bytes_sent = %ld",cn,(long)r->next->bytes_sent);
   }
 if ( r->prev )
   {
    mod_gzip_printf( "%s: r->prev->bytes_sent = %ld",cn,(long)r->prev->bytes_sent);
   }

 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Call mod_gzip_sendfile2(tempfile_redir1=[%s])...",
                   cn, npp(tempfile_redir1));
 #endif

 rc = (int)
 mod_gzip_sendfile2(
 r,
 dconf,
 tempfile_redir1
 );

 #ifdef MOD_GZIP_DEBUG1

 mod_gzip_printf( "%s: Back mod_gzip_sendfile2(tempfile_redir1=[%s])...",
                   cn, npp(tempfile_redir1));

 if ( rc == OK )
   {
    mod_gzip_printf( "%s: rc = %d OK", cn, (int) rc);
   }
 else if ( rc == DECLINED )
   {
    mod_gzip_printf( "%s: rc = %d DECLINED", cn, (int) rc );
   }
 else
   {
    mod_gzip_printf( "%s: rc = %d ( HTTP ERROR CODE? )", cn, (int) rc );
   }

 #endif

 if ( rc != OK )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: mod_gzip_sendfile2() call FAILED",cn);
    #endif
   }
 else
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: mod_gzip_sendfile2() call SUCCEEDED",cn);
    #endif
   }

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: r->connection->client->outcnt     = %ld",
                   cn, r->connection->client->outcnt );
 mod_gzip_printf( "%s: r->connection->client->bytes_sent = %ld",
                   cn, r->connection->client->bytes_sent );
 mod_gzip_printf( "%s: Sum of the 2..................... = %ld",
                   cn, r->connection->client->outcnt +
                       r->connection->client->bytes_sent );
 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: FINAL CLEANUP...",cn);
 mod_gzip_printf( "%s: dconf__keep_workfiles = %d",
                   cn, dconf__keep_workfiles );
 #endif

 if ( !dconf__keep_workfiles )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Call mod_gzip_delete_file(tempfile_redir1=[%s])...",
                      cn, npp(tempfile_redir1));
    #endif

    mod_gzip_delete_file( r, tempfile_redir1 );

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Back mod_gzip_delete_file(tempfile_redir1=[%s])...",
                      cn, npp(tempfile_redir1));
    #endif
   }

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Exit > return( OK ) >",cn,(int)rc);
 #endif

 return OK;
}

int mod_gzip_dyn1_getfdo1(
request_rec *r,
char        *filename
)
{
 int status      = OK;
 int temp_fd     = 0;
 int open_failed = 0;

 #ifdef WIN32
 HANDLE temp_fdhan;
 #endif

 #ifdef MOD_GZIP_DEBUG1
 char cn[]="mod_gzip_dyn1_getfdo1()";
 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Entry...",cn);
 mod_gzip_printf( "%s: filename=[%s]",cn,npp(filename));
 #endif

 #ifdef MOD_GZIP_DYN1_GETFDO1_ERROR_TEST1

 status = DECLINED;
 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: ** TEST ** 'status' forced to 'DECLINED'...",cn);
 #endif
 return DECLINED;
 #endif

 #ifdef WIN32

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: WIN32: Call CreateFile(filename=[%s],GENERIC_WRITE, FILE_SHARE_WRITE )...",
                   cn, npp(filename));
 #endif

 temp_fdhan =
 CreateFile(
 filename,
 GENERIC_WRITE,
 FILE_SHARE_WRITE,
 NULL,
 CREATE_ALWAYS,
 FILE_ATTRIBUTE_NORMAL,
 NULL
 );

 if ( temp_fdhan == INVALID_HANDLE_VALUE )
   {
    open_failed = 1;
   }

 #else

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: UNIX: Call open(filename=[%s],O_RDWR|O_CREAT|O_TRUNC,S_IRWXU)...",
                   cn, npp(filename));
 #endif

 if ((temp_fd = open(filename,O_RDWR|O_CREAT|O_TRUNC,S_IRWXU)) == -1)
   {
    open_failed = 1;
   }

 #endif

 if ( open_failed )
   {
    #ifdef MOD_GZIP_DEBUG1

    #ifdef WIN32
    mod_gzip_printf( "%s: WIN32: .... CreateFile() call FAILED", cn );
    mod_gzip_printf( "%s: WIN32: temp_fd = %d", cn, temp_fd );
    #else
    mod_gzip_printf( "%s: UNIX: .... open() call FAILED", cn );
    mod_gzip_printf( "%s: UNIX: temp_fd = %d", cn, temp_fd );
    #endif

    #endif

    ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, r,
    "mod_gzip: ERROR: Couldn't create a file descriptor at HTTP : %s", filename);

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Exit > return( HTTP_INTERNAL_SERVER_ERROR ) >",cn);
    #endif

    return HTTP_INTERNAL_SERVER_ERROR;
   }

 #ifdef MOD_GZIP_DEBUG1

 #ifdef WIN32

 mod_gzip_printf( "%s: WIN32: .... CreateFile() call SUCCEEDED", cn );
 mod_gzip_printf( "%s: WIN32: temp_fdhan = (WIN32 HANDLE) %d", cn, (int) temp_fdhan );

 #else

 mod_gzip_printf( "%s: UNIX: .... open() call SUCCEEDED", cn );
 mod_gzip_printf( "%s: UNIX: temp_fd = (UNIX int) %d", cn, temp_fd );

 #endif

 #endif

 #ifdef WIN32

 r->connection->client->fd  = (int) temp_fdhan;
 r->connection->client->hFH = (HANDLE) temp_fdhan;

 #ifdef MOD_GZIP_DEBUG1

 mod_gzip_printf( "%s: WIN32: r->connection->client->fd    = (int   ) %d",  cn, (int ) r->connection->client->fd );
 mod_gzip_printf( "%s: WIN32: r->connection->client->hFH   = (HANDLE) %ld", cn, (long) r->connection->client->hFH );
 mod_gzip_printf( "%s: WIN32: r->connection->client->flags = %ld",  cn, (long) r->connection->client->flags );

 if ( r->connection->client->flags & B_SOCKET )
   {
    mod_gzip_printf( "%s: WIN32: r->connection->client->flags & B_SOCKET is TRUE",cn);
   }
 else
   {
    mod_gzip_printf( "%s: WIN32: r->connection->client->flags & B_SOCKET is FALSE",cn);
   }

 #endif

 #else

 r->connection->client->fd = temp_fd;

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: UNIX: r->connection->client->fd = %d", cn, r->connection->client->fd );
 #endif

 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Exit > return( status=%d ) >", cn, status );
 #endif

 return status;
}

long mod_gzip_sendfile1(
request_rec *r,
char        *input_filename,
FILE        *ifh_passed,
long         starting_offset
)
{
 FILE *ifh;
 int   rc;
 int   err;
 int   bytesread;
 int   byteswritten;
 long  total_byteswritten=0;

 #define   MOD_GZIP_SENDFILE1_BUFFER_SIZE 4096
 char tmp[ MOD_GZIP_SENDFILE1_BUFFER_SIZE + 16 ];

 #ifdef MOD_GZIP_DEBUG1
 char cn[]="mod_gzip_sendfile1()";
 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Entry...",cn);
 mod_gzip_printf( "%s: r               = %ld", cn,(long)r);
 mod_gzip_printf( "%s: input_filename  = [%s]",cn,npp(input_filename));
 mod_gzip_printf( "%s: ifh_passed      = %ld", cn,(long) ifh_passed);
 mod_gzip_printf( "%s: starting_offset = %ld", cn,(long) starting_offset);
 #endif

 if ( !r ) return 0;
 if ( ( !ifh_passed ) && ( !input_filename ) ) return 0;

 if ( ifh_passed ) 
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: 'ifh_passed' is valid",cn);
    mod_gzip_printf( "%s: 'ifh' set to 'ifh_passed'...",cn);
    mod_gzip_printf( "%s: fopen() was NOT called...",cn);
    #endif

    ifh = ifh_passed;
   }
 else 
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: 'ifh_passed' was NULL...",cn);
    mod_gzip_printf( "%s: Call fopen(%s)...",cn,npp(input_filename));
    #endif

    ifh = fopen( input_filename, "rb" ); 

    if ( !ifh ) 
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: .... fopen() FAILED",cn);
       mod_gzip_printf( "%s: Exit > return( 0 ) >",cn);
       #endif

       return 0;
      }

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: .... fopen() SUCCEEDED",cn);
    #endif
   }

 if ( starting_offset > -1 )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Call fseek(ifh,starting_offset=%ld,0)...",
                      cn, (long) starting_offset );
    #endif

    rc = fseek( ifh, starting_offset, 0 );

    if ( rc != 0 ) 
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: .... fseek() FAILED",cn);
       mod_gzip_printf( "%s: Exit > return( 0 ) >",cn);
       #endif

       return 0;
      }
    #ifdef MOD_GZIP_DEBUG1
    else
      {
       mod_gzip_printf( "%s: .... fseek() SUCCEEDED",cn);
      }
    #endif
   }

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: sizeof( tmp ) = %d",cn,sizeof(tmp));
 mod_gzip_printf( "%s: MOD_GZIP_SENDFILE1_BUFFER_SIZE = %d",
               cn,(int)MOD_GZIP_SENDFILE1_BUFFER_SIZE);
 mod_gzip_printf( "%s: Sending file contents now...",cn);
 #endif

 for (;;)
    {
     bytesread = fread( tmp, 1, MOD_GZIP_SENDFILE1_BUFFER_SIZE, ifh );

     #ifdef MOD_GZIP_DEBUG1
     #ifdef MOD_GZIP_DEBUG1_SENDFILE1
     mod_gzip_printf( "%s: Back fread(): bytesread=%d",cn,bytesread);
     #endif
     #endif

     if ( bytesread < 1 ) break; 

     byteswritten = (int)
     mod_gzip_send( tmp, bytesread, r );

     if ( byteswritten > 0 )
       {
        total_byteswritten += byteswritten;
       }

     #ifdef MOD_GZIP_DEBUG1
     #ifdef MOD_GZIP_DEBUG1_SENDFILE1
     mod_gzip_printf( "%s: byteswritten = %d",cn,(int)byteswritten);
     #endif
     #endif

     if ( byteswritten != bytesread )
       {
        err = errno;

        #ifdef FUTURE_USE
        #if defined(WIN32) || defined(NETWARE)
        err = WSAGetLastError();
        #else  
        err = errno;
        #endif 
        #endif 

        #ifdef MOD_GZIP_DEBUG1
        mod_gzip_printf( "%s: TRANSMIT ERROR: bytesread=%d byteswritten=%d err=%d",
        cn,(int)bytesread,(int)byteswritten,(int)err );
        #endif

        #ifdef MOD_GZIP_DEBUG1
        mod_gzip_printf( "%s: Breaking out of transmit loop early...",cn);
        #endif

        break; 
       }
    }

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Done sending file contents...",cn);
 mod_gzip_printf( "%s: total_byteswritten = %ld",cn,total_byteswritten);
 #endif

 if ( !ifh_passed )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: 'ifh_passed' was NULL so close the",cn);
    mod_gzip_printf( "%s: input file that was opened at the",cn);
    mod_gzip_printf( "%s: start of this routine...",cn);
    mod_gzip_printf( "%s: Call fclose(%s)...",cn,npp(input_filename));
    #endif

    fclose( ifh );

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Back fclose(%s)...",cn,npp(input_filename));
    #endif
   }
 else 
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: 'ifh_passed' was VALID on entry.",cn);
    mod_gzip_printf( "%s: No call to fclose() was made.",cn);
    #endif
   }

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Exit > return( total_byteswritten = %ld ) >",
                   cn, (long) total_byteswritten );
 #endif

 return total_byteswritten;
}

int mod_gzip_sendfile2(
request_rec   *r,
mod_gzip_conf *dconf,
char          *input_filename
)
{
 FILE *ifh=0;
 FILE *ofh=0;
 int   ofh_used=0;

 int   i=0;
 int   rc=1;
 int   err=0;
 int   final_rc=1;
 int   linecount=0;
 int   bytesleft=0;
 int   bytesread=0;
 int   bytestoread=0;
 int   skip_advance=0;
 int   byteswritten=0;
 int   bytestowrite=0;
 long  total_bytes_sent=0;
 int   valid_char_count=0;
 int   total_bytes_left_to_copy=0;

 /*
  * TODO: This is a brain-teaser to solve.
  *
  * gcc UNIX compiler will report the following 'ok_to_send'
  * flag as no longer being 'used' inside this function but
  * if it is removed or even commented out then Apache GP faults
  * at runtime when logic enters this function ( but only if
  * MOD_GZIP_DEBUG1 flag is OFF at compile time ?? ).
  *
  * There is no 'macro' anywhere that bears this name and
  * there doesn't seem to be any reason for this GP fault.
  *
  * If this 'ok_to_send' variable is removed everything is fine
  * for Win32... it is only UNIX gcc compiler that reports no
  * warning but will still GP fault at runtime.
  *
  * It's a mystery. Can anyone solve it?
  */

 int ok_to_send = 1; /* This MUST remain or UNIX 'GP faults' ??? */

 #ifdef MOD_GZIP_DEBUG1
 int   total_bytes_copied=0;
 #endif

 struct stat sbuf;

 #define   MOD_GZIP_SENDFILE2_BUFFER_SIZE 4099
 char tmp[ MOD_GZIP_SENDFILE2_BUFFER_SIZE + 16 ];

 #define MOD_GZIP_LINE_BUFFER_SIZE 2048
 char lbuf[ MOD_GZIP_LINE_BUFFER_SIZE + 16 ];

 #define MOD_GZIP_DEBUG1_SENDFILE2

 #ifdef MOD_GZIP_DEBUG1
 char cn[]="mod_gzip_sendfile2()";
 #endif

 char *sp    = 0; 
 char *p1    = 0;
 int   p1len = 0;
 char *p2    = lbuf;
 int   p2len = 0;
 char *p3    = 0;

 long hbytes_in    = 0; 
 long bbytes_in    = 0; 
 long bbytes_out   = 0; 
 long bbytes_total = 0; 

 int  action_flag  = 0; 
 int  header_done  = 0; 
 int  body_done    = 0; 
 int  te_seen      = 0; 
 int  te_chunked   = 0; 
 int  send_as_is   = 0; 
 int  resp_code    = 0; 
 int  ce_seen      = 0; 
 int  ct_seen      = 0; 
 char ct_value[ 256 ];  

 int   field_ok        = 0; 
 int   fields_excluded = 0; 
 char *fieldkey        = 0; 
 char *fieldstring     = 0; 

 char output_filename1[256]; 

 long raw_input_length = 0; 

 #define CHUNKE_GET_HEXSTRING  1
 #define CHUNKE_WAIT_REAL_EOL1 2
 #define CHUNKE_GET_FOOTER     3
 #define CHUNKE_GET_REAL_DATA  4

 #define CHUNKE_HEXSTRING_MAXLEN 10

 int   chunke_count           = 1;
 char *chunke_errstr          = NULL;
 int   chunke_state           = CHUNKE_GET_HEXSTRING;
 int   chunke_nextstate       = 0;
 long  chunke_realdata_length = 0;
 long  chunke_realdata_read   = 0;
 int   chunke_hexstring_oset  = 0;
 char  chunke_hexstring[ CHUNKE_HEXSTRING_MAXLEN + 1 ];

 #ifdef CHUNKE_USES_OUTPUT_BUFFER
 char *chunke_obufstart  = lbuf; 
 char *chunke_obuf       = chunke_obufstart;
 int   chunke_obuflen    = 0;
 int   chunke_obuflenmax = MOD_GZIP_LINE_BUFFER_SIZE; 
 #endif

 int   dconf__dechunk        = 0; 
 int   dconf__keep_workfiles = 0; 
 char *dconf__temp_dir       = 0; 

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Entry...",cn);
 #endif

 if ( !r              ) return DECLINED;
 if ( !dconf          ) return DECLINED;
 if ( !input_filename ) return DECLINED;

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: r = %ld",cn,(long)r);
 mod_gzip_printf( "%s: r->proxyreq    = %d",  cn,(int) r->proxyreq );
 mod_gzip_printf( "%s: input_filename = [%s]",cn,npp(input_filename));
 #endif

 ct_value[0] = 0; 

 dconf__dechunk        = dconf->dechunk;
 dconf__keep_workfiles = dconf->keep_workfiles;
 dconf__temp_dir       = dconf->temp_dir;

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: dconf__dechunk        = %d",   cn, (int) dconf__dechunk );
 mod_gzip_printf( "%s: dconf__keep_workfiles = %d",   cn, (int) dconf__keep_workfiles );
 mod_gzip_printf( "%s: dconf__temp_dir       = [%s]", cn, npp(dconf__temp_dir));
 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Call stat(%s)...",cn,npp(input_filename));
 #endif

 rc = stat( input_filename, &sbuf );

 if ( rc ) 
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: .... stat() call FAILED",cn);
    #endif

    #ifdef MOD_GZIP_USES_APACHE_LOGS
    ap_table_setn( r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"STAT_FAILED"));
    #endif

    ap_log_error( APLOG_MARK,APLOG_NOERRNO|APLOG_ERR, r->server,
    "mod_gzip: stat(%s) in sendfile2", input_filename );

    ap_log_error( "",0,APLOG_NOERRNO|APLOG_ERR, r->server,
    "mod_gzip: %s",mod_gzip_check_permissions);

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Exit > return( DECLINED ) >",cn);
    #endif

    return DECLINED;
   }
 else 
   {
    raw_input_length = (long) sbuf.st_size;

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: .... stat() call SUCCEEDED",cn);
    mod_gzip_printf( "%s: sbuf.st_size     = %ld",cn,(long)sbuf.st_size);
    mod_gzip_printf( "%s: raw_input_length = %ld",cn,(long)raw_input_length);
    #endif

    if ( sbuf.st_size < 1 )
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: ERROR: The input file is EMPTY",cn);
       mod_gzip_printf( "%s: Exit > return( DECLINED ) >",cn);
       #endif

       #ifdef MOD_GZIP_USES_APACHE_LOGS
       ap_table_setn( r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"CAP_FILE_EMPTY"));
       #endif

       ap_log_error( APLOG_MARK,APLOG_NOERRNO|APLOG_ERR, r->server,
       "mod_gzip: EMPTY FILE [%s] in sendfile2", input_filename );

       ap_log_error( "",0,APLOG_NOERRNO|APLOG_ERR, r->server,
       "mod_gzip: %s",mod_gzip_check_permissions);

       return DECLINED;
      }
   }

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Call fopen(%s)...",cn,npp(input_filename));
 #endif

 ifh = fopen( input_filename, "rb" ); 

 if ( !ifh ) 
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: .... fopen() FAILED",cn);
    #endif

    #ifdef MOD_GZIP_USES_APACHE_LOGS
    ap_table_setn( r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"CAP_FOPEN_FAILED"));
    #endif

    ap_log_error( APLOG_MARK,APLOG_NOERRNO|APLOG_ERR, r->server,
    "mod_gzip: fopen(%s) in sendfile2", input_filename);

    ap_log_error( "",0,APLOG_NOERRNO|APLOG_ERR, r->server,
    "mod_gzip: %s",mod_gzip_check_permissions);

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Exit > return( DECLINED ) >",cn);
    #endif

    return DECLINED;
   }

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: .... fopen() SUCCEEDED",cn);
 mod_gzip_printf( "%s: sizeof( tmp ) = %d",cn,sizeof(tmp));
 mod_gzip_printf( "%s: MOD_GZIP_SENDFILE2_BUFFER_SIZE = %d",
               cn,(int)MOD_GZIP_SENDFILE2_BUFFER_SIZE);
 mod_gzip_printf( "%s: Processing header now...",cn);
 #endif

 for (;;)
    {
     bytesread = fread( tmp, 1, MOD_GZIP_SENDFILE2_BUFFER_SIZE, ifh );

     #ifdef MOD_GZIP_DEBUG1
     mod_gzip_printf( "%s: HEADER: %5.5d: Back fread(): bytesread=%d",
                       cn,linecount,bytesread);
     #endif

     if ( bytesread < 1 ) break; 

     p1    = tmp;
     p1len = 0;

     for ( i=0; i<bytesread; i++ )
        {
         if ( *p1 == 10 )
           {
            *p2 = 0; 

            linecount++; 

            if ( valid_char_count < 1 )
              {
               #ifdef MOD_GZIP_DEBUG1
               mod_gzip_printf( "%s: HEADER: %5.5d: lbuf=[CR/LF]",
                                 cn,linecount);
               #endif

               p1++;         
               p1len++;      
               hbytes_in++;  

               header_done = 1; 

               break; 
              }
            else 
              {
               #ifdef MOD_GZIP_DEBUG1
               mod_gzip_printf( "%s: HEADER: %5.5d: lbuf=[%s]",
                                 cn,linecount,lbuf);
               #endif

               if ( linecount == 1 )
                 {
                  p3 = lbuf; 

                  while((*p3!=0)&&(*p3<33)) p3++; 
                  while((*p3!=0)&&(*p3>32)) p3++; 
                  while((*p3!=0)&&(*p3<33)) p3++; 

                  #ifdef MOD_GZIP_DEBUG1
                  mod_gzip_printf( "%s: HEADER: %5.5d: * p3=[%s]",
                                    cn,linecount,npp(p3));
                  #endif

                  if ( *p3 != 0 )
                    {
                     resp_code = (int) atoi( p3 );
                    }

                  #ifdef MOD_GZIP_DEBUG1
                  mod_gzip_printf( "%s: HEADER: %5.5d: * resp_code = %d",
                                    cn,linecount,resp_code);
                  #endif
                 }

               else if ( lbuf[0] == 'T' )
                 {
                  if ( mod_gzip_strnicmp(lbuf,"Transfer-Encoding:",18)==0)
                    {
                     #ifdef MOD_GZIP_DEBUG1
                     mod_gzip_printf( "%s: HEADER: %5.5d: * 'Transfer-Encoding:' seen",
                                       cn,linecount);
                     #endif

                     te_seen = 1; 

                     if ( mod_gzip_stringcontains( lbuf, "chunked" ) )
                       {
                        #ifdef MOD_GZIP_DEBUG1
                        mod_gzip_printf( "%s: HEADER: %5.5d: * 'Transfer-Encoding: chunked' seen",
                                          cn,linecount);
                        #endif

                        te_chunked = 1; 
                       }
                    }
                 }

               else if ( lbuf[0] == 'C' )
                 {
                  if ( mod_gzip_strnicmp(lbuf,"Content-Encoding:",17)==0)
                    {
                     #ifdef MOD_GZIP_DEBUG1
                     mod_gzip_printf( "%s: HEADER: %5.5d: * 'Content-Encoding:' seen",
                                       cn,linecount);
                     #endif

                     ce_seen = 1; 
                    }

                  else if ( mod_gzip_strnicmp(lbuf,"Content-Type:",13)==0)
                    {
                     #ifdef MOD_GZIP_DEBUG1
                     mod_gzip_printf( "%s: HEADER: %5.5d: * 'Content-Type:' seen",
                                       cn,linecount);
                     #endif

                     ct_seen = 1; 

                     p3 = lbuf; 

                     while((*p3!=0)&&(*p3<33)) p3++; 
                     while((*p3!=0)&&(*p3>32)) p3++; 
                     while((*p3!=0)&&(*p3<33)) p3++; 

                     if ( p2len < 230 ) 
                       {
                        mod_gzip_strcpy( ct_value, p3 );
                       }
                     else 
                       {
                        sprintf( ct_value, "%-230.230s", p3 );
                       }

                     #ifdef MOD_GZIP_DEBUG1
                     mod_gzip_printf( "%s: HEADER: %5.5d: * ct_value=[%s]",
                                       cn,linecount,npp(ct_value));
                     #endif
                    }
                 }

                if ( ( fields_excluded < 1 ) &&
                     ( dconf->imap_total_isrspheader > 0 ) )
                  {
                   #ifdef MOD_GZIP_DEBUG1
                   mod_gzip_printf( "%s: dconf->imap_total_isrspheader = %d", cn,
                                   (int) dconf->imap_total_isrspheader );
                   mod_gzip_printf( "%s: Checking RESPONSE header field...", cn );
                   #endif

                   p3 = lbuf; 

                   while((*p3!=0)&&(*p3<33)) p3++; 
                   while((*p3!=0)&&(*p3>32)) p3++; 
                   while((*p3!=0)&&(*p3<33)) p3++; 

                   fieldkey    = lbuf; 
                   fieldstring = p3;   

                   #ifdef MOD_GZIP_DEBUG1
                   mod_gzip_printf( "%s: Call mod_gzip_validate1()...",cn);
                   #endif

                   field_ok =
                   mod_gzip_validate1(
                   (request_rec   *) r,
                   (mod_gzip_conf *) dconf,
                   NULL, /* r->filename     (Not used here) */
                   NULL, /* r->uri          (Not used here) */
                   NULL, /* r->content_type (Not used here) */
                   NULL, /* r->handler      (Not used here) */
                   (char *) fieldkey,     /* Field key */
                   (char *) fieldstring,  /* Field string */
                   MOD_GZIP_RESPONSE      /* Direction */
                   );

                   #ifdef MOD_GZIP_DEBUG1
                   mod_gzip_printf( "%s: Back mod_gzip_validate1()...",cn);
                   mod_gzip_printf( "%s: field_ok = %d",cn,field_ok);
                   #endif

                   if ( field_ok == MOD_GZIP_IMAP_DECLINED1 )
                     {
                      fields_excluded++; 
                     }
                  }
              }

            p2    = lbuf;
            p2len = 0;

            valid_char_count = 0;
           }
         else 
           {
            if ( *p1 > 32 ) valid_char_count++;

            if (( p2len < MOD_GZIP_LINE_BUFFER_SIZE )&&( *p1 != 13 ))
              {
               *p2++ = *p1;
               p2len++;
              }
           }

         p1++;        
         p1len++;     
         hbytes_in++; 
        }

     if ( header_done ) break;
    }

 bbytes_total = raw_input_length - hbytes_in; 

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Done processing header...",cn);
 mod_gzip_printf( "%s: header_done     = %d",cn,header_done);
 mod_gzip_printf( "%s: fields_excluded = %d",cn,fields_excluded);
 mod_gzip_printf( "%s: bytesread       = %d",cn,bytesread);
 mod_gzip_printf( "%s: p1len           = %d",cn,p1len);
 mod_gzip_printf( "%s: resp_code       = %d (HTTP response code)",cn,resp_code);
 mod_gzip_printf( "%s: hbytes_in       = %ld (Total HEADER bytes read)",cn,(long)hbytes_in);
 mod_gzip_printf( "%s: bbytes_total    = %ld (Total BODY bytes left)",cn,(long)bbytes_total);
 mod_gzip_printf( "%s: te_seen         = %d (Transfer-Encoding:)",cn,te_seen);
 mod_gzip_printf( "%s: te_chunked      = %d (Transfer-Encoding: chunked)",cn,te_chunked);
 mod_gzip_printf( "%s: ce_seen         = %d (Content-Encoding:)",cn,ce_seen);
 mod_gzip_printf( "%s: ct_seen         = %d (Content-Type:)",cn,ct_seen);
 mod_gzip_printf( "%s: ct_value        = [%s]",cn,npp(ct_value));
 mod_gzip_printf( "%s: dconf__dechunk  = %ld",cn,(long)dconf__dechunk);
 #endif

 if ( !header_done )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: ERROR: 'header_done' flag is NOT SET",cn);
    mod_gzip_printf( "%s: No valid HTTP 'End of header' was found...",cn);
    #endif

    #ifdef MOD_GZIP_USES_APACHE_LOGS
    ap_table_setn( r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"NO_HTTP_EOH"));
    #endif

    ap_log_error( APLOG_MARK,APLOG_NOERRNO|APLOG_ERR, r->server,
    "mod_gzip: Invalid HTTP response header for r->the_request[%s] in sendfile2", r->the_request);
    ap_log_error( APLOG_MARK,APLOG_NOERRNO|APLOG_ERR, r->server,
    "mod_gzip: Invalid HTTP response header in FILE [%s] in sendfile2",input_filename);

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Exit > return( DECLINED ) >",cn);
    #endif

    return DECLINED;
   }

 if ( !resp_code )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: ERROR: 'resp_code' value is NOT SET",cn);
    mod_gzip_printf( "%s: No valid HTTP response code was found...",cn);
    #endif

    #ifdef MOD_GZIP_USES_APACHE_LOGS
    ap_table_setn( r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"NO_HTTP_RESP_CODE"));
    #endif

    ap_log_error( APLOG_MARK,APLOG_NOERRNO|APLOG_ERR, r->server,
    "mod_gzip: No valid HTTP response code for r->the_request[%s] in sendfile2", r->the_request);
    ap_log_error( APLOG_MARK,APLOG_NOERRNO|APLOG_ERR, r->server,
    "mod_gzip: No valid HTTP response code in FILE [%s] in sendfile2",input_filename);

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Exit > return( DECLINED ) >",cn);
    #endif

    return DECLINED;
   }

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: --------------------------------------------------",cn);
 mod_gzip_printf( "%s: Checking for various 'send as is' conditions...",cn);
 mod_gzip_printf( "%s: --------------------------------------------------",cn);
 #endif

 #ifdef MOD_GZIP_USES_APACHE_LOGS
 mod_gzip_strcpy( lbuf, "SEND_AS_IS");
 #endif

 if ( resp_code != 200 )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: resp_code is NOT '200'...",cn);
    mod_gzip_printf( "%s: Issuing send_as_is++",cn);
    #endif

    send_as_is++;

    #ifdef MOD_GZIP_USES_APACHE_LOGS
    mod_gzip_strcat( lbuf, ":NO_200");
    #endif
   }

 if ( ( !send_as_is ) && ( fields_excluded > 0 ) )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: 'fields_excluded' = %d ( More than ZERO )",
                       cn, fields_excluded );
    mod_gzip_printf( "%s: Issuing send_as_is++",cn);
    #endif

    send_as_is++;

    #ifdef MOD_GZIP_USES_APACHE_LOGS
    mod_gzip_strcat( lbuf, ":RESPONSE_FIELD_EXCLUDED");
    #endif
   }

 if ( ( !send_as_is ) && ( bbytes_total < 1 ) )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: bbytes_total is less than 1",cn);
    mod_gzip_printf( "%s: There is no valid BODY data.",cn);
    mod_gzip_printf( "%s: Issuing send_as_is++",cn);
    #endif

    send_as_is++;

    #ifdef MOD_GZIP_USES_APACHE_LOGS
    mod_gzip_strcat( lbuf, ":NO_BODY");
    #endif
   }

 if ( ( !send_as_is ) && ( ( te_seen == 1 ) && ( te_chunked == 0 ) ) )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: 'Transfer-Encoding:' is present but 'type' is UNKNOWN",cn);
    mod_gzip_printf( "%s: Issuing send_as_is++",cn);
    #endif

    send_as_is++;

    #ifdef MOD_GZIP_USES_APACHE_LOGS
    mod_gzip_strcat( lbuf, ":UNKNOWN_TE_VALUE");
    #endif
   }

 if ( ( !send_as_is ) && ( ce_seen == 1 ) )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: 'Content-Encoding:' is already present...",cn);
    mod_gzip_printf( "%s: Issuing send_as_is++",cn);
    #endif

    send_as_is++;

    #ifdef MOD_GZIP_USES_APACHE_LOGS
    mod_gzip_strcat( lbuf, ":HAS_CE");
    #endif
   }

 if ( ( !send_as_is ) && ( ct_seen != 1 ) )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: 'Content-Type:' response field NOT PRESENT...",cn);
    mod_gzip_printf( "%s: Issuing send_as_is++",cn);
    #endif

    send_as_is++;

    #ifdef MOD_GZIP_USES_APACHE_LOGS
    mod_gzip_strcat( lbuf, ":NO_CONTENT_TYPE_IN_RESPONSE_HEADER");
    #endif
   }
 else if ( !send_as_is ) 
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: 'Content-Type:' response field is PRESENT",cn);
    #endif

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: dconf->imap_total_ismime = %d", cn,
                    (int) dconf->imap_total_ismime );
    #endif

    if ( dconf->imap_total_ismime > 0 )
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Call mod_gzip_validate1(ct_value=[%s])...",
                         cn,npp(ct_value));
       #endif

       action_flag =
       mod_gzip_validate1(
       (request_rec   *) r,
       (mod_gzip_conf *) dconf,
       NULL,     /* r->filename  (Not used here) */
       NULL,     /* r->uri       (Not used here) */
       ct_value, /* Content type (Used         ) */
       NULL,     /* r->handler   (Not used here) */
       NULL,     /* Field key    (Not used here) */
       NULL,     /* Field string (Not used here) */
       0         /* Direction    (Not used here) */
       );

       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Back mod_gzip_validate1(ct_value=[%s])...",
                         cn,npp(ct_value));
       mod_gzip_printf( "%s: action_flag  = %d",cn,action_flag);
       #endif

       if ( action_flag != MOD_GZIP_IMAP_DECLINED1 )
         {
          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: This 'Content-Type:' is NOT EXCLUDED",cn);
          #endif
         }
       else
         {
          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: This 'Content-Type:' is EXCLUDED",cn);
          mod_gzip_printf( "%s: Issuing send_as_is++",cn);
          #endif

          send_as_is++;

          #ifdef MOD_GZIP_USES_APACHE_LOGS
          mod_gzip_strcat( lbuf, ":RESPONSE_CONTENT_TYPE_EXCLUDED");
          #endif
         }
      }
   }

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: --------------------------------------------------",cn);
 mod_gzip_printf( "%s: send_as_is   = %d",cn,send_as_is);
 mod_gzip_printf( "%s: --------------------------------------------------",cn);
 #endif

 if ( send_as_is > 0 ) 
   {
    #ifdef MOD_GZIP_USES_APACHE_LOGS
    ap_table_setn( r->notes,"mod_gzip_result",ap_pstrdup(r->pool,lbuf));
    #endif

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Sending response 'as is'...",cn);
    mod_gzip_printf( "%s: Issuing 'goto mod_gzip_sendfile2_send_as_is'...",cn);
    #endif

    goto mod_gzip_sendfile2_send_as_is;
   }

 if ( !te_seen )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Call fclose(%s)...",cn, npp(input_filename));
    #endif

    fclose( ifh );

    ifh = 0; 

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Back fclose(%s)...",cn,npp(input_filename));
    mod_gzip_printf( "%s: Call mod_gzip_encode_and_transmit()...",cn);
    #endif

    rc =
    mod_gzip_encode_and_transmit(
    r,              
    dconf,          
    input_filename, 
    1,              
    bbytes_total,   
    0,              
    hbytes_in,      
    ""              
    );

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Back mod_gzip_encode_and_transmit()...",cn);
    mod_gzip_printf( "%s: rc = %d",cn,rc);
    #endif

    if ( rc == DECLINED )
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: rc = DECLINED (FAILURE)",cn);
       mod_gzip_printf( "%s: Sending response 'as is'...",cn);
       mod_gzip_printf( "%s: Issuing 'goto mod_gzip_sendfile2_send_as_is'...",cn);
       #endif

       goto mod_gzip_sendfile2_send_as_is;
      }

    else 
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: rc = OK (SUCCESS)",cn);
       mod_gzip_printf( "%s: Call goto mod_gzip_sendfile2_cleanup...",cn,rc);
       #endif

       final_rc = rc; 

       goto mod_gzip_sendfile2_cleanup;
      }
   }

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: * TRANSFER ENCODING DE-CHUNKING OPTION",cn);
 mod_gzip_printf( "%s: dconf__dechunk = %ld",cn,(long)dconf__dechunk);
 #endif

 if ( dconf__dechunk ) 
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: dconf__dechunk = YES",cn);
    mod_gzip_printf( "%s: BODY data 'de-chunking' option is ON...",cn);
    #endif
   }
 else 
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: dconf__dechunk = NO",cn);
    mod_gzip_printf( "%s: BODY data 'de-chunking' option is OFF...",cn);
    #endif

    #ifdef MOD_GZIP_USES_APACHE_LOGS

    ap_table_setn( r->notes,"mod_gzip_result",
    ap_pstrdup(r->pool,"SEND_AS_IS:DECHUNK_OPTION_IS_OFF"));
    #endif

    ap_log_error( APLOG_MARK,APLOG_NOERRNO|APLOG_ERR, r->server,
    "mod_gzip: DECHUNK option is OFF in sendfile2");

    ap_log_error( APLOG_MARK,APLOG_NOERRNO|APLOG_ERR, r->server,
    "mod_gzip: Cannot compress chunked response for [%s]",
    r->the_request);

    ap_log_error( APLOG_MARK,APLOG_NOERRNO|APLOG_ERR, r->server,
    "mod_gzip: ** Uncompressed responses that use 'Transfer-encoding: chunked'");

    ap_log_error( APLOG_MARK,APLOG_NOERRNO|APLOG_ERR, r->server,
    "mod_gzip: ** must be 'de-chunked' before they can be compressed." );

    ap_log_error( APLOG_MARK,APLOG_NOERRNO|APLOG_ERR, r->server,
    "mod_gzip: ** Turn DECHUNK option ON for this response category." );

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Sending response 'as is'...",cn);
    mod_gzip_printf( "%s: Issuing 'goto mod_gzip_sendfile2_send_as_is'...",cn);
    #endif

    goto mod_gzip_sendfile2_send_as_is;
   }

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: De-chunking the body data now...",cn);
 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Creating 'output_filename1' string now...",cn);
 #endif

 mod_gzip_create_unique_filename(
 dconf__temp_dir,
 (char *) output_filename1,
 MOD_GZIP_MAX_PATH_LEN
 );

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: output_filename1 = [%s]",cn,npp(output_filename1));
 mod_gzip_printf( "%s: Call OUTPUT fopen(%s)...",cn,npp(output_filename1));
 #endif

 ofh = fopen( output_filename1, "wb" );

 if ( !ofh ) 
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: .... OUTPUT fopen() FAILED",cn);
    #endif

    #ifdef MOD_GZIP_USES_APACHE_LOGS

    ap_table_setn( r->notes,"mod_gzip_result",
    ap_pstrdup(r->pool,"SEND_AS_IS:FOPEN_FAILED"));
    #endif

    ap_log_error( APLOG_MARK,APLOG_NOERRNO|APLOG_ERR, r->server,
    "mod_gzip: fopen(%s) for OUTPUT in sendfile2", output_filename1 );

    ap_log_error( "",0,APLOG_NOERRNO|APLOG_ERR, r->server,
    "mod_gzip: %s",mod_gzip_check_permissions);

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Sending response 'as is'...",cn);
    mod_gzip_printf( "%s: Issuing 'goto mod_gzip_sendfile2_send_as_is'...",cn);
    #endif

    goto mod_gzip_sendfile2_send_as_is;
   }

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: .... OUTPUT fopen() SUCCEEDED",cn);
 #endif

 ofh_used = 1; 

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Call fseek(ifh,0,0)...",cn );
 #endif

 rc = fseek( ifh, 0, 0 );

 if ( rc != 0 ) 
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: .... fseek(ifh,0,0) FAILED",cn);
    #endif

    #ifdef MOD_GZIP_USES_APACHE_LOGS

    ap_table_setn( r->notes,"mod_gzip_result",
    ap_pstrdup(r->pool,"SEND_AS_IS:FSEEK_FAILED"));
    #endif

    ap_log_error( APLOG_MARK,APLOG_NOERRNO|APLOG_ERR, r->server,
    "mod_gzip: fseek(ifh,0,0) in sendfile2" );

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Sending response 'as is'...",cn);
    mod_gzip_printf( "%s: Issuing 'goto mod_gzip_sendfile2_send_as_is'...",cn);
    #endif

    goto mod_gzip_sendfile2_send_as_is;
   }
 #ifdef MOD_GZIP_DEBUG1
 else
   {
    mod_gzip_printf( "%s: .... fseek(ifh,0,0) SUCCEEDED",cn);
   }
 #endif

 total_bytes_left_to_copy = hbytes_in;

 #ifdef MOD_GZIP_DEBUG1
 total_bytes_copied = 0;
 mod_gzip_printf( "%s: Copying %ld header bytes now...",
                   cn,(long)total_bytes_left_to_copy);
 #endif

 while( total_bytes_left_to_copy > 0 )
    {
     if ( total_bytes_left_to_copy < MOD_GZIP_SENDFILE2_BUFFER_SIZE )
       {
        bytestoread = total_bytes_left_to_copy;
       }
     else
       {
        bytestoread = MOD_GZIP_SENDFILE2_BUFFER_SIZE;
       }

     bytesread    = fread ( tmp, 1, bytestoread, ifh );
     byteswritten = fwrite( tmp, 1, bytesread,   ofh );

     total_bytes_left_to_copy -= byteswritten;

     #ifdef MOD_GZIP_DEBUG1
     total_bytes_copied += byteswritten;
     #endif
    }

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: %ld header bytes have been copied...",
                   cn,(long)total_bytes_copied);
 #endif

 for (;;) 
    {
     p1 = tmp; 

     bytesread = fread( p1, 1, MOD_GZIP_SENDFILE2_BUFFER_SIZE, ifh );

     #ifdef MOD_GZIP_DEBUG1
     mod_gzip_printf( "%s: Back fread(): bytesread=%d",cn,bytesread);
     #endif

     if ( bytesread < 1 ) break; 

     bbytes_in += bytesread; 
     bytesleft  = bytesread; 

     if ( te_chunked ) 
       {
        #ifdef MOD_GZIP_DEBUG1
        mod_gzip_printf( "%s: 'te_chunked' flag is TRUE...",cn);
        mod_gzip_printf( "%s: Filtering chunked data...",cn);
        #endif

        sp = p1; 

        while ( bytesleft > 0 )
          {
           switch( chunke_state )
             {
              case CHUNKE_GET_HEXSTRING:

                   if ((*sp==13)||(*sp==10)||(*sp==';'))
                     {
                      chunke_hexstring[chunke_hexstring_oset]=0;

                      #ifdef MOD_GZIP_CHUNKE_DIAG1
                      mod_gzip_printf("chunke: %5.5ld chunke_hexstring=[%s]\n",
                                       chunke_count, npp(chunke_hexstring) );
                      #endif

                      chunke_realdata_length = (long)
                      strtol(chunke_hexstring,&chunke_errstr,16);

                      #ifdef MOD_GZIP_CHUNKE_DIAG1
                      mod_gzip_printf("chunke: %5.5ld realdata_length= 0x%X %ld\n",
                                 chunke_count, chunke_realdata_length,
                                               chunke_realdata_length );
                      #endif

                      if ( chunke_realdata_length < 1 )
                        {
                         #ifdef MOD_GZIP_CHUNKE_DIAG1
                         mod_gzip_printf("chunke: %5.5ld END OF CHUNKED DATA\n",
                                          chunke_count);
                         #endif
                        }

                      chunke_realdata_read = 0;

                      if ( *sp == 10 ) 
                        {
                         if ( chunke_realdata_length == 0 )
                           {
                            chunke_state = CHUNKE_GET_FOOTER;
                           }
                         else 
                           {
                            chunke_state = CHUNKE_GET_REAL_DATA;
                           }
                        }
                      else 
                        {
                         chunke_state = CHUNKE_WAIT_REAL_EOL1;

                         if ( chunke_realdata_length == 0 )
                           {
                            chunke_nextstate = CHUNKE_GET_FOOTER;
                           }
                         else 
                           {
                            chunke_nextstate = CHUNKE_GET_REAL_DATA;
                           }
                        }
                     }

                   else 
                     {
                      if ( ( chunke_hexstring_oset <
                             CHUNKE_HEXSTRING_MAXLEN ) &&
                           ( *sp > 32 ) )
                        {
                         chunke_hexstring[
                         chunke_hexstring_oset++] = *sp;

                         #ifdef MOD_GZIP_CHUNKE_DIAG1
                         chunke_hexstring[
                         chunke_hexstring_oset ] = 0;
                         #endif
                        }

                      #ifdef MOD_GZIP_CHUNKE_DIAG1
                      mod_gzip_printf("chunke: %5.5ld chunke_hexstring is now [%s]\n",
                                       chunke_count, npp(chunke_hexstring) );
                      #endif
                     }

                   break;

              case CHUNKE_WAIT_REAL_EOL1:

                   if ( *sp == 10 )
                     {
                      #ifdef MOD_GZIP_CHUNKE_DIAG1
                      mod_gzip_printf("chunke: %5.5ld Real EOL seen...\n",
                                       chunke_count );
                      #endif

                      chunke_state = chunke_nextstate;
                     }

                   break;

              case CHUNKE_GET_FOOTER:

                   if ( *sp == 10 )
                     {
                      #ifdef MOD_GZIP_CHUNKE_DIAG1
                      mod_gzip_printf("chunke: %5.5ld FOOTER EOL seen...\n",
                                       chunke_count );
                      #endif
                     }

                   break;

              case CHUNKE_GET_REAL_DATA:

                   #ifdef CHUNKE_USES_OUTPUT_BUFFER

                   *chunke_obuf++ = *sp;
                    chunke_obuflen++;

                   if ( chunke_obuflen >= chunke_obuflenmax )
                     {
                      byteswritten =
                      fwrite(
                      chunke_obufstart,
                      1,
                      chunke_obuflen,
                      ofh
                      );

                      if ( byteswritten > 0 )
                        {
                         bbytes_out += byteswritten;
                        }

                      chunke_obuf    = chunke_obufstart;
                      chunke_obuflen = 0;
                     }

                   chunke_realdata_read++;

                   #else 

                   bytestowrite = bytesleft; 

                   if ( ( chunke_realdata_read + bytesleft ) >
                          chunke_realdata_length )
                     {
                      bytestowrite =
                      chunke_realdata_length - chunke_realdata_read;
                     }

                   byteswritten =
                   fwrite(
                   sp,
                   1,
                   bytestowrite,
                   ofh
                   );

                   if ( byteswritten > 0 )
                     {
                      sp                   += byteswritten;
                      bytesleft            -= byteswritten;
                      bbytes_out           += byteswritten;
                      chunke_realdata_read += byteswritten;

                      skip_advance = 1;
                     }

                   #endif 

                   if ( chunke_realdata_read >=
                        chunke_realdata_length )
                     {
                      #ifdef CHUNKE_USES_OUTPUT_BUFFER

                      if ( chunke_obuflen > 0  )
                        {
                         byteswritten =
                         fwrite(
                         chunke_obufstart,
                         1,
                         chunke_obuflen,
                         ofh
                         );

                         if ( byteswritten > 0 )
                           {
                            bbytes_out += byteswritten;
                           }

                         chunke_obuf    = chunke_obufstart;
                         chunke_obuflen = 0;
                        }

                      #endif 

                      #ifdef MOD_GZIP_CHUNKE_DIAG1
                      mod_gzip_printf("chunke: %5.5ld DONE reading CHUNK data...\n",
                                 chunke_count );
                      mod_gzip_printf("chunke: %5.5ld realdata_length = %ld\n",
                                 chunke_count, chunke_realdata_length );
                      mod_gzip_printf("chunke: %5.5ld realdata_read   = %ld\n",
                                 chunke_count, chunke_realdata_read );
                      mod_gzip_printf("chunke: %5.5ld Increasing 'chunke_count' to %d\n",
                                 chunke_count, chunke_count + 1 );
                      mod_gzip_printf("chunke: %5.5ld Advancing to CHUNKE_WAIT_REAL_EOL1...\n",
                                 chunke_count );
                      #endif

                      chunke_count++;

                      chunke_hexstring[0]    = 0;
                      chunke_hexstring_oset  = 0;
                      chunke_realdata_length = 0;
                      chunke_realdata_read   = 0;

                      chunke_state     = CHUNKE_WAIT_REAL_EOL1;
                      chunke_nextstate = CHUNKE_GET_HEXSTRING;
                     }

                   break;

              default: break;
             }

           if ( skip_advance ) 
             {
              skip_advance = 0; 
             }
           else 
             {
              sp++;        
              bytesleft--; 
             }
          }

        #ifdef FUTURE_USE
        chunke_buffer_scan_done: ; 
        #endif

        #ifdef MOD_GZIP_CHUNKE_DIAG1
        mod_gzip_printf("chunke: %5.5ld Out of next buffer scan loop\n",
                   chunke_count );
        mod_gzip_printf("chunke: %5.5ld realdata_length = %ld\n",
                   chunke_count, chunke_realdata_length );
        mod_gzip_printf("chunke: %5.5ld realdata_read   = %ld\n",
                   chunke_count, chunke_realdata_read );
        mod_gzip_printf("chunke: %5.5ld realdata_left   = %ld\n",
                   chunke_count, chunke_realdata_length - chunke_realdata_read);
        mod_gzip_printf("chunke: %5.5ld Looping back for more input data...\n",
                   chunke_count );
        #endif
       }

     else 
       {
        byteswritten =
        fwrite(
        p1,
        1,
        bytesread,
        ofh
        );

        if ( byteswritten > 0 )
          {
           bbytes_out += byteswritten; 
          }

        #ifdef MOD_GZIP_DEBUG1
        mod_gzip_printf( "%s: byteswritten = %d",cn,(int)byteswritten);
        #endif

        if ( byteswritten != bytesread )
          {
           err = errno;

           #ifdef FUTURE_USE
           #if defined(WIN32) || defined(NETWARE)
           err = WSAGetLastError();
           #else  
           err = errno;
           #endif 
           #endif 

           #ifdef MOD_GZIP_DEBUG1
           mod_gzip_printf( "%s: TRANSMIT ERROR: bytesread=%d byteswritten=%d err=%d",
           cn,(int)bytesread,(int)byteswritten,(int)err );
           #endif

           #ifdef MOD_GZIP_DEBUG1
           mod_gzip_printf( "%s: Breaking out of transmit loop early...",cn);
           #endif

           body_done = 1; 
           break;         
          }
       }

     if ( body_done ) break;
    }

 bbytes_total = bbytes_out;

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Done processing BODY...",cn);
 mod_gzip_printf( "%s: Call fclose(ofh)...",cn);
 #endif

 fclose( ofh ); ofh = 0;

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Back fclose(ofh)...",cn);
 mod_gzip_printf( "%s: bbytes_in    = %ld (Total BODY bytes read)",cn,(long)bbytes_in);
 mod_gzip_printf( "%s: bbytes_out   = %ld (Total BODY bytes written)",cn,(long)bbytes_out);
 mod_gzip_printf( "%s: bbytes_total = %ld (Same as bbytes_out)",cn,(long)bbytes_total);
 #endif

 if ( bbytes_total > 0 )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Call fclose(%s)...",cn, npp(input_filename));
    #endif

    fclose( ifh ); ifh = 0;

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Back fclose(%s)...",cn, npp(input_filename));
    #endif

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Call mod_gzip_encode_and_transmit()...",cn);
    #endif

    rc =
    mod_gzip_encode_and_transmit(
    r,                
    dconf,            
    output_filename1, 
    1,                
    bbytes_total,     
    0,                
    hbytes_in,        
    "DECHUNK:"        
    );

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Back mod_gzip_encode_and_transmit()...",cn);
    mod_gzip_printf( "%s: rc = %d",cn,rc);
    #endif

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: dconf__keep_workfiles = %d",
                      cn, dconf__keep_workfiles );
    #endif

    if ( !dconf__keep_workfiles ) 
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Call mod_gzip_delete_file(output_filename1=[%s])...",
                         cn, npp(output_filename1));
       #endif

       mod_gzip_delete_file( r, output_filename1 );

       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Back mod_gzip_delete_file(output_filename1=[%s])...",
                         cn, npp(output_filename1));
       #endif
      }

    if ( rc == DECLINED )
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: rc = DECLINED (FAILURE)",cn);
       mod_gzip_printf( "%s: Sending response 'as is'...",cn);
       mod_gzip_printf( "%s: Issuing 'goto mod_gzip_sendfile2_send_as_is'...",cn);
       #endif

       goto mod_gzip_sendfile2_send_as_is;
      }

    else 
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: rc = OK (SUCCESS)",cn);
       mod_gzip_printf( "%s: Call goto mod_gzip_sendfile2_cleanup...",cn,rc);
       #endif

       final_rc = rc; 

       goto mod_gzip_sendfile2_cleanup;
      }
   }

 else 
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: ERROR: bbytes_total is ZERO",cn);
    mod_gzip_printf( "%s: ERROR: Original response must be sent 'as is'",cn);
    mod_gzip_printf( "%s: Issuing 'goto mod_gzip_sendfile2_send_as_is'...",cn);
    #endif

    #ifdef MOD_GZIP_USES_APACHE_LOGS

    ap_table_setn( r->notes,"mod_gzip_result",
    ap_pstrdup(r->pool,"SEND_AS_IS:BODY_MISSING"));
    #endif

    goto mod_gzip_sendfile2_send_as_is;
   }

 mod_gzip_sendfile2_send_as_is: ;

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: * SEND 'AS IS'...",cn);
 mod_gzip_printf( "%s: * label entry point: mod_gzip_sendfile2_send_as_is",cn);
 mod_gzip_printf( "%s: Sending response 'as is'...",cn);
 #endif

 if ( !ifh )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Input file was closed.",cn);
    mod_gzip_printf( "%s: Re-opening input_filename[%s]...",
                      cn, npp(input_filename));
    #endif

    ifh = fopen( input_filename, "rb" ); 

    if ( !ifh ) 
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: .... fopen() FAILED",cn);
       #endif

       ap_log_error( APLOG_MARK,APLOG_NOERRNO|APLOG_ERR, r->server,
       "mod_gzip: fopen(%s) on REOPEN in sendfile2", input_filename );

       final_rc = DECLINED;

       goto mod_gzip_sendfile2_cleanup;
      }

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: .... fopen() SUCCEEDED",cn);
    #endif
   }

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Call mod_gzip_sendfile1()...",cn);
 #endif

 total_bytes_sent = (long)
 mod_gzip_sendfile1(
 r,    
 NULL, 
 ifh,  
 0     
 );

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Back mod_gzip_sendfile1()...",cn);
 mod_gzip_printf( "%s: .... total_bytes_sent = %ld",cn,(long)total_bytes_sent);
 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Call mod_gzip_flush_and_update_counts()...",cn);
 #endif

 mod_gzip_flush_and_update_counts(
 r,
 dconf,
 hbytes_in,                   
 total_bytes_sent - hbytes_in 
 );

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Back mod_gzip_flush_and_update_counts()...",cn);
 #endif

 final_rc = rc; 

 mod_gzip_sendfile2_cleanup: ;

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: * FINAL CLEANUP",cn);
 mod_gzip_printf( "%s: * label entry point: mod_gzip_sendfile2_cleanup",cn);
 mod_gzip_printf( "%s: dconf__keep_workfiles = %ld",cn,(long)dconf__keep_workfiles);
 mod_gzip_printf( "%s: ifh = %ld (Input  file handle)",cn,(long)ifh);
 #endif

 if ( ifh )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Input file is still OPEN...",cn );
    mod_gzip_printf( "%s: Call fclose(%s)...",cn, npp(input_filename));
    #endif

    fclose( ifh ); ifh = 0;

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Back fclose(%s)...",cn,npp(input_filename));
    #endif
   }
 #ifdef MOD_GZIP_DEBUG1
 else
   {
    mod_gzip_printf( "%s: Input file is already CLOSED...",cn );
    mod_gzip_printf( "%s: NO CALL MADE TO fclose(%s)",cn,npp(input_filename));
   }
 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Caller must delete input_filename=[%s]",
                   cn, npp(input_filename));
 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: ofh_used = %ld (Output file usage flag)",cn,(long)ofh_used);
 mod_gzip_printf( "%s: ofh      = %ld (Output file handle)",cn,(long)ofh);
 #endif

 if ( ofh_used ) 
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: 'ofh_used' is TRUE",cn );
    #endif

    if ( ofh ) 
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Output file is still OPEN...",cn );
       mod_gzip_printf( "%s: Call fclose(output_filename1=[%s])...",
                         cn, npp(output_filename1));
       #endif

       fclose( ofh ); ofh = 0;

       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Back fclose(output_filename1=[%s])...",
                         cn, npp(output_filename1));
       #endif
      }
    #ifdef MOD_GZIP_DEBUG1
    else
      {
       mod_gzip_printf( "%s: 'ofh' is NULL. File is already CLOSED.",cn );
      }
    #endif

    if ( !dconf__keep_workfiles )
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: 'dconf__keep_workfiles' is FALSE",cn );
       mod_gzip_printf( "%s: Call mod_gzip_delete_file(output_filename1=[%s])...",
                         cn, npp(output_filename1));
       #endif

       mod_gzip_delete_file( r, output_filename1 );

       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Back mod_gzip_delete_file(output_filename1=[%s])...",
                         cn, npp(output_filename1));
       #endif
      }

    #ifdef MOD_GZIP_DEBUG1
    else
      {
       mod_gzip_printf( "%s: 'dconf__keep_workfiles' is TRUE.",cn );
       mod_gzip_printf( "%s: Keeping output_filename1=[%s]",
                         cn, npp(output_filename1));
      }
    #endif

    ofh_used = 0; 
   }
 #ifdef MOD_GZIP_DEBUG1
 else
   {
    mod_gzip_printf( "%s: 'ofh_used' is FALSE",cn );
    mod_gzip_printf( "%s: Output file was NOT USED",cn );
   }
 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: final_rc was......: %d",cn,final_rc);
 #endif

 final_rc = OK;

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: final_rc is now...: %d",cn,final_rc);
 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Exit > return( final_rc = %d ) >",cn,final_rc);
 #endif

 return final_rc; 
}

long mod_gzip_send_header(
request_rec *r,
char        *input_filename,
long         content_length
)
{
 #ifdef MOD_GZIP_DEBUG1
 char cn[]="mod_gzip_send_header()";
 #endif

 FILE *ifh=0;

 int i;
 int bytesread=0;
 int ok_to_send=0;
 int valid_char_count=0;

 #define   MOD_GZIP_SEND_HEADER_BUFFER_SIZE 4096
 char tmp[ MOD_GZIP_SEND_HEADER_BUFFER_SIZE + 16 ];

 #define MOD_GZIP_LINE_BUFFER_SIZE 2048
 char lbuf[ MOD_GZIP_LINE_BUFFER_SIZE + 16 ];

 char *p1;
 int   p1len = 0;

 char *p2    = lbuf;
 int   p2len = 0;

 int  send_header = 0; 
 int  header_done = 0; 
 int  te_seen     = 0; 
 int  te_chunked  = 0; 
 int  ce_seen     = 0; 

 long hbytes_in   = 0; 
 long hbytes_out  = 0; 

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Entry...",cn);
 mod_gzip_printf( "%s: input_filename = [%s]",cn,npp(input_filename));
 mod_gzip_printf( "%s: content_length = %ld", cn,(long)content_length);
 #endif

 if ( !r              ) return 0;
 if ( !input_filename ) return 0;

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Call fopen(%s)...",cn,npp(input_filename));
 #endif

 ifh = fopen( input_filename, "rb" ); 

 if ( !ifh ) 
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: .... fopen() FAILED",cn);
    mod_gzip_printf( "%s: Exit > return( 0 ) >",cn);
    #endif

    return( 0 ); 
   }

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: .... fopen() SUCCEEDED",cn);
 mod_gzip_printf( "%s: sizeof( tmp ) = %d",cn,sizeof(tmp));
 mod_gzip_printf( "%s: MOD_GZIP_SEND_HEADER_BUFFER_SIZE = %d",
               cn,(int)MOD_GZIP_SEND_HEADER_BUFFER_SIZE);
 mod_gzip_printf( "%s: Processing header now...",cn);
 #endif

 send_header = 1;

 for (;;)
    {
     bytesread = fread( tmp, 1, MOD_GZIP_SEND_HEADER_BUFFER_SIZE, ifh );

     #ifdef MOD_GZIP_DEBUG1
     mod_gzip_printf( "%s: HEADER: Back fread(): bytesread=%d",cn,bytesread);
     #endif

     if ( bytesread < 1 ) break; 

     p1    = tmp;
     p1len = 0;

     for ( i=0; i<bytesread; i++ )
        {
         if ( *p1 == 10 )
           {
            *p2 = 0; 

            if ( valid_char_count < 1 )
              {
               p1++;         
               p1len++;      
               hbytes_in++;  

               sprintf( lbuf, "Content-Encoding: gzip" );

               #ifdef MOD_GZIP_DEBUG1
               mod_gzip_printf( "%s: HEADER: ADDING: lbuf=[%s]",cn,npp(lbuf));
               #endif

               mod_gzip_strcat( lbuf, "\r\n" );

               if ( send_header )
                 {
                  hbytes_out += mod_gzip_send(lbuf,mod_gzip_strlen(lbuf),r);
                 }

               sprintf( lbuf, "Content-Length: %ld",(long)content_length);

               #ifdef MOD_GZIP_DEBUG1
               mod_gzip_printf( "%s: HEADER: ADDING: lbuf=[%s]",cn,npp(lbuf));
               #endif

               mod_gzip_strcat( lbuf, "\r\n" );

               if ( send_header )
                 {
                  hbytes_out += mod_gzip_send(lbuf,mod_gzip_strlen(lbuf),r);
                 }

               #ifdef MOD_GZIP_DEBUG1
               mod_gzip_printf( "%s: HEADER: ADDING: lbuf=[CR/LF]",cn);
               #endif

               if ( send_header )
                 {
                  hbytes_out += mod_gzip_send("\r\n",2,r);
                 }

               header_done = 1; 

               break; 
              }
            else 
              {
               ok_to_send = 1; 

               if ( lbuf[0] == 'T' )
                 {
                  if ( mod_gzip_strnicmp(lbuf,"Transfer-Encoding:",18)==0)
                    {
                     #ifdef MOD_GZIP_DEBUG1
                     mod_gzip_printf( "%s: HEADER: * 'Transfer-Encoding:' seen",cn);
                     #endif

                     te_seen = 1; 

                     if ( mod_gzip_stringcontains( lbuf, "chunked" ) )
                       {
                        #ifdef MOD_GZIP_DEBUG1
                        mod_gzip_printf( "%s: HEADER: * 'Transfer-Encoding: chunked' seen",cn);
                        #endif

                        te_chunked = 1; 
                        ok_to_send = 0; 
                       }
                    }
                 }

               #ifdef FUTURE_USE
               else if ( lbuf[0] == 'E' )
                 {
                  if ( mod_gzip_strnicmp(lbuf,"ETag:",5)==0)
                    {
                     ok_to_send = 0; 
                    }
                 }
               #endif

               else if ( lbuf[0] == 'C' )
                 {
                  if ( mod_gzip_strnicmp(lbuf,"Content-Encoding:",17)==0)
                    {
                     ce_seen = 1; 
                    }
                  else if ( mod_gzip_strnicmp(lbuf,"Content-Length:",15)==0)
                    {
                     ok_to_send = 0; 
                    }
                 }

               if ( ok_to_send )
                 {
                  #ifdef MOD_GZIP_DEBUG1
                  mod_gzip_printf( "%s: HEADER: Sending lbuf=[%s]",cn,npp(lbuf));
                  #endif

                  *p2++ = 13; p2len++;
                  *p2++ = 10; p2len++;
                  *p2++ = 0;

                  if ( send_header )
                    {
                     hbytes_out += mod_gzip_send( lbuf, p2len, r );
                    }
                 }
               #ifdef MOD_GZIP_DEBUG1
               else
                 {
                  #ifdef MOD_GZIP_DEBUG1
                  mod_gzip_printf( "%s: HEADER: NOSEND: lbuf=[%s]",cn,npp(lbuf));
                  #endif
                 }
               #endif
              }

            p2    = lbuf;
            p2len = 0;

            valid_char_count = 0;
           }
         else 
           {
            if ( *p1 > 32 ) valid_char_count++;

            if (( p2len < MOD_GZIP_LINE_BUFFER_SIZE )&&( *p1 != 13 ))
              {
               *p2++ = *p1;
               p2len++;
              }
           }

         p1++;        
         p1len++;     
         hbytes_in++; 
        }

     if ( header_done ) break;
    }

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Done processing header...",cn);
 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Call fclose(%s)...",cn,npp(input_filename));
 #endif

 fclose( ifh ); ifh = 0;

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Back fclose(%s)...",cn,npp(input_filename));
 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: p1len        = %d",cn,p1len);
 mod_gzip_printf( "%s: hbytes_in    = %ld (Total HEADER bytes read)",cn,(long)hbytes_in);
 mod_gzip_printf( "%s: hbytes_out   = %ld (Total HEADER bytes sent)",cn,(long)hbytes_out);
 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Exit > return( hbytes_out = %d ) >",cn,hbytes_out);
 #endif

 return (long) hbytes_out; 
}

FILE *mod_gzip_open_output_file(
request_rec *r,
char *output_filename,
int  *rc
)
{
 FILE *ifh;

 #ifdef MOD_GZIP_DEBUG1
 char cn[]="mod_gzip_open_output_file():::";
 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Entry...",cn);
 mod_gzip_printf( "%s: output_filename=[%s]",cn,npp(output_filename));
 #endif

 ifh = fopen( output_filename, "rb" ); 

 if ( !ifh ) 
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: ERROR: Cannot open file [%s]",
                      cn,npp(output_filename));
    #endif

    ap_log_error( APLOG_MARK,APLOG_NOERRNO|APLOG_ERR, r->server,
    "mod_gzip: Cannot re-open output_filename=[%s]",
    output_filename );

    #ifdef MOD_GZIP_USES_APACHE_LOGS

    ap_table_setn(
    r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"SEND_AS_IS:WORK_OPENFAIL"));

    #endif 

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Exit > return( NULL ) >",cn);
    #endif

    *rc = DECLINED; 

    return NULL;
   }

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: File is now open...",cn);
 mod_gzip_printf( "%s: Exit > return( FILE *ifh ) >",cn);
 #endif

 *rc = OK; 

 return ifh; 
}

int mod_gzip_flush_and_update_counts(
request_rec   *r,
mod_gzip_conf *dconf,
long           total_header_bytes_sent,
long           total_body_bytes_sent
)
{
 #ifdef MOD_GZIP_DEBUG1
 char cn[]="mod_gzip_flush_and_update_counts()";
 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Entry",cn);
 mod_gzip_printf( "%s: total_header_bytes_sent = %ld",
              cn,(long)total_header_bytes_sent);
 mod_gzip_printf( "%s: total_body_bytes_sent   = %ld",
              cn,(long)total_body_bytes_sent);
 mod_gzip_printf( "%s: total_bytes_sent (Sum)  = %ld",
              cn,(long)total_header_bytes_sent +
                       total_body_bytes_sent );
 mod_gzip_printf( "%s: dconf->add_header_count = %d",
              cn,(int )dconf->add_header_count );
 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Before... ap_rflush()...",cn);
 mod_gzip_printf( "%s: r->connection->client->outcnt     = %ld",
             cn,(long) r->connection->client->outcnt );
 mod_gzip_printf( "%s: r->connection->client->bytes_sent = %ld",
             cn,(long) r->connection->client->bytes_sent );
 mod_gzip_printf( "%s: Call..... ap_rflush()...",cn);
 #endif

 ap_rflush(r);

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Back..... ap_rflush()...",cn);
 mod_gzip_printf( "%s: After.... ap_rflush()...",cn);
 mod_gzip_printf( "%s: r->connection->client->outcnt     = %ld",
             cn,(long) r->connection->client->outcnt );
 mod_gzip_printf( "%s: r->connection->client->bytes_sent = %ld",
             cn,(long) r->connection->client->bytes_sent );
 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: dconf->add_header_count = %ld",
              cn,(long)dconf->add_header_count);
 #endif

 if ( dconf->add_header_count )
   {
    r->connection->client->bytes_sent =
    total_header_bytes_sent + total_body_bytes_sent;
   }
 else 
   {
    r->connection->client->bytes_sent = total_body_bytes_sent;
   }

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: After count update...",cn);
 mod_gzip_printf( "%s: r->connection->client->outcnt     = %ld",
             cn,(long) r->connection->client->outcnt );
 mod_gzip_printf( "%s: r->connection->client->bytes_sent = %ld",
             cn,(long) r->connection->client->bytes_sent );
 mod_gzip_printf( "%s: Exit > return( 1 ) >",cn);
 #endif

 return 1;
}

int mod_gzip_encode_and_transmit(
request_rec   *r,
mod_gzip_conf *dconf,
char          *source,
int            source_is_a_file,
long           input_size,
int            nodecline,
long           header_length,
char          *result_prefix_string
)
{
 char *prefix_string = result_prefix_string;

 GZP_CONTROL   gzc;
 GZP_CONTROL*  gzp = &gzc;

 int   rc                = 0;
 int   err               = 0;
 FILE *ifh               = 0;
 int   bytesread         = 0;
 long  byteswritten      = 0;
 long  output_size       = 0;
 long  compression_ratio = 0;
 char *gz1_ismem_obuf    = 0;
 int   finalize_stats    = 1;
 long  output_offset     = 0;

 long  total_bytes_sent             = 0;
 long  total_header_bytes_sent      = 0;
 long  total_compressed_bytes_sent  = 0;
 int   gz1_ismem_obuf_was_allocated = 0;

 #define MOD_GZIP_LARGE_BUFFER_SIZE 4000

 char tmp[ MOD_GZIP_LARGE_BUFFER_SIZE + 2 ];

 char actual_content_encoding_name[] = "gzip";

 char dummy_result_prefix_string[] = "";

 #ifdef MOD_GZIP_DEBUG1
 char scratch2[90];
 #endif

 #ifdef MOD_GZIP_DEBUG1
 char cn[]="mod_gzip_encode_and_transmit()";
 #endif

 #ifdef MOD_GZIP_USES_APACHE_LOGS
 char log_info[90];
 #endif

 char *dconf__temp_dir           = 0;
 int   dconf__keep_workfiles     = 0;
 long  dconf__minimum_file_size  = 300;
 long  dconf__maximum_file_size  = 0;
 long  dconf__maximum_inmem_size = 0;

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Entry...", cn);
 #endif

 /*
  * Initialize the GZP control deck on the stack with some
  * safe default values. As of this writing the filename and
  * scratch buffers do not need to be fully reset on entry
  * so there is no need to suffer the overhead of a full
  * 'memset( gzp, 0, sizeof( GZP_CONTROL )' call.
  */

 gzp->decompress           = 0;
 gzp->input_ismem          = 0;
 gzp->input_ismem_ibuf     = 0;
 gzp->input_ismem_ibuflen  = 0;
 gzp->input_filename[0]    = 0;
 gzp->input_offset         = header_length;
 gzp->output_ismem         = 0;
 gzp->output_ismem_obuf    = 0;
 gzp->output_ismem_obuflen = 0;
 gzp->output_filename[0]   = 0;
 gzp->result_code          = 0;
 gzp->bytes_out            = 0;

 if ( dconf )
   {
    dconf__keep_workfiles     = dconf->keep_workfiles;
    dconf__minimum_file_size  = dconf->minimum_file_size;
    dconf__maximum_file_size  = dconf->maximum_file_size;
    dconf__maximum_inmem_size = dconf->maximum_inmem_size;
    dconf__temp_dir           = dconf->temp_dir;
   }

 #ifdef MOD_GZIP_DEBUG1

 mod_gzip_printf( "%s: dconf__keep_workfiles     = %d",  cn, (int) dconf__keep_workfiles );
 mod_gzip_printf( "%s: dconf__temp_dir           = [%s]",cn, npp(dconf__temp_dir));
 mod_gzip_printf( "%s: dconf__minimum_file_size  = %ld", cn, (long) dconf__minimum_file_size );
 mod_gzip_printf( "%s: dconf__maximum_file_size  = %ld", cn, (long) dconf__maximum_file_size );
 mod_gzip_printf( "%s: dconf__maximum_inmem_size = %ld", cn, (long) dconf__maximum_inmem_size );

 mod_gzip_printf( "%s: source_is_a_file = %d",  cn, source_is_a_file );
 mod_gzip_printf( "%s: nodecline        = %d",  cn, nodecline );
 mod_gzip_printf( "%s: header_length    = %ld", cn, header_length );

 if ( source_is_a_file )
   {
    mod_gzip_printf( "%s: source = [%s]", cn, npp(source));
   }
 else
   {
    mod_gzip_printf( "%s: source = MEMORY BUFFER", cn );
   }

 mod_gzip_printf( "%s: input_size = %ld", cn,(long)input_size);

 #endif

 #ifdef MOD_GZIP_USES_APACHE_LOGS

 if ( !prefix_string )
   {
    prefix_string = dummy_result_prefix_string;
   }

 sprintf( log_info,"%sOK", prefix_string );

 ap_table_setn(
 r->notes,"mod_gzip_result",ap_pstrdup(r->pool,log_info));

 sprintf( log_info,"%d", (int) input_size );
 ap_table_setn( r->notes,"mod_gzip_input_size",ap_pstrdup(r->pool,log_info));

 #endif

 if ( input_size < 1 )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: ERROR: Input source has no valid length.",cn);
    mod_gzip_printf( "%s: This request will not be processed...",cn);
    #endif

    #ifdef MOD_GZIP_USES_APACHE_LOGS

    sprintf( log_info,"%sDECLINED:NO_ILEN", prefix_string );

    ap_table_setn(
    r->notes,"mod_gzip_result",ap_pstrdup(r->pool,log_info));

    #endif

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Exit > return( DECLINED ) >",cn);
    #endif

    return DECLINED;
   }

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: dconf__minimum_file_size  = %ld",
            cn, (long) dconf__minimum_file_size );
 #endif

 if ( input_size < (long) dconf__minimum_file_size )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Source does not meet the minimum size requirement...",cn);
    #endif

    #ifdef MOD_GZIP_USES_APACHE_LOGS

    sprintf( log_info,"%sDECLINED:TOO_SMALL", prefix_string );

    ap_table_setn(
    r->notes,"mod_gzip_result",ap_pstrdup(r->pool,log_info));

    #endif

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Exit > return( DECLINED ) >",cn);
    #endif

    return DECLINED;
   }
 else
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Source meets the minimum size requirement.",cn);
    mod_gzip_printf( "%s: Assuming OK to proceed...",cn);
    #endif
   }

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: dconf__maximum_file_size = %ld",
            cn, (long) dconf__maximum_file_size );
 #endif

 if ( ( dconf__maximum_file_size > 0 ) &&
      ( input_size > (long) dconf__maximum_file_size ) )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Source exceeds the maximum size limit...",cn);
    #endif

    #ifdef MOD_GZIP_USES_APACHE_LOGS

    sprintf( log_info,"%sDECLINED:TOO_BIG", prefix_string );

    ap_table_setn(
    r->notes,"mod_gzip_result",ap_pstrdup(r->pool,log_info));

    #endif

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Exit > return( DECLINED ) >",cn);
    #endif

    return DECLINED;
   }
 else
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Source does not exceed the maximum size limit.",cn);
    mod_gzip_printf( "%s: Assuming OK to proceed...",cn);
    #endif
   }

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: dconf__maximum_inmem_size = %ld",
            cn, (long) dconf__maximum_inmem_size );
 #endif

 if ( source_is_a_file )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Input source is file[%s]",cn,npp(source));
    #endif

    mod_gzip_strcpy( gzp->input_filename, source );

    gzp->input_ismem         = 0;
    gzp->input_ismem_ibuf    = 0;
    gzp->input_ismem_ibuflen = 0;
   }
 else
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Input source is a MEMORY BUFFER",cn);
    #endif

    gzp->input_ismem         = 1;
    gzp->input_ismem_ibuf    = source;
    gzp->input_ismem_ibuflen = input_size;
   }

 gzp->decompress = 0;

 if ( dconf__maximum_inmem_size > (long) 60000L )
   {
    /* TODO: Some OS'es will have a 'malloc()' problem if the */
    /* in-memory size is greater than 64k so for now just set */
    /* 60k as the fixed upper limit. Expand this later. */

    /* NOTE: Testing has shown that for responses greater than */
    /* 60k or so the 'swap to disk' option SHOULD be used, anyway. */
    /* MOST responses will be far less than 60k. */

    dconf__maximum_inmem_size = (long) 60000L;
   }

 if ( input_size < (long) dconf__maximum_inmem_size )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Input source is small enough for in-memory compression.",cn);
    #endif

    *gzp->output_filename = 0;
     gzp->output_ismem    = 1;

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Call malloc( input_size + 1000 = %ld )...",
                     cn,(long)(input_size+1000));
    #endif

    gz1_ismem_obuf = (char *) malloc( input_size + 1000 );

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Back malloc( input_size + 1000 = %ld )...",
                     cn,(long)(input_size+1000));
    #endif

    if ( !gz1_ismem_obuf )
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: ERROR: Cannot allocate GZP memory...",cn);
       mod_gzip_printf( "%s: Defaulting to output file method... ",cn);
       #endif

       gzp->output_ismem = 0;
      }
    else
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: ( %ld + 1000 ) bytes allocated OK",cn,(long)input_size);
       #endif

       gz1_ismem_obuf_was_allocated = 1;

       /* TODO: This probably isn't necessary and just wastes time... */
       memset( gz1_ismem_obuf, 0, ( input_size + 1000 ) );

       gzp->output_ismem_obuf    = gz1_ismem_obuf;
       gzp->output_ismem_obuflen = input_size + 1000;
      }
   }
 #ifdef MOD_GZIP_DEBUG1
 else
   {
    mod_gzip_printf( "%s: Input source is too large for in-memory compression.",cn);
   }
 #endif

 if ( gzp->output_ismem != 1 )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Ouput target is a FILE...",cn);
    #endif

    mod_gzip_create_unique_filename(
    dconf__temp_dir,
    (char *) gzp->output_filename,
    MOD_GZIP_MAX_PATH_LEN
    );

    gzp->output_ismem         = 0;
    gz1_ismem_obuf            = 0;
    gzp->output_ismem_obuf    = 0;
    gzp->output_ismem_obuflen = 0;
   }

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: gzp->decompress      = %d"  ,cn,gzp->decompress);
 mod_gzip_printf( "%s: gzp->input_ismem     = %d",  cn,gzp->input_ismem);
 mod_gzip_printf( "%s: gzp->output_ismem    = %d",  cn,gzp->output_ismem);
 mod_gzip_printf( "%s: gzp->input_filename  = [%s]",cn,npp(gzp->input_filename));
 mod_gzip_printf( "%s: gzp->input_offset    = %ld", cn,gzp->input_offset);
 mod_gzip_printf( "%s: gzp->output_filename = [%s]",cn,npp(gzp->output_filename));
 mod_gzip_printf( "%s: Call gzp_main(r,gzp)...",cn);
 #endif

 rc = gzp_main( r, gzp );

 output_size = (long) gzp->bytes_out;

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Back gzp_main(r,gzp)...",cn);
 mod_gzip_printf( "%s: input_size     = %ld",cn,(long)input_size);
 mod_gzip_printf( "%s: output_size    = %ld",cn,(long)output_size);
 mod_gzip_printf( "%s: gzp->bytes_out = %ld",cn,(long)gzp->bytes_out);
 mod_gzip_printf( "%s: Bytes saved    = %ld",cn,
                 (long)input_size-gzp->bytes_out );
 #endif

 compression_ratio = 0;

 if ( ( input_size  > 0 ) &&
      ( output_size > 0 ) )
   {
    compression_ratio = 100 - (int)
    ( output_size * 100L / input_size );
   }

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Compression    = %ld percent",cn,
          (long) compression_ratio );
 #endif

 #ifdef MOD_GZIP_USES_APACHE_LOGS

 sprintf( log_info,"%d", (int) output_size );
 ap_table_setn( r->notes,"mod_gzip_output_size",ap_pstrdup(r->pool,log_info));

 sprintf( log_info,"%d", (int) compression_ratio );
 ap_table_setn( r->notes,"mod_gzip_compression_ratio",ap_pstrdup(r->pool,log_info));

 #endif

 if ( output_size < 1 )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Compressed version has no length.",cn);
    mod_gzip_printf( "%s: Sending the original version uncompressed...",cn);
    #endif

    finalize_stats = 0;

    #ifdef MOD_GZIP_USES_APACHE_LOGS

    sprintf( log_info,"%sDECLINED:NO_OLEN", prefix_string );

    ap_table_setn(
    r->notes,"mod_gzip_result",ap_pstrdup(r->pool,log_info));

    #endif

    if ( gz1_ismem_obuf )
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: gz1_ismem_obuf is still a VALID pointer",cn);
       mod_gzip_printf( "%s: gz1_ismem_obuf_was_allocated = %d",
                    cn,(int) gz1_ismem_obuf_was_allocated );
       #endif

       if ( gz1_ismem_obuf_was_allocated )
         {
          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: Call free( gz1_ismem_obuf )...",cn);
          #endif

          free( gz1_ismem_obuf );

          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: Back free( gz1_ismem_obuf )...",cn);
          #endif

          gz1_ismem_obuf = 0;
          gz1_ismem_obuf_was_allocated = 0;
         }
      }

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Exit > return( DECLINED ) >",cn);
    #endif

    return DECLINED;
   }

 if ( output_size > input_size )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Compressed version is larger than original.",cn);
    mod_gzip_printf( "%s: Sending the original version uncompressed...",cn);
    #endif

    finalize_stats = 0;

    #ifdef MOD_GZIP_USES_APACHE_LOGS

    sprintf( log_info,"%sDECLINED:ORIGINAL_SMALLER", prefix_string );

    ap_table_setn(
    r->notes,"mod_gzip_result",ap_pstrdup(r->pool,log_info));

    #endif

    if ( gz1_ismem_obuf )
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: gz1_ismem_obuf is still a VALID pointer",cn);
       mod_gzip_printf( "%s: gz1_ismem_obuf_was_allocated = %d",
                    cn,(int) gz1_ismem_obuf_was_allocated );
       #endif

       if ( gz1_ismem_obuf_was_allocated )
         {
          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: Call free( gz1_ismem_obuf )...",cn);
          #endif

          free( gz1_ismem_obuf );

          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: Back free( gz1_ismem_obuf )...",cn);
          #endif

          gz1_ismem_obuf = 0;
          gz1_ismem_obuf_was_allocated = 0;
         }
      }

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Exit > return( DECLINED ) >",cn);
    #endif

    return DECLINED;
   }
 #ifdef MOD_GZIP_DEBUG1
 else
   {
    mod_gzip_printf( "%s: Compressed version is smaller than original.",cn);
    mod_gzip_printf( "%s: Sending the compressed version...",cn);
   }
 #endif

 if ( !gzp->output_ismem )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Re-opening compressed output file [%s]...",
             cn, npp(gzp->output_filename));
    #endif

    ifh = mod_gzip_open_output_file( r, gzp->output_filename, &rc );

    if ( !ifh )
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: ERROR: Cannot re-open file [%s]",
                cn,npp(gzp->output_filename));
       #endif

       #ifdef MOD_GZIP_USES_APACHE_LOGS

       sprintf( log_info,"%sDECLINED:REOPEN_FAILED", prefix_string );

       ap_table_setn(
       r->notes,"mod_gzip_result",ap_pstrdup(r->pool,log_info));

       #endif

       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Exit > return( DECLINED ) >",cn);
       #endif

       return DECLINED;
      }

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Workfile re-opened OK...",cn);
    #endif
   }

 /* TODO: This isn't really necessary? */
 r->content_encoding = actual_content_encoding_name;

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: r->content_encoding is now [%s]",
               cn, npp(r->content_encoding));
 #endif

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Starting transmit phase...",cn);
 mod_gzip_printf( "%s: output_offset   = %ld",cn, (long) output_offset );
 #endif

 /* Send the response header... */

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Call mod_gzip_send_header(source=%s)",cn,npp(source));
 #endif

 total_header_bytes_sent = (int)
 mod_gzip_send_header(
 r,
 source,
 output_size
 );

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Back mod_gzip_send_header(source=%s)",cn,npp(source));
 mod_gzip_printf( "%s: total_header_bytes_sent = %d",cn,rc);
 #endif

 total_compressed_bytes_sent = 0;

 /* Send the compressed data.. */

 if ( gzp->output_ismem )
   {
    #ifdef MOD_GZIP_DEBUG1

    mod_gzip_printf( "%s: Sending in-memory output buffer...",cn);
    mod_gzip_printf( "%s: output_size   = %ld",cn,(long)output_size);
    mod_gzip_printf( "%s: output_offset = %ld",cn,(long)output_offset);

    #ifdef  MOD_GZIP_DUMP_JUST_BEFORE_SENDING
    mod_gzip_hexdump( gz1_ismem_obuf, output_size );
    #endif

    mod_gzip_printf( "%s: Call mod_gzip_send( gz1_ismem_obuf+output_offset=%ld, output_size=%ld )...",
                     cn, (long) output_offset, (long) output_size );
    #endif

    byteswritten = (long)
    mod_gzip_send( gz1_ismem_obuf+output_offset, output_size, r );

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Back mod_gzip_send( gz1_ismem_obuf+output_offset=%ld, output_size=%ld )...",
                     cn, (long) output_offset, (long) output_size );
    mod_gzip_printf( "%s: byteswritten = %ld",cn,(long)byteswritten);
    #endif

    if ( byteswritten > 0 )
      {
       total_compressed_bytes_sent = byteswritten;
      }

    if ( byteswritten != output_size )
      {
       err = errno;

       #ifdef FUTURE_USE
       #if defined(WIN32) || defined(NETWARE)
       err = WSAGetLastError();
       #else
       err = errno;
       #endif
       #endif

       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: TRANSMIT ERROR: output_size=%ld byteswritten=%ld err=%d",
       cn,(long)output_size,(long)byteswritten,(int)err );
       #endif

       #ifdef MOG_GZIP_DEBUG1

       mod_gzip_log_comerror( r, "mod_gzip: TRANSMIT_ERROR:", err );

       #else

       ap_log_error( APLOG_MARK,APLOG_NOERRNO|APLOG_ERR, r->server,
       "mod_gzip: TRANSMIT_ERROR:ISMEM:%d",(int)err);

       #endif

       #ifdef MOD_GZIP_USES_APACHE_LOGS

       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_translate_comerror( err, scratch2 );
       sprintf( log_info,"%sTRANSMIT_ERROR:ISMEM:%d:%s", prefix_string, (int) err, scratch2 );
       #else
       sprintf( log_info,"%sTRANSMIT_ERROR:ISMEM:%d", prefix_string, (int) err );
       #endif

       ap_table_setn( r->notes,"mod_gzip_result",ap_pstrdup(r->pool,log_info));

       #endif
      }
   }
 else
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: sizeof( tmp )              = %d",cn,sizeof(tmp));
    mod_gzip_printf( "%s: MOD_GZIP_LARGE_BUFFER_SIZE = %d",cn,(int)MOD_GZIP_LARGE_BUFFER_SIZE);
    mod_gzip_printf( "%s: Transmit buffer size       = %d",cn,sizeof(tmp));
    mod_gzip_printf( "%s: Sending output file...",cn);
    #endif

    if ( output_offset > 0 )
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: output_offset = %ld",cn,(long)output_offset);
       mod_gzip_printf( "%s: Call fseek(ifh,%ld,1)...",cn,(long)output_offset);
       #endif

       fseek( ifh, (long) output_offset, 1 );

       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Back fseek(ifh,%ld,1)...",cn,(long)output_offset);
       #endif
      }
    else
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: output_offset = %ld = ZERO",cn,(long)output_offset);
       mod_gzip_printf( "%s: No fseek() call required before transmit.",cn);
       #endif
      }

    for (;;)
       {
        bytesread = fread( tmp, 1, MOD_GZIP_LARGE_BUFFER_SIZE, ifh );

        #ifdef MOD_GZIP_DEBUG1
        mod_gzip_printf( "%s: Back fread(): bytesread=%d",cn,bytesread);
        #endif

        if ( bytesread < 1 ) break;

        byteswritten = (long)
        mod_gzip_send( tmp, bytesread, r );

        #ifdef MOD_GZIP_DEBUG1
        mod_gzip_printf( "%s: byteswritten = %ld",cn,(long)byteswritten);
        #endif

        if ( byteswritten > 0 )
          {
           total_compressed_bytes_sent += byteswritten;
          }

        if ( byteswritten != bytesread )
          {
           err = errno;

           #ifdef FUTURE_USE
           #if defined(WIN32) || defined(NETWARE)
           err = WSAGetLastError();
           #else
           err = errno;
           #endif
           #endif

           #ifdef MOD_GZIP_DEBUG1
           mod_gzip_printf( "%s: TRANSMIT ERROR: bytesread=%ld byteswritten=%ld err=%d",
           cn,(long)bytesread,(long)byteswritten,(int)err );
           #endif

           #ifdef MOG_GZIP_DEBUG1

           mod_gzip_log_comerror( r, "mod_gzip: TRANSMIT_ERROR:", err );

           #else

           ap_log_error( APLOG_MARK,APLOG_NOERRNO|APLOG_ERR, r->server,
           "mod_gzip: TRANSMIT_ERROR:%d",(int)err);

           #endif

           #ifdef MOD_GZIP_USES_APACHE_LOGS

           #ifdef MOD_GZIP_DEBUG1
           mod_gzip_translate_comerror( err, scratch2 );
           sprintf( log_info,"%sTRANSMIT_ERROR:%d:%s", prefix_string, (int) err, scratch2 );
           #else
           sprintf( log_info,"%sTRANSMIT_ERROR:%d", prefix_string, (int) err );
           #endif

           ap_table_setn( r->notes,"mod_gzip_result",ap_pstrdup(r->pool,log_info));

           #endif

           #ifdef MOD_GZIP_DEBUG1
           mod_gzip_printf( "%s: Breaking out of transmit loop early...",cn);
           #endif

           break;
          }

       }/* End for(;;) loop that transmits workfile... */

   }/* End 'else' */

 /* Flush the output and update counts... */

 total_bytes_sent =
 total_header_bytes_sent +
 total_compressed_bytes_sent;

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Done Sending compressed data...",cn);
 mod_gzip_printf( "%s: total_header_bytes_sent     = %ld",
              cn,(long)total_header_bytes_sent);
 mod_gzip_printf( "%s: total_compressed_bytes_sent = %ld",
              cn,(long)total_compressed_bytes_sent);
 mod_gzip_printf( "%s: total_bytes_sent (Sum)      = %ld",
              cn,(long)total_bytes_sent);
 mod_gzip_printf( "%s: Call mod_gzip_flush_and_update_counts()...",cn);
 #endif

 mod_gzip_flush_and_update_counts(
 r,
 dconf,
 total_header_bytes_sent,
 total_compressed_bytes_sent
 );

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Back mod_gzip_flush_and_update_counts()...",cn);
 #endif

 /* Cleanup... */

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Cleanup phase START...",cn);
 #endif

 if ( gzp->output_ismem )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Output source was MEMORY...",cn);
    #endif

    if ( gz1_ismem_obuf )
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: gz1_ismem_obuf is still VALID...",cn);
       #endif

       if ( gz1_ismem_obuf_was_allocated )
         {
          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: gz1_ismem_obuf_was_allocated = TRUE",cn);
          mod_gzip_printf( "%s: Call free( gz1_ismem_obuf )...",cn);
          #endif

          free( gz1_ismem_obuf );

          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: Back free( gz1_ismem_obuf )...",cn);
          #endif

          gz1_ismem_obuf = 0;
          gz1_ismem_obuf_was_allocated = 0;
         }
      }
   }
 else
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Output source was a WORKFILE...",cn);
    mod_gzip_printf( "%s: Closing workfile [%s]...",cn,npp(gzp->output_filename));
    #endif

    fclose( ifh );
    ifh = 0;

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: dconf__keep_workfiles = %d",
                      cn, dconf__keep_workfiles );
    #endif

    if ( !dconf__keep_workfiles )
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Deleting workfile [%s]...",
                cn, npp(gzp->output_filename));
       #endif

       #ifdef WIN32
       DeleteFile( gzp->output_filename );
       #else
       unlink( gzp->output_filename );
       #endif
      }
    else
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Keeping workfile [%s]...",
                         cn, npp(gzp->output_filename));
       #endif
      }
   }

 #ifdef MOD_GZIP_USES_APACHE_LOGS

 if ( finalize_stats )
   {
    sprintf( log_info,"%d", (int) output_size );
    ap_table_setn( r->notes,"mod_gzip_output_size",ap_pstrdup(r->pool,log_info));

    sprintf( log_info,"%d", (int) compression_ratio );
    ap_table_setn( r->notes,"mod_gzip_compression_ratio",ap_pstrdup(r->pool,log_info));
   }

 #endif

 if ( r->server->loglevel == APLOG_DEBUG )
   {
    ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, r->server,
    "mod_gzip: r->uri=[%s] OK: Bytes In:%ld Out:%ld Compression: %ld pct.",
    r->uri,
    (long) input_size,
    (long) output_size,
    (long) compression_ratio
    );
   }

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Exit > return( OK ) >",cn);
 #endif

 return OK;
}

/*--------------------------------------------------------------------------*/
/* COMPRESSION_SUPPORT: START                                               */
/*--------------------------------------------------------------------------*/

#define BIG_MEM

typedef unsigned       uns;
typedef unsigned int   uni;
typedef unsigned char  uch;
typedef unsigned short ush;
typedef unsigned long  ulg;
typedef int            gz1_file_t;

#ifdef __STDC__
   typedef void *voidp;
#else
   typedef char *voidp;
#endif

#if defined(__MSDOS__) && !defined(MSDOS)
#  define MSDOS
#endif

#if defined(__OS2__) && !defined(OS2)
#  define OS2
#endif

#if defined(OS2) && defined(MSDOS)
#  undef MSDOS
#endif

#ifdef MSDOS
#  ifdef __GNUC__
#    define near
#  else
#    define MAXSEG_64K
#    ifdef __TURBOC__
#      define NO_OFF_T
#      ifdef __BORLANDC__
#        define DIRENT
#      else
#        define NO_UTIME
#      endif
#    else
#      define HAVE_SYS_UTIME_H
#      define NO_UTIME_H
#    endif
#  endif
#  define PATH_SEP2 '\\'
#  define PATH_SEP3 ':'
#  define MAX_PATH_LEN  128
#  define NO_MULTIPLE_DOTS
#  define MAX_EXT_CHARS 3
#  define Z_SUFFIX "z"
#  define NO_CHOWN
#  define PROTO
#  define STDC_HEADERS
#  define NO_SIZE_CHECK
#  define casemap(c) tolow(c)
#  include <io.h>
#  undef  OS_CODE
#  define OS_CODE  0x00
#  define SET_BINARY_MODE(fd) setmode(fd, O_BINARY)
#  if !defined(NO_ASM) && !defined(ASMV)
#    define ASMV
#  endif
#else
#  define near
#endif

#ifdef OS2
#  define PATH_SEP2 '\\'
#  define PATH_SEP3 ':'
#  define MAX_PATH_LEN  260
#  ifdef OS2FAT
#    define NO_MULTIPLE_DOTS
#    define MAX_EXT_CHARS 3
#    define Z_SUFFIX "z"
#    define casemap(c) tolow(c)
#  endif
#  define NO_CHOWN
#  define PROTO
#  define STDC_HEADERS
#  include <io.h>
#  undef  OS_CODE
#  define OS_CODE  0x06
#  define SET_BINARY_MODE(fd) setmode(fd, O_BINARY)
#  ifdef _MSC_VER
#    define HAVE_SYS_UTIME_H
#    define NO_UTIME_H
#    define MAXSEG_64K
#    undef near
#    define near _near
#  endif
#  ifdef __EMX__
#    define HAVE_SYS_UTIME_H
#    define NO_UTIME_H
#    define DIRENT
#    define EXPAND(argc,argv) \
       {_response(&argc, &argv); _wildcard(&argc, &argv);}
#  endif
#  ifdef __BORLANDC__
#    define DIRENT
#  endif
#  ifdef __ZTC__
#    define NO_DIR
#    define NO_UTIME_H
#    include <dos.h>
#    define EXPAND(argc,argv) \
       {response_expand(&argc, &argv);}
#  endif
#endif

#ifdef WIN32
#  define HAVE_SYS_UTIME_H
#  define NO_UTIME_H
#  define PATH_SEP2 '\\'
#  define PATH_SEP3 ':'
#  undef  MAX_PATH_LEN
#  define MAX_PATH_LEN  260
#  define NO_CHOWN
#  define PROTO
#  define STDC_HEADERS
#  define SET_BINARY_MODE(fd) setmode(fd, O_BINARY)
#  include <io.h>
#  include <malloc.h>
#  ifdef NTFAT
#    define NO_MULTIPLE_DOTS
#    define MAX_EXT_CHARS 3
#    define Z_SUFFIX "z"
#    define casemap(c) tolow(c)
#  endif
#  undef  OS_CODE

#  define OS_CODE  0x00

#endif

#ifdef MSDOS
#  ifdef __TURBOC__
#    include <alloc.h>
#    define DYN_ALLOC
     void * fcalloc (unsigned items, unsigned size);
     void fcfree (void *ptr);
#  else
#    include <malloc.h>
#    define fcalloc(nitems,itemsize) halloc((long)(nitems),(itemsize))
#    define fcfree(ptr) hfree(ptr)
#  endif
#else
#  ifdef MAXSEG_64K
#    define fcalloc(items,size) calloc((items),(size))
#  else
#    define fcalloc(items,size) malloc((size_t)(items)*(size_t)(size))
#  endif
#  define fcfree(ptr) free(ptr)
#endif

#if defined(VAXC) || defined(VMS)
#  define PATH_SEP ']'
#  define PATH_SEP2 ':'
#  define SUFFIX_SEP ';'
#  define NO_MULTIPLE_DOTS
#  define Z_SUFFIX "-gz"
#  define RECORD_IO 1
#  define casemap(c) tolow(c)
#  undef  OS_CODE
#  define OS_CODE  0x02
#  define OPTIONS_VAR "GZIP_OPT"
#  define STDC_HEADERS
#  define NO_UTIME
#  define EXPAND(argc,argv) vms_expand_args(&argc,&argv);
#  include <file.h>
#  define unlink delete
#  ifdef VAXC
#    define NO_FCNTL_H
#    include <unixio.h>
#  endif
#endif

#ifdef AMIGA
#  define PATH_SEP2 ':'
#  define STDC_HEADERS
#  undef  OS_CODE
#  define OS_CODE  0x01
#  define ASMV
#  ifdef __GNUC__
#    define DIRENT
#    define HAVE_UNISTD_H
#  else
#    define NO_STDIN_FSTAT
#    define SYSDIR
#    define NO_SYMLINK
#    define NO_CHOWN
#    define NO_FCNTL_H
#    include <fcntl.h>
#    define direct dirent
     extern void _expand_args(int *argc, char ***argv);
#    define EXPAND(argc,argv) _expand_args(&argc,&argv);
#    undef  O_BINARY
#  endif
#endif

#if defined(ATARI) || defined(atarist)
#  ifndef STDC_HEADERS
#    define STDC_HEADERS
#    define HAVE_UNISTD_H
#    define DIRENT
#  endif
#  define ASMV
#  undef  OS_CODE
#  define OS_CODE  0x05
#  ifdef TOSFS
#    define PATH_SEP2 '\\'
#    define PATH_SEP3 ':'
#    define MAX_PATH_LEN  128
#    define NO_MULTIPLE_DOTS
#    define MAX_EXT_CHARS 3
#    define Z_SUFFIX "z"
#    define NO_CHOWN
#    define casemap(c) tolow(c)
#    define NO_SYMLINK
#  endif
#endif

#ifdef MACOS
#  define PATH_SEP ':'
#  define DYN_ALLOC
#  define PROTO
#  define NO_STDIN_FSTAT
#  define NO_CHOWN
#  define NO_UTIME
#  define chmod(file, mode) (0)
#  define OPEN(name, flags, mode) open(name, flags)
#  define SEEKFORWARD(handle, offset) lseek( handle, offset, 1 )
#  undef  OS_CODE
#  define OS_CODE  0x07
#  ifdef MPW
#    define isatty(fd) ((fd) <= 2)
#  endif
#endif

#ifdef __50SERIES
#  define PATH_SEP '>'
#  define STDC_HEADERS
#  define NO_MEMORY_H
#  define NO_UTIME_H
#  define NO_UTIME
#  define NO_CHOWN 
#  define NO_STDIN_FSTAT 
#  define NO_SIZE_CHECK 
#  define NO_SYMLINK
#  define RECORD_IO  1
#  define casemap(c)  tolow(c)
#  define put_char(c) put_byte((c) & 0x7F)
#  define get_char(c) ascii2pascii(get_byte())
#  undef  OS_CODE
#  define OS_CODE  0x0F
#  ifdef SIGTERM
#    undef SIGTERM
#  endif
#endif

#if defined(pyr) && !defined(NOMEMCPY)
#  define NOMEMCPY
#endif

#ifdef TOPS20
#  undef  OS_CODE
#  define OS_CODE  0x0a
#endif

#ifndef unix
#  define NO_ST_INO
#endif

#ifndef OS_CODE
#  undef  OS_CODE
#  define OS_CODE  0x03
#endif

#ifndef PATH_SEP
#  define PATH_SEP '/'
#endif

#ifndef casemap
#  define casemap(c) (c)
#endif

#ifndef OPTIONS_VAR
#  define OPTIONS_VAR "GZIP"
#endif

#ifndef Z_SUFFIX
#  define Z_SUFFIX ".gz"
#endif

#ifdef MAX_EXT_CHARS
#  define MAX_SUFFIX  MAX_EXT_CHARS
#else
#  define MAX_SUFFIX  30
#endif

#ifndef MIN_PART
#  define MIN_PART 3
#endif

#ifndef EXPAND
#  define EXPAND(argc,argv)
#endif

#ifndef RECORD_IO
#  define RECORD_IO 0
#endif

#ifndef SET_BINARY_MODE
#  define SET_BINARY_MODE(fd)
#endif

#ifndef OPEN
#  define OPEN(name, flags, mode) open(name, flags, mode)
#endif

#ifndef SEEKFORWARD
#  define SEEKFORWARD(handle, offset) lseek( handle, offset, 1 )
#endif

#ifndef get_char
#  define get_char() get_byte()
#endif

#ifndef put_char
#  define put_char(c) put_byte(c)
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

#define OK          0
#define LZ1_ERROR   1
#define WARNING     2
#define STORED      0
#define COMPRESSED  1
#define PACKED      2
#define LZHED       3
#define DEFLATED    8
#define MAX_METHODS 9

#ifndef O_CREAT
#include <sys/file.h>
#ifndef O_CREAT
#define O_CREAT FCREAT
#endif
#ifndef O_EXCL
#define O_EXCL FEXCL
#endif
#endif

#ifndef S_IRUSR
#define S_IRUSR 0400
#endif
#ifndef S_IWUSR
#define S_IWUSR 0200
#endif
#define RW_USER (S_IRUSR | S_IWUSR)

#ifndef MAX_PATH_LEN
#define MAX_PATH_LEN 256
#endif

#ifndef SEEK_END
#define SEEK_END 2
#endif

#define PACK_MAGIC     "\037\036"
#define GZIP_MAGIC     "\037\213"
#define OLD_GZIP_MAGIC "\037\236"
#define LZH_MAGIC      "\037\240"
#define PKZIP_MAGIC    "\120\113\003\004"
#define ASCII_FLAG   0x01 
#define CONTINUATION 0x02 
#define EXTRA_FIELD  0x04 
#define ORIG_NAME    0x08 
#define COMMENT      0x10 
#define ENCRYPTED    0x20 
#define RESERVED     0xC0 
#define UNKNOWN 0xffff
#define BINARY  0
#define ASCII   1

#ifndef WSIZE
#define WSIZE 0x8000
#endif

#ifndef INBUFSIZ
#ifdef  SMALL_MEM
#define INBUFSIZ  0x2000
#else
#define INBUFSIZ  0x8000
#endif
#endif
#define INBUF_EXTRA 64

#ifndef	OUTBUFSIZ
#ifdef SMALL_MEM
#define OUTBUFSIZ   8192
#else
#define OUTBUFSIZ  0x4000
#endif
#endif
#define OUTBUF_EXTRA 2048

#ifndef DIST_BUFSIZE
#ifdef  SMALL_MEM
#define DIST_BUFSIZE 0x2000
#else
#define DIST_BUFSIZE 0x8000
#endif
#endif

#ifndef BITS
#define BITS 16
#endif

#define LZW_MAGIC  "\037\235"

#define MIN_MATCH  3
#define MAX_MATCH  258

#define MIN_LOOKAHEAD (MAX_MATCH+MIN_MATCH+1)
#define MAX_DIST  (WSIZE-MIN_LOOKAHEAD)

#ifdef  SMALL_MEM
#define HASH_BITS  13
#endif
#ifdef  MEDIUM_MEM
#define HASH_BITS  14
#endif
#ifndef HASH_BITS
#define HASH_BITS  15
#endif

#define HASH_SIZE (unsigned)(1<<HASH_BITS)
#define HASH_MASK (HASH_SIZE-1)
#define WMASK     (WSIZE-1)
#define H_SHIFT   ((HASH_BITS+MIN_MATCH-1)/MIN_MATCH)

#ifndef TOO_FAR
#define TOO_FAR 4096
#endif

#define NIL          0
#define FAST         4
#define SLOW         2
#define REP_3_6      16
#define REPZ_3_10    17
#define REPZ_11_138  18
#define MAX_BITS     15
#define MAX_BL_BITS  7
#define D_CODES      30
#define BL_CODES     19
#define SMALLEST     1
#define LENGTH_CODES 29
#define LITERALS     256
#define END_BLOCK    256
#define L_CODES (LITERALS+1+LENGTH_CODES)

#ifndef LIT_BUFSIZE
#ifdef  SMALL_MEM
#define LIT_BUFSIZE  0x2000
#else
#ifdef  MEDIUM_MEM
#define LIT_BUFSIZE  0x4000
#else
#define LIT_BUFSIZE  0x8000
#endif
#endif
#endif

#define HEAP_SIZE (2*L_CODES+1)
#define STORED_BLOCK 0
#define STATIC_TREES 1
#define DYN_TREES    2
#define NO_FILE  (-1) 

#define BMAX 16         
#define N_MAX 288       

#define LOCSIG 0x04034b50L      
#define LOCFLG 6                
#define CRPFLG 1                
#define EXTFLG 8                
#define LOCHOW 8                
#define LOCTIM 10               
#define LOCCRC 14               
#define LOCSIZ 18               
#define LOCLEN 22               
#define LOCFIL 26               
#define LOCEXT 28               
#define LOCHDR 30               
#define EXTHDR 16               
#define RAND_HEAD_LEN  12
#define BUFSIZE (8 * 2*sizeof(char))

#define translate_eol 0  

#define FLUSH_BLOCK(eof) \
   flush_block(gz1,gz1->block_start >= 0L ? (char*)&gz1->window[(unsigned)gz1->block_start] : \
         (char*)NULL, (long)gz1->strstart - gz1->block_start, (eof))

#ifdef DYN_ALLOC
#  define ALLOC(type, array, size) { \
      array = (type*)fcalloc((size_t)(((size)+1L)/2), 2*sizeof(type)); \
      if (array == NULL) error("insufficient memory"); \
   }
#  define FREE(array) {if (array != NULL) fcfree(array), array=NULL;}
#else
#  define ALLOC(type, array, size)
#  define FREE(array)
#endif

#define GZ1_MAX(a,b) (a >= b ? a : b)

#define tolow(c)  (isupper(c) ? (c)-'A'+'a' : (c))    

#define smaller(tree, n, m) \
   (tree[n].fc.freq < tree[m].fc.freq || \
   (tree[n].fc.freq == tree[m].fc.freq && gz1->depth[n] <= gz1->depth[m]))

#define send_code(c, tree) send_bits(gz1,tree[c].fc.code, tree[c].dl.len)

#define put_byte(c) {gz1->outbuf[gz1->outcnt++]=(uch)(c); if (gz1->outcnt==OUTBUFSIZ)\
                     flush_outbuf(gz1);}

#define put_short(w) \
{ if (gz1->outcnt < OUTBUFSIZ-2) { \
    gz1->outbuf[gz1->outcnt++] = (uch) ((w) & 0xff); \
    gz1->outbuf[gz1->outcnt++] = (uch) ((ush)(w) >> 8); \
  } else { \
    put_byte((uch)((w) & 0xff)); \
    put_byte((uch)((ush)(w) >> 8)); \
  } \
}

#define put_long(n) { \
    put_short((n) & 0xffff); \
    put_short(((ulg)(n)) >> 16); \
}

#ifdef CRYPT

#  define NEXTBYTE() \
     (decrypt ? (cc = get_byte(), zdecode(cc), cc) : get_byte())
#else
#  define NEXTBYTE() (uch)get_byte()
#endif

#define NEEDBITS(n) {while(k<(n)){b|=((ulg)NEXTBYTE())<<k;k+=8;}}
#define DUMPBITS(n) {b>>=(n);k-=(n);}

#define SH(p) ((ush)(uch)((p)[0]) | ((ush)(uch)((p)[1]) << 8))
#define LG(p) ((ulg)(SH(p)) | ((ulg)(SH((p)+2)) << 16))

#define put_ubyte(c) {gz1->window[gz1->outcnt++]=(uch)(c); if (gz1->outcnt==WSIZE)\
   flush_window(gz1);}

#define WARN(msg) { if (gz1->exit_code == OK) gz1->exit_code = WARNING; }

#define get_byte()  (gz1->inptr < gz1->insize ? gz1->inbuf[gz1->inptr++] : fill_inbuf(gz1,0))
#define try_byte()  (gz1->inptr < gz1->insize ? gz1->inbuf[gz1->inptr++] : fill_inbuf(gz1,1))

#define d_code(dist) \
   ((dist) < 256 ? gz1->dist_code[dist] : gz1->dist_code[256+((dist)>>7)])

typedef struct config {
   ush good_length; 
   ush max_lazy;    
   ush nice_length; 
   ush max_chain;
} config;

config configuration_table[10] = {

 {0,    0,  0,    0},  
 {4,    4,  8,    4},  
 {4,    5, 16,    8},
 {4,    6, 32,   32},
 {4,    4, 16,   16},  
 {8,   16, 32,   32},
 {8,   16, 128, 128},
 {8,   32, 128, 256},
 {32, 128, 258, 1024},
 {32, 258, 258, 4096}}; 

typedef struct ct_data {

    union {
        ush  freq; 
        ush  code; 
    } fc;
    union {
        ush  dad;  
        ush  len;  
    } dl;

} ct_data;

typedef struct tree_desc {
    ct_data *dyn_tree;    
    ct_data *static_tree; 
    int     *extra_bits;  
    int     extra_base;   
    int     elems;        
    int     max_length;   
    int     max_code;     
} tree_desc;

struct huft {
  uch e;                
  uch b;                
  union {
    ush n;              
    struct huft *t;     
  } v;
};

uch bl_order[BL_CODES]
   = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};

int extra_lbits[LENGTH_CODES]
   = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};

int extra_dbits[D_CODES]
   = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};

int extra_blbits[BL_CODES]
   = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,3,7};

ulg crc_32_tab[] = {
  0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
  0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
  0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
  0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
  0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
  0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
  0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
  0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
  0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
  0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
  0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
  0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
  0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
  0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
  0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
  0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
  0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
  0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
  0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
  0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
  0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
  0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
  0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
  0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
  0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
  0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
  0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
  0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
  0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
  0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
  0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
  0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
  0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
  0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
  0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
  0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
  0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
  0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
  0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
  0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
  0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
  0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
  0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
  0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
  0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
  0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
  0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
  0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
  0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
  0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
  0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
  0x2d02ef8dL
};

typedef struct _GZ1 {
 long     versionid1;
 int      state;
 int      done;
 int      deflate1_initialized;     
 unsigned deflate1_hash_head;       
 unsigned deflate1_prev_match;      
 int      deflate1_flush;           
 int      deflate1_match_available; 
 unsigned deflate1_match_length;    

 char ifname[MAX_PATH_LEN]; 
 char ofname[MAX_PATH_LEN]; 

 struct stat istat;     
 gz1_file_t  zfile;
 
 int   input_ismem;     
 char *input_ptr;       
 long  input_bytesleft; 
 
 int   output_ismem;    
 char *output_ptr;      
 uns   output_maxlen;   

 int  compr_level;      
 long time_stamp;       
 long ifile_size;       
 int  ifd;              
 int  ofd;              
 int  part_nb;          
 int  last_member;      
 int  save_orig_name;   
 long header_bytes;     
 long bytes_in;         
 long bytes_out;        
 uns  insize;           
 uns  inptr;            
 uns  outcnt;           
 uns  ins_h;            
 long block_start;      
 uns  good_match;       
 uni  max_lazy_match;   
 uni  prev_length;      
 uns  max_chain_length; 
 uns  strstart;         
 uns  match_start;      
 int  eofile;           
 uns  lookahead;        
 ush *file_type;        
 int *file_method;      
 ulg  opt_len;          
 ulg  static_len;       
 ulg  compressed_len;   
 ulg  input_len;        
 uns  last_flags;       
 uch  flags;            
 uns  last_lit;         
 uns  last_dist;        
 uch  flag_bit;         
 int  heap_len;         
 int  heap_max;         
 ulg  bb;               
 uns  bk;               
 ush  bi_buf;           
 int  bi_valid;         
 uns  hufts;            
 int  decrypt;          
 int  ascii;            
 int  msg_done;         
 int  abortflag;        
 int  decompress;       
 int  do_lzw;           
 int  to_stdout;        
 int  force;            
 int  verbose;
 int  quiet;
 int  list;             
 int  test;             
 int  ext_header;       
 int  pkzip;            
 int  method;           
 int  level;            
 int  no_time;          
 int  no_name;          
 int  exit_code;        
 int  lbits;            
 int  dbits;            
 ulg  window_size;      
 ulg  crc;              

 uch  dist_code[512];
 uch  length_code[MAX_MATCH-MIN_MATCH+1];
 int  heap[2*L_CODES+1];
 uch  depth[2*L_CODES+1];
 int  base_length[LENGTH_CODES];
 int  base_dist[D_CODES];
 ush  bl_count[MAX_BITS+1];
 uch  flag_buf[(LIT_BUFSIZE/8)];

 #ifdef DYN_ALLOC
 uch *inbuf;
 uch *outbuf;
 ush *d_buf;
 uch *window;
 #else
 uch inbuf [INBUFSIZ +INBUF_EXTRA];
 uch outbuf[OUTBUFSIZ+OUTBUF_EXTRA];
 ush d_buf [DIST_BUFSIZE];
 uch window[2L*WSIZE];
 #endif

 #ifdef FULL_SEARCH
 #define nice_match MAX_MATCH
 #else
 int nice_match;
 #endif

 #ifdef CRYPT
 uch cc;
 #endif

 ct_data static_ltree[L_CODES+2];
 ct_data static_dtree[D_CODES];
 ct_data dyn_dtree[(2*D_CODES+1)];
 ct_data dyn_ltree[HEAP_SIZE];
 ct_data bl_tree[2*BL_CODES+1];

 tree_desc l_desc;
 tree_desc d_desc;
 tree_desc bl_desc;

 #ifndef MAXSEG_64K

 ush prev2[1L<<BITS];

 #define prev gz1->prev2
 #define head (gz1->prev2+WSIZE)

 #else

 ush * prev2;
 ush * tab_prefix1;

 #define prev gz1->prev2
 #define head gz1->tab_prefix1

 #endif

} GZ1;
typedef GZ1 *PGZ1;
int gz1_size = sizeof( GZ1 );

/* Declare some local function protypes... */

/* Any routine name that can/might conflict with */
/* other modules or object code should simply have */
/* the standard prefix 'gz1_' added to the front. */
/* This will only usually be any kind of problem at all */
/* if the code is being compiled directly into the parent */
/* instead of being built as a standalone DLL or DSO library. */

PGZ1  gz1_init        ( void     );
int   gz1_cleanup     ( PGZ1 gz1 );
ulg   gz1_deflate     ( PGZ1 gz1 );
ulg   gz1_deflate_fast( PGZ1 gz1 );

/* The rest of the routines should not need the 'gz1_' prefix. */
/* No conflicts reported at this time. */

int   inflate        ( PGZ1 gz1 );
int   inflate_dynamic( PGZ1 gz1 );
int   inflate_stored ( PGZ1 gz1 );
int   inflate_fixed  ( PGZ1 gz1 );
void  fill_window    ( PGZ1 gz1 );
void  flush_outbuf   ( PGZ1 gz1 );
void  flush_window   ( PGZ1 gz1 );
void  bi_windup      ( PGZ1 gz1 );
void  set_file_type  ( PGZ1 gz1 );
void  init_block     ( PGZ1 gz1 );
int   build_bl_tree  ( PGZ1 gz1 );
void  read_error     ( PGZ1 gz1 );
void  write_error    ( PGZ1 gz1 );
int   get_header     ( PGZ1 gz1, int in );
int   inflate_block  ( PGZ1 gz1, int *e );
int   fill_inbuf     ( PGZ1 gz1, int eof_ok );
char *gz1_basename   ( PGZ1 gz1, char *fname );
int   longest_match  ( PGZ1 gz1, unsigned cur_match );
void  bi_init        ( PGZ1 gz1, gz1_file_t zipfile );
int   file_read      ( PGZ1 gz1, char *buf, unsigned size );
void  write_buf      ( PGZ1 gz1, int fd, voidp buf, unsigned cnt );

void  error( char *msg   );

int zip(
PGZ1 gz1, 
int  in,  
int  out  
);

ulg flush_block(
PGZ1  gz1,        
char *buf,        
ulg   stored_len, 
int   eof         
);

void copy_block(
PGZ1      gz1,    
char     *buf,    
unsigned  len,    
int       header  
);

int ct_tally(
PGZ1 gz1,  
int  dist, 
int  lc    
);

void send_bits(
PGZ1 gz1,   
int  value, 
int  length 
);

void send_tree(
PGZ1      gz1,     
ct_data  *tree,    
int       max_code 
);

void send_all_trees(
PGZ1 gz1,    
int  lcodes, 
int  dcodes, 
int  blcodes 
);

void ct_init(
PGZ1  gz1,    
ush  *attr,   
int  *methodp 
);

void lm_init(
PGZ1 gz1,        
int  pack_level, 
ush *flags       
);

void build_tree(
PGZ1        gz1, 
tree_desc  *desc 
);

void compress_block(
PGZ1      gz1,   
ct_data  *ltree, 
ct_data  *dtree  
);

void gen_bitlen(
PGZ1        gz1, 
tree_desc  *desc 
);

void pqdownheap(
PGZ1      gz1,  
ct_data  *tree, 
int       k     
);

int huft_build(
PGZ1          gz1, 
unsigned     *b,   
unsigned      n,   
unsigned      s,   
ush          *d,   
ush          *e,   
struct huft **t,   
int          *m    
);

ulg updcrc(
PGZ1      gz1, 
uch      *s,   
unsigned  n    
);

int inflate_codes(
PGZ1         gz1,  
struct huft *tl,   
struct huft *td,   
int          bl,   
int          bd    
);

void gen_codes(
PGZ1      gz1,     
ct_data  *tree,    
int       max_code 
);

void scan_tree(
PGZ1     gz1,     
ct_data *tree,    
int      max_code 
);

unsigned bi_reverse(
PGZ1     gz1,  
unsigned code, 
int      len   
);

int huft_free(
PGZ1         gz1, 
struct huft *t    
);

PGZ1 gz1_init()
{
 PGZ1 gz1=0; 

 gz1 = (PGZ1) malloc( gz1_size );

 if ( !gz1 ) 
   {
    return 0; 
   }

 memset( gz1, 0, gz1_size );

 ALLOC(uch, gz1->inbuf,  INBUFSIZ +INBUF_EXTRA);

 if ( !gz1->inbuf ) 
   {
    free( gz1 ); 
    return 0;    
   }

 ALLOC(uch, gz1->outbuf, OUTBUFSIZ+OUTBUF_EXTRA);
 
 if ( !gz1->outbuf ) 
   {
    FREE( gz1->inbuf  ); 
    free( gz1         ); 
    return 0;            
   }

 ALLOC(ush, gz1->d_buf,  DIST_BUFSIZE);

 if ( !gz1->d_buf ) 
   {
    FREE( gz1->outbuf ); 
    FREE( gz1->inbuf  ); 
    free( gz1         ); 
    return 0;            
   }

 ALLOC(uch, gz1->window, 2L*WSIZE);

 if ( !gz1->window ) 
   {
    FREE( gz1->d_buf  ); 
    FREE( gz1->outbuf ); 
    FREE( gz1->inbuf  ); 
    free( gz1         ); 
    return 0;            
   }

 #ifndef MAXSEG_64K
 
 #else 
 
 ALLOC(ush, gz1->prev2, 1L<<(BITS-1) );

 if ( !gz1->prev2 ) 
   {
    FREE( gz1->window ); 
    FREE( gz1->d_buf  ); 
    FREE( gz1->outbuf ); 
    FREE( gz1->inbuf  ); 
    free( gz1         ); 
    return 0;            
   }

 ALLOC(ush, gz1->tab_prefix1, 1L<<(BITS-1) );

 if ( !gz1->tab_prefix1 ) 
   {
    FREE( gz1->prev2  ); 
    FREE( gz1->window ); 
    FREE( gz1->d_buf  ); 
    FREE( gz1->outbuf ); 
    FREE( gz1->inbuf  ); 
    free( gz1         ); 
    return 0;            
   }

 #endif 

 gz1->method      = DEFLATED;     
 gz1->level       = 6;            
 gz1->no_time     = -1;           
 gz1->no_name     = -1;           
 gz1->exit_code   = OK;           
 gz1->lbits       = 9;            
 gz1->dbits       = 6;            

 gz1->window_size = (ulg)2*WSIZE;     
 gz1->crc         = (ulg)0xffffffffL; 

 gz1->d_desc.dyn_tree     = (ct_data *) gz1->dyn_dtree;
 gz1->d_desc.static_tree  = (ct_data *) gz1->static_dtree;
 gz1->d_desc.extra_bits   = (int     *) extra_dbits; 
 gz1->d_desc.extra_base   = (int      ) 0;
 gz1->d_desc.elems        = (int      ) D_CODES;
 gz1->d_desc.max_length   = (int      ) MAX_BITS;
 gz1->d_desc.max_code     = (int      ) 0;

 gz1->l_desc.dyn_tree     = (ct_data *) gz1->dyn_ltree;
 gz1->l_desc.static_tree  = (ct_data *) gz1->static_ltree;
 gz1->l_desc.extra_bits   = (int     *) extra_lbits; 
 gz1->l_desc.extra_base   = (int      ) LITERALS+1;
 gz1->l_desc.elems        = (int      ) L_CODES;
 gz1->l_desc.max_length   = (int      ) MAX_BITS;
 gz1->l_desc.max_code     = (int      ) 0;

 gz1->bl_desc.dyn_tree    = (ct_data *) gz1->bl_tree;
 gz1->bl_desc.static_tree = (ct_data *) 0;
 gz1->bl_desc.extra_bits  = (int     *) extra_blbits; 
 gz1->bl_desc.extra_base  = (int      ) 0;
 gz1->bl_desc.elems       = (int      ) BL_CODES;
 gz1->bl_desc.max_length  = (int      ) MAX_BL_BITS;
 gz1->bl_desc.max_code    = (int      ) 0;

 return (PGZ1) gz1;

}

int gz1_cleanup( PGZ1 gz1 )
{
 
 #ifndef MAXSEG_64K
 
 #else
 
 FREE( gz1->tab_prefix1 );
 FREE( gz1->prev2       );
 #endif 

 FREE( gz1->window ); 
 FREE( gz1->d_buf  ); 
 FREE( gz1->outbuf ); 
 FREE( gz1->inbuf  ); 

 free( gz1 ); 
 gz1 = 0;     

 return 0;
}

int (*read_buf)(PGZ1 gz1, char *buf, unsigned size);

void error( char *msg )
{
 msg = msg;
}

int (*work)( PGZ1 gz1, int infile, int outfile ) = 0; 

#ifdef __BORLANDC__
#pragma argsused
#endif

int get_header( PGZ1 gz1, int in )
{
 uch       flags;    
 char      magic[2]; 
 ulg       stamp;    
 unsigned  len;      
 unsigned  part;     

 if ( gz1->force && gz1->to_stdout )
   {
    magic[0] = (char)try_byte();
    magic[1] = (char)try_byte();
   }
 else
   {
    magic[0] = (char)get_byte();
    magic[1] = (char)get_byte();
   }

 gz1->method       = -1;        
 gz1->header_bytes = 0;         
 gz1->last_member  = RECORD_IO; 
 gz1->part_nb++;                

 if ( memcmp(magic, GZIP_MAGIC,     2 ) == 0 ||
      memcmp(magic, OLD_GZIP_MAGIC, 2 ) == 0 )
   {
    gz1->method = (int)get_byte();

    if ( gz1->method != DEFLATED )
      {
       gz1->exit_code = LZ1_ERROR;

       return -1;
      }

    return -1;

    if ((flags & ENCRYPTED) != 0)
      {
       gz1->exit_code = LZ1_ERROR;
       return -1;
      }

    if ((flags & CONTINUATION) != 0)
      {
       gz1->exit_code = LZ1_ERROR;
       if ( gz1->force <= 1) return -1;
      }

    if ((flags & RESERVED) != 0)
      {
       gz1->exit_code = LZ1_ERROR;
       if ( gz1->force <= 1)
          return -1;
      }

    stamp  = (ulg)get_byte();
	stamp |= ((ulg)get_byte()) << 8;
	stamp |= ((ulg)get_byte()) << 16;
	stamp |= ((ulg)get_byte()) << 24;

    if ( stamp != 0 && !gz1->no_time )
      {
       gz1->time_stamp = stamp;
      }

    (void)get_byte(); 
    (void)get_byte(); 

    if ((flags & CONTINUATION) != 0)
      {
       part  = (unsigned)  get_byte();
       part |= ((unsigned) get_byte())<<8;
      }

    if ((flags & EXTRA_FIELD) != 0)
      {
        len  = (unsigned)  get_byte();
        len |= ((unsigned) get_byte())<<8;

        while (len--) (void)get_byte();
      }

    if ((flags & COMMENT) != 0)
      {
       while (get_char() != 0)  ;
      }

    if ( gz1->part_nb == 1 )
      {
       gz1->header_bytes = gz1->inptr + 2*sizeof(long);
      }
   }

 return gz1->method;
}

int fill_inbuf( PGZ1 gz1, int eof_ok )
{
 int len;
 int bytes_to_copy;

 gz1->insize = 0;
 errno       = 0;

 do {
     if ( gz1->input_ismem )
       {
        if ( gz1->input_bytesleft > 0 )
          {
           bytes_to_copy = INBUFSIZ - gz1->insize;

           if ( bytes_to_copy > gz1->input_bytesleft )
             {
              bytes_to_copy = gz1->input_bytesleft;
             }

           memcpy(
           (char*)gz1->inbuf+gz1->insize,
           gz1->input_ptr,
           bytes_to_copy
           );

           gz1->input_ptr       += bytes_to_copy;
           gz1->input_bytesleft -= bytes_to_copy;

           len = bytes_to_copy;
          }
        else 
          {
           len = 0; 
          }
       }
     else 
       {
        len =
        read(
        gz1->ifd,
        (char*)gz1->inbuf+gz1->insize,
        INBUFSIZ-gz1->insize
        );
       }

     if (len == 0 || len == EOF) break;

     gz1->insize += len;

    } while( gz1->insize < INBUFSIZ );

 if ( gz1->insize == 0 )
   {
    if( eof_ok ) return EOF;
    read_error( gz1 );
   }

 gz1->bytes_in += (ulg) gz1->insize;
 gz1->inptr     = 1;

 return gz1->inbuf[0];
}

ulg updcrc(
PGZ1      gz1, 
uch      *s,   
unsigned  n    
)
{
 register ulg c; 

 if ( s == NULL )
   {
    c = 0xffffffffL;
   }
 else
   {
    c = gz1->crc;

    if ( n )
      {
       do{
          c = crc_32_tab[((int)c ^ (*s++)) & 0xff] ^ (c >> 8);

         } while( --n );
      }
   }

 gz1->crc = c;

 return( c ^ 0xffffffffL ); 
}

void read_error( PGZ1 gz1 )
{
 gz1->abortflag = 1;
}

void mod_gzip_strlwr( char *s )
{
 char *p1=s;

 if ( s == 0 ) return;

 while ( *p1 != 0 )
   {
    if ( *p1 > 96 ) *p1 = *p1 - 32;
    p1++;
   }
}

#ifdef __BORLANDC__
#pragma argsused
#endif

char *gz1_basename( PGZ1 gz1, char *fname )
{
 char *p;
 if ((p = strrchr(fname, PATH_SEP))  != NULL) fname = p+1;
 #ifdef PATH_SEP2
 if ((p = strrchr(fname, PATH_SEP2)) != NULL) fname = p+1;
 #endif
 #ifdef PATH_SEP3
 if ((p = strrchr(fname, PATH_SEP3)) != NULL) fname = p+1;
 #endif
 #ifdef SUFFIX_SEP
 if ((p = strrchr(fname, SUFFIX_SEP)) != NULL) *p = '\0';
 #endif
 if (casemap('A') == 'a') mod_gzip_strlwr(fname);
 return fname;
}

void write_buf( PGZ1 gz1, int fd, voidp buf, unsigned cnt )
{
 unsigned n;

 if ( gz1->output_ismem )
   {
    if ( ( gz1->bytes_out + cnt ) < gz1->output_maxlen )
      {
       memcpy( gz1->output_ptr, buf, cnt );
       gz1->output_ptr += cnt;
      }
    else
      {
       write_error( gz1 );
      }
   }
 else
   {
    while ((n = write(fd, buf, cnt)) != cnt)
      {
       if (n == (unsigned)(-1))
         {
          write_error( gz1 );
         }
       cnt -= n;
       buf = (voidp)((char*)buf+n);
      }
   }
}

void write_error( PGZ1 gz1 )
{
 gz1->abortflag = 1;
}

#ifdef __TURBOC__
#ifndef BC55

static ush ptr_offset = 0;

void * fcalloc(
unsigned items, 
unsigned size   
)
{
 void * buf = farmalloc((ulg)items*size + 16L);

 if (buf == NULL) return NULL;

 if (ptr_offset == 0)
   {
    ptr_offset = (ush)((uch*)buf-0);
   }
 else if (ptr_offset != (ush)((uch*)buf-0))
   {
    error("inconsistent ptr_offset");
   }

 *((ush*)&buf+1) += (ptr_offset + 15) >> 4;
 *(ush*)&buf = 0;

 return buf;
}

void fcfree( void *ptr )
{
 *((ush*)&ptr+1) -= (ptr_offset + 15) >> 4;
 *(ush*)&ptr = ptr_offset;

 farfree(ptr);
}

#endif 
#endif 

int zip(
PGZ1 gz1, 
int in,   
int out   
)
{
 uch  flags = 0;         
 ush  attr  = 0;         
 ush  deflate_flags = 0; 

 gz1->ifd    = in;
 gz1->ofd    = out;
 gz1->outcnt = 0;

 gz1->method = DEFLATED;

 put_byte(GZIP_MAGIC[0]); 
 put_byte(GZIP_MAGIC[1]);
 put_byte(DEFLATED);      

 if ( gz1->save_orig_name )
   {
	flags |= ORIG_NAME;
   }

 put_byte(flags);           
 put_long(gz1->time_stamp); 

 gz1->crc = -1; 

 updcrc( gz1, NULL, 0 ); 

 bi_init( gz1, out );
 ct_init( gz1, &attr, &gz1->method );
 lm_init( gz1, gz1->level, &deflate_flags );

 put_byte((uch)deflate_flags); 

 put_byte(OS_CODE); 

 if ( gz1->save_orig_name )
   {
    char *p = gz1_basename( gz1, gz1->ifname );

    do {
	    put_char(*p);

       } while (*p++);
   }

 gz1->header_bytes = (long)gz1->outcnt;

 (void) gz1_deflate( gz1 );

 put_long( gz1->crc      );
 put_long( gz1->bytes_in );

 gz1->header_bytes += 2*sizeof(long);

 flush_outbuf( gz1 );

 return OK;
}

ulg gz1_deflate( PGZ1 gz1 )
{
    unsigned hash_head;      
    unsigned prev_match;     
    int flush;               
    int match_available = 0; 
    register unsigned match_length = MIN_MATCH-1; 
#ifdef DEBUG
    long isize;        
#endif

    if (gz1->compr_level <= 3)
      {
       return gz1_deflate_fast(gz1);
      }

    while (gz1->lookahead != 0)
      {
       gz1->ins_h =
       (((gz1->ins_h)<<H_SHIFT) ^ (gz1->window[gz1->strstart+MIN_MATCH-1])) & HASH_MASK;

       prev[ gz1->strstart & WMASK ] = hash_head = head[ gz1->ins_h ];

       head[ gz1->ins_h ] = gz1->strstart;

        gz1->prev_length = match_length, prev_match = gz1->match_start;
        match_length = MIN_MATCH-1;

        if (hash_head != NIL && gz1->prev_length < gz1->max_lazy_match &&
            gz1->strstart - hash_head <= MAX_DIST) {
            
            match_length = longest_match( gz1, hash_head );
            
            if (match_length > gz1->lookahead) match_length = gz1->lookahead;

            if (match_length == MIN_MATCH && gz1->strstart-gz1->match_start > TOO_FAR){
                
                match_length--;
            }
        }
        
        if (gz1->prev_length >= MIN_MATCH && match_length <= gz1->prev_length) {

            flush = ct_tally(gz1,gz1->strstart-1-prev_match, gz1->prev_length - MIN_MATCH);

            gz1->lookahead        -= ( gz1->prev_length - 1 );
            gz1->prev_length -= 2;

            do {
                gz1->strstart++;

                gz1->ins_h =
                (((gz1->ins_h)<<H_SHIFT) ^ (gz1->window[ gz1->strstart + MIN_MATCH-1])) & HASH_MASK;

                prev[ gz1->strstart & WMASK ] = hash_head = head[gz1->ins_h];

                head[ gz1->ins_h ] = gz1->strstart;

            } while (--gz1->prev_length != 0);
            match_available = 0;
            match_length = MIN_MATCH-1;
            gz1->strstart++;
            if (flush) FLUSH_BLOCK(0), gz1->block_start = gz1->strstart;

        } else if (match_available) {
            
            if (ct_tally( gz1, 0, gz1->window[gz1->strstart-1] )) {
                FLUSH_BLOCK(0), gz1->block_start = gz1->strstart;
            }
            gz1->strstart++;
            gz1->lookahead--;
        } else {
            
            match_available = 1;
            gz1->strstart++;
            gz1->lookahead--;
        }
        
        while (gz1->lookahead < MIN_LOOKAHEAD && !gz1->eofile) fill_window(gz1);
    }
    if (match_available) ct_tally( gz1, 0, gz1->window[gz1->strstart-1] );

    return FLUSH_BLOCK(1); 

 return 0;
}

void flush_outbuf( PGZ1 gz1 )
{
 if ( gz1->outcnt == 0 )
   {
    return;
   }

 write_buf( gz1, gz1->ofd, (char *)gz1->outbuf, gz1->outcnt );

 gz1->bytes_out += (ulg)gz1->outcnt;
 gz1->outcnt = 0;
}

void lm_init(
PGZ1 gz1,        
int  pack_level, 
ush *flags       
)
{
 register unsigned j;

 if ( pack_level < 1 || pack_level > 9 )
   {
    error("bad pack level");
   }

 gz1->compr_level = pack_level;

 #if defined(MAXSEG_64K) && HASH_BITS == 15
 for (j = 0;  j < HASH_SIZE; j++) head[j] = NIL;
 #else
 memset( (char*)head, 0, (HASH_SIZE*sizeof(*head)) );
 #endif

 gz1->max_lazy_match   = configuration_table[pack_level].max_lazy;
 gz1->good_match       = configuration_table[pack_level].good_length;
 #ifndef FULL_SEARCH
 gz1->nice_match       = configuration_table[pack_level].nice_length;
 #endif
 gz1->max_chain_length = configuration_table[pack_level].max_chain;

 if ( pack_level == 1 )
   {
    *flags |= FAST;
   }
 else if ( pack_level == 9 )
   {
    *flags |= SLOW;
   }

 gz1->strstart    = 0;
 gz1->block_start = 0L;
 #ifdef ASMV
 match_init(); 
 #endif

 gz1->lookahead = read_buf(gz1,(char*)gz1->window,
                  sizeof(int) <= 2 ? (unsigned)WSIZE : 2*WSIZE);

 if (gz1->lookahead == 0 || gz1->lookahead == (unsigned)EOF)
   {
    gz1->eofile = 1, gz1->lookahead = 0;
    return;
   }

 gz1->eofile = 0;

 while (gz1->lookahead < MIN_LOOKAHEAD && !gz1->eofile)
   {
    fill_window(gz1);
   }

 gz1->ins_h = 0;

 for ( j=0; j<MIN_MATCH-1; j++ )
    {
     gz1->ins_h =
     (((gz1->ins_h)<<H_SHIFT) ^ (gz1->window[j])) & HASH_MASK;
    }
}

void fill_window( PGZ1 gz1 )
{
 register unsigned n, m;

 unsigned more =
 (unsigned)( gz1->window_size - (ulg)gz1->lookahead - (ulg)gz1->strstart );

 if ( more == (unsigned)EOF)
   {
    more--;
   }
 else if ( gz1->strstart >= WSIZE+MAX_DIST )
   {
    memcpy((char*)gz1->window, (char*)gz1->window+WSIZE, (unsigned)WSIZE);

    gz1->match_start -= WSIZE;
    gz1->strstart    -= WSIZE; 

    gz1->block_start -= (long) WSIZE;

    for ( n = 0; n < HASH_SIZE; n++ )
       {
        m = head[n];
        head[n] = (ush)(m >= WSIZE ? m-WSIZE : NIL);
       }

    for ( n = 0; n < WSIZE; n++ )
       {
        m = prev[n];

        prev[n] = (ush)(m >= WSIZE ? m-WSIZE : NIL);
       }

    more += WSIZE;
   }

 if ( !gz1->eofile )
   {
    n = read_buf(gz1,(char*)gz1->window+gz1->strstart+gz1->lookahead, more);

    if ( n == 0 || n == (unsigned)EOF )
      {
       gz1->eofile = 1;
      }
    else
      {
       gz1->lookahead += n;
      }
   }
}

ulg gz1_deflate_fast( PGZ1 gz1 )
{
    unsigned hash_head; 
    int flush;      
    unsigned match_length = 0;  

    gz1->prev_length = MIN_MATCH-1;

    while (gz1->lookahead != 0)
      {
       gz1->ins_h =
       (((gz1->ins_h)<<H_SHIFT) ^ (gz1->window[ gz1->strstart + MIN_MATCH-1])) & HASH_MASK;
       
       prev[ gz1->strstart & WMASK ] = hash_head = head[ gz1->ins_h ];

       head[ gz1->ins_h ] = gz1->strstart;

        if (hash_head != NIL && gz1->strstart - hash_head <= MAX_DIST) {
            
            match_length = longest_match( gz1, hash_head );
            
            if (match_length > gz1->lookahead) match_length = gz1->lookahead;
        }
        if (match_length >= MIN_MATCH) {

            flush = ct_tally(gz1,gz1->strstart-gz1->match_start, match_length - MIN_MATCH);

            gz1->lookahead -= match_length;

            if (match_length <= gz1->max_lazy_match )
              {
                match_length--; 

                do {
                    gz1->strstart++;

                    gz1->ins_h =
                    (((gz1->ins_h)<<H_SHIFT) ^ (gz1->window[ gz1->strstart + MIN_MATCH-1])) & HASH_MASK;
                    
                    prev[ gz1->strstart & WMASK ] = hash_head = head[ gz1->ins_h ];

                    head[ gz1->ins_h ] = gz1->strstart;

                } while (--match_length != 0);
            gz1->strstart++;
            } else {
            gz1->strstart += match_length;
	        match_length = 0;
            gz1->ins_h = gz1->window[gz1->strstart];

            gz1->ins_h =
            (((gz1->ins_h)<<H_SHIFT) ^ (gz1->window[gz1->strstart+1])) & HASH_MASK;
            
#if MIN_MATCH != 3
                Call UPDATE_HASH() MIN_MATCH-3 more times
#endif
            }
        } else {
            
            flush = ct_tally(gz1, 0, gz1->window[gz1->strstart]);
            gz1->lookahead--;
        gz1->strstart++;
        }
        if (flush) FLUSH_BLOCK(0), gz1->block_start = gz1->strstart;

        while (gz1->lookahead < MIN_LOOKAHEAD && !gz1->eofile) fill_window(gz1);
    }

 return FLUSH_BLOCK(1);
}

void ct_init(
PGZ1  gz1,    
ush  *attr,   
int  *methodp 
)
{
 #ifdef DD1
 int i,ii;
 #endif

 int n;      
 int bits;   
 int length; 
 int code;   
 int dist;   

 gz1->file_type      = attr;
 gz1->file_method    = methodp;
 gz1->compressed_len = gz1->input_len = 0L;
        
 if ( gz1->static_dtree[0].dl.len != 0 )
   {
    return;
   }

 length = 0;

 for ( code = 0; code < LENGTH_CODES-1; code++ )
    {
     gz1->base_length[code] = length;

     for ( n = 0; n < (1<<extra_lbits[code]); n++ )
        {
         gz1->length_code[length++] = (uch)code;
        }
    }

 gz1->length_code[length-1] = (uch)code;

 dist = 0;

 for ( code = 0 ; code < 16; code++ )
    {
     gz1->base_dist[code] = dist;

     for ( n = 0; n < (1<<extra_dbits[code]); n++ )
        {
         gz1->dist_code[dist++] = (uch)code;
        }
    }

 dist >>= 7; 

 for ( ; code < D_CODES; code++ )
    {
     gz1->base_dist[code] = dist << 7;

     for ( n = 0; n < (1<<(extra_dbits[code]-7)); n++ )
        {
         gz1->dist_code[256 + dist++] = (uch)code;
        }
    }

 for ( bits = 0; bits <= MAX_BITS; bits++ )
    {
     gz1->bl_count[bits] = 0;
    }

 n = 0;

 while (n <= 143) gz1->static_ltree[n++].dl.len = 8, gz1->bl_count[8]++;
 while (n <= 255) gz1->static_ltree[n++].dl.len = 9, gz1->bl_count[9]++;
 while (n <= 279) gz1->static_ltree[n++].dl.len = 7, gz1->bl_count[7]++;
 while (n <= 287) gz1->static_ltree[n++].dl.len = 8, gz1->bl_count[8]++;

 gen_codes(gz1,(ct_data *)gz1->static_ltree, L_CODES+1);

 for ( n = 0; n < D_CODES; n++ )
    {
     gz1->static_dtree[n].dl.len  = 5;
     gz1->static_dtree[n].fc.code = bi_reverse( gz1, n, 5 );
    }

 init_block( gz1 );
}

ulg flush_block(
PGZ1  gz1,        
char *buf,        
ulg   stored_len, 
int   eof         
)
{
 ulg opt_lenb;     
 ulg static_lenb;  
 int max_blindex;  

 gz1->flag_buf[gz1->last_flags] = gz1->flags;

 if (*gz1->file_type == (ush)UNKNOWN) set_file_type(gz1);

 build_tree( gz1, (tree_desc *)(&gz1->l_desc) );
 build_tree( gz1, (tree_desc *)(&gz1->d_desc) );

 max_blindex = build_bl_tree( gz1 );

 opt_lenb         = (gz1->opt_len+3+7)>>3;
 static_lenb      = (gz1->static_len+3+7)>>3;
 gz1->input_len  += stored_len; 

 if (static_lenb <= opt_lenb) opt_lenb = static_lenb;

#ifdef FORCE_METHOD
 
 if ( level == 1 && eof && gz1->compressed_len == 0L )
#else
 if (stored_len <= opt_lenb && eof && gz1->compressed_len == 0L && 0 )
#endif
   {
    if (buf == (char*)0) error ("block vanished");

    copy_block( gz1, buf, (unsigned)stored_len, 0 ); 

    gz1->compressed_len = stored_len << 3;
    *gz1->file_method   = STORED;

#ifdef FORCE_METHOD
 } else if (level == 2 && buf != (char*)0) { 
#else
 } else if (stored_len+4 <= opt_lenb && buf != (char*)0) {
                    
#endif
     
     send_bits(gz1,(STORED_BLOCK<<1)+eof, 3);  
     gz1->compressed_len = (gz1->compressed_len + 3 + 7) & ~7L;
     gz1->compressed_len += (stored_len + 4) << 3;

     copy_block(gz1, buf, (unsigned)stored_len, 1); 

#ifdef FORCE_METHOD
 } else if (level == 3) { 
#else
 } else if (static_lenb == opt_lenb) {
#endif
     send_bits(gz1,(STATIC_TREES<<1)+eof, 3);

     compress_block(
     gz1,
     (ct_data *)gz1->static_ltree,
     (ct_data *)gz1->static_dtree
     );

     gz1->compressed_len += 3 + gz1->static_len;
    }
  else
    {
     send_bits(gz1,(DYN_TREES<<1)+eof, 3);

     send_all_trees(
     gz1,
     gz1->l_desc.max_code+1,
     gz1->d_desc.max_code+1,
     max_blindex+1
     );

     compress_block(
     gz1,
     (ct_data *)gz1->dyn_ltree,
     (ct_data *)gz1->dyn_dtree
     );

     gz1->compressed_len += 3 + gz1->opt_len;
    }

 init_block( gz1 );

 if ( eof )
   {
    bi_windup( gz1 );

    gz1->compressed_len += 7;  
   }

 return gz1->compressed_len >> 3;
}

#ifdef __BORLANDC__
#pragma argsused
#endif

unsigned bi_reverse(
PGZ1     gz1,  
unsigned code, 
int      len   
)
{
 register unsigned res = 0;

 do {
     res |= code & 1;
     code >>= 1, res <<= 1;

    } while (--len > 0);

 return res >> 1;
}

void set_file_type( PGZ1 gz1 )
{
 int n = 0;
 unsigned ascii_freq = 0;
 unsigned bin_freq = 0;

 while (n < 7)        bin_freq += gz1->dyn_ltree[n++].fc.freq;
 while (n < 128)    ascii_freq += gz1->dyn_ltree[n++].fc.freq;
 while (n < LITERALS) bin_freq += gz1->dyn_ltree[n++].fc.freq;

 *gz1->file_type = bin_freq > (ascii_freq >> 2) ? BINARY : ASCII;
}

void init_block( PGZ1 gz1 )
{
 int n; 

 for (n = 0; n < L_CODES;  n++) gz1->dyn_ltree[n].fc.freq = 0;
 for (n = 0; n < D_CODES;  n++) gz1->dyn_dtree[n].fc.freq = 0;
 for (n = 0; n < BL_CODES; n++) gz1->bl_tree[n].fc.freq   = 0;

 gz1->dyn_ltree[END_BLOCK].fc.freq = 1;

 gz1->opt_len    = 0L;
 gz1->static_len = 0L;
 gz1->last_lit   = 0;
 gz1->last_dist  = 0;
 gz1->last_flags = 0;
 gz1->flags      = 0;
 gz1->flag_bit   = 1;
}

void bi_init( PGZ1 gz1, gz1_file_t zipfile )
{
 gz1->zfile    = zipfile;
 gz1->bi_buf   = 0;
 gz1->bi_valid = 0;

 if ( gz1->zfile != NO_FILE )
   {
    read_buf = file_read;
   }
}

int ct_tally(
PGZ1 gz1,  
int  dist, 
int  lc    
)
{
 int dcode;

 gz1->inbuf[gz1->last_lit++] = (uch)lc;

 if ( dist == 0 )
   {
    gz1->dyn_ltree[lc].fc.freq++; 
   }
 else
   {
    dist--; 

    gz1->dyn_ltree[gz1->length_code[lc]+LITERALS+1].fc.freq++;
    gz1->dyn_dtree[d_code(dist)].fc.freq++;
    gz1->d_buf[gz1->last_dist++] = (ush)dist;
    gz1->flags |= gz1->flag_bit;
   }

 gz1->flag_bit <<= 1;

 if ( (gz1->last_lit & 7) == 0 )
   {
    gz1->flag_buf[gz1->last_flags++] = gz1->flags;
    gz1->flags = 0, gz1->flag_bit = 1;
   }

 if ( gz1->level > 2 && (gz1->last_lit & 0xfff) == 0)
   {
    ulg out_length = (ulg) ( gz1->last_lit * 8L );
    ulg in_length  = (ulg) ( gz1->strstart - gz1->block_start );

    for ( dcode = 0; dcode < D_CODES; dcode++ )
       {
        out_length += (ulg) ((gz1->dyn_dtree[dcode].fc.freq)*(5L+extra_dbits[dcode]));
       }

    out_length >>= 3;

    if ( gz1->last_dist < gz1->last_lit/2 && out_length < in_length/2 )
      {
       return 1;
      }
   }

 return( gz1->last_lit == LIT_BUFSIZE-1 || gz1->last_dist == DIST_BUFSIZE );
}

void compress_block(
PGZ1     gz1,   
ct_data *ltree, 
ct_data *dtree  
)
{
 unsigned dist;   
 int lc;          
 unsigned lx = 0; 
 unsigned dx = 0; 
 unsigned fx = 0; 
 uch flag = 0;    
 unsigned code;   
 int extra;       

 if (gz1->last_lit != 0) do {
     if ((lx & 7) == 0) flag = gz1->flag_buf[fx++];
     lc = gz1->inbuf[lx++];
     if ((flag & 1) == 0) {
         send_code(lc, ltree); 
     } else {
         
         code = gz1->length_code[lc];
         send_code(code+LITERALS+1, ltree); 
         extra = extra_lbits[code];
         if (extra != 0) {
             lc -= gz1->base_length[code];
             send_bits(gz1,lc, extra); 
         }
         dist = gz1->d_buf[dx++];
         
         code = d_code(dist);

         send_code(code, dtree);       
         extra = extra_dbits[code];
         if (extra != 0) {
             dist -= gz1->base_dist[code];
             send_bits(gz1,dist, extra); 
         }
     } 
     flag >>= 1;
 } while (lx < gz1->last_lit);

 send_code(END_BLOCK, ltree);
}

#ifndef ASMV

int longest_match( PGZ1 gz1, unsigned cur_match )
{
 unsigned chain_length = gz1->max_chain_length;   
 register uch *scan = gz1->window + gz1->strstart;     
 register uch *match;                        
 register int len;                           
 int best_len = gz1->prev_length;                 
 unsigned limit = gz1->strstart > (unsigned)MAX_DIST ? gz1->strstart - (unsigned)MAX_DIST : NIL;
 
#if HASH_BITS < 8 || MAX_MATCH != 258
   error: Code too clever
#endif

#ifdef UNALIGNED_OK
    
    register uch *strend    = gz1->window + gz1->strstart + MAX_MATCH - 1;
    register ush scan_start = *(ush*)scan;
    register ush scan_end   = *(ush*)(scan+best_len-1);
#else
    register uch *strend    = gz1->window + gz1->strstart + MAX_MATCH;
    register uch scan_end1  = scan[best_len-1];
    register uch scan_end   = scan[best_len];
#endif

    if (gz1->prev_length >= gz1->good_match) {
        chain_length >>= 2;
    }

    do {
        match = gz1->window + cur_match;

#if (defined(UNALIGNED_OK) && MAX_MATCH == 258)
        
        if (*(ush*)(match+best_len-1) != scan_end ||
            *(ush*)match != scan_start) continue;

        scan++, match++;
        do {
        } while (*(ush*)(scan+=2) == *(ush*)(match+=2) &&
                 *(ush*)(scan+=2) == *(ush*)(match+=2) &&
                 *(ush*)(scan+=2) == *(ush*)(match+=2) &&
                 *(ush*)(scan+=2) == *(ush*)(match+=2) &&
                 scan < strend);
        
        if (*scan == *match) scan++;

        len = (MAX_MATCH - 1) - (int)(strend-scan);
        scan = strend - (MAX_MATCH-1);
#else 
        if (match[best_len]   != scan_end  ||
            match[best_len-1] != scan_end1 ||
            *match            != *scan     ||
            *++match          != scan[1])      continue;

        scan += 2, match++;

        do {
        } while (*++scan == *++match && *++scan == *++match &&
                 *++scan == *++match && *++scan == *++match &&
                 *++scan == *++match && *++scan == *++match &&
                 *++scan == *++match && *++scan == *++match &&
                 scan < strend);

        len = MAX_MATCH - (int)(strend - scan);
        scan = strend - MAX_MATCH;
#endif 
        if (len > best_len) {
            gz1->match_start = cur_match;
            best_len = len;
            if (len >= gz1->nice_match) break;
#ifdef UNALIGNED_OK
            scan_end = *(ush*)(scan+best_len-1);
#else
            scan_end1  = scan[best_len-1];
            scan_end   = scan[best_len];
#endif
        }
    } while ((cur_match = prev[cur_match & WMASK]) > limit
	     && --chain_length != 0);

    return best_len;
}
#endif 

void send_bits(
PGZ1 gz1,   
int  value, 
int  length 
)
{
 if ( gz1->bi_valid > (int) BUFSIZE - length )
   {
    gz1->bi_buf |= (value << gz1->bi_valid);

    put_short(gz1->bi_buf);

    gz1->bi_buf = (ush)value >> (BUFSIZE - gz1->bi_valid);
    gz1->bi_valid += length - BUFSIZE;
   }
 else
   {
    gz1->bi_buf |= value << gz1->bi_valid;
    gz1->bi_valid += length;
   }
}

void build_tree(
PGZ1       gz1, 
tree_desc *desc 
)
{
 int elems      = desc->elems;
 ct_data *tree  = desc->dyn_tree;
 ct_data *stree = desc->static_tree;

 int n;             
 int m;             
 int max_code = -1; 
 int node = elems;  
 int new1;          

    gz1->heap_len = 0, gz1->heap_max = HEAP_SIZE;

    for (n = 0; n < elems; n++) {
        if (tree[n].fc.freq != 0) {
            gz1->heap[++gz1->heap_len] = max_code = n;
            gz1->depth[n] = 0;
        } else {
            tree[n].dl.len = 0;
        }
    }

    while (gz1->heap_len < 2) {
        new1 = gz1->heap[++gz1->heap_len] = (max_code < 2 ? ++max_code : 0);
        tree[new1].fc.freq = 1;
        gz1->depth[new1] = 0;
        gz1->opt_len--; if (stree) gz1->static_len -= stree[new1].dl.len;
    }
    desc->max_code = max_code;

    for (n = gz1->heap_len/2; n >= 1; n--) pqdownheap(gz1, tree, n);

    do {
        n = gz1->heap[SMALLEST];
        gz1->heap[SMALLEST] = gz1->heap[gz1->heap_len--];
        pqdownheap(gz1, tree, SMALLEST);
        m = gz1->heap[SMALLEST];
        gz1->heap[--gz1->heap_max] = n;
        gz1->heap[--gz1->heap_max] = m;
        tree[node].fc.freq = tree[n].fc.freq + tree[m].fc.freq;
        gz1->depth[node] = (uch) (GZ1_MAX(gz1->depth[n], gz1->depth[m]) + 1);
        tree[n].dl.dad = tree[m].dl.dad = (ush)node;
        gz1->heap[SMALLEST] = node++;
        pqdownheap(gz1, tree, SMALLEST);

    } while (gz1->heap_len >= 2);

    gz1->heap[--gz1->heap_max] = gz1->heap[SMALLEST];

    gen_bitlen(gz1,(tree_desc *)desc);

    gen_codes(gz1,(ct_data *)tree, max_code);
}

int build_bl_tree( PGZ1 gz1 )
{
 int max_blindex; 

 scan_tree( gz1, (ct_data *)gz1->dyn_ltree, gz1->l_desc.max_code );
 scan_tree( gz1, (ct_data *)gz1->dyn_dtree, gz1->d_desc.max_code );

 build_tree( gz1, (tree_desc *)(&gz1->bl_desc) );

 for ( max_blindex = BL_CODES-1; max_blindex >= 3; max_blindex-- )
    {
     if (gz1->bl_tree[bl_order[max_blindex]].dl.len != 0) break;
    }

 gz1->opt_len += 3*(max_blindex+1) + 5+5+4;

 return max_blindex;
}

void gen_codes(
PGZ1     gz1,     
ct_data *tree,    
int      max_code 
)
{
 ush next_code[MAX_BITS+1]; 
 ush code = 0;              
 int bits;                  
 int n;                     

 for ( bits = 1; bits <= MAX_BITS; bits++ )
    {
     next_code[bits] = code = (code + gz1->bl_count[bits-1]) << 1;
    }

 for ( n = 0;  n <= max_code; n++ )
    {
     int len = tree[n].dl.len;
     if (len == 0) continue;

     tree[n].fc.code = bi_reverse( gz1, next_code[len]++, len );
    }

 return;
}

void gen_bitlen(
PGZ1       gz1, 
tree_desc *desc 
)
{
 ct_data *tree   = desc->dyn_tree;
 int *extra      = desc->extra_bits;
 int base             = desc->extra_base;
 int max_code         = desc->max_code;
 int max_length       = desc->max_length;
 ct_data *stree  = desc->static_tree;
 int h;              
 int n, m;           
 int bits;           
 int xbits;          
 ush f;              
 int overflow = 0;   

 for (bits = 0; bits <= MAX_BITS; bits++) gz1->bl_count[bits] = 0;

 tree[gz1->heap[gz1->heap_max]].dl.len = 0;

 for (h = gz1->heap_max+1; h < HEAP_SIZE; h++) {
     n = gz1->heap[h];
     bits = tree[tree[n].dl.dad].dl.len + 1;
     if (bits > max_length) bits = max_length, overflow++;
     tree[n].dl.len = (ush)bits;
     
     if (n > max_code) continue; 

     gz1->bl_count[bits]++;
     xbits = 0;
     if (n >= base) xbits = extra[n-base];
     f = tree[n].fc.freq;
     gz1->opt_len += (ulg)f * (bits + xbits);
     if (stree) gz1->static_len += (ulg)f * (stree[n].dl.len + xbits);
 }
 if (overflow == 0) return;

 do {
     bits = max_length-1;
     while (gz1->bl_count[bits] == 0) bits--;
     gz1->bl_count[bits]--;      
     gz1->bl_count[bits+1] += 2; 
     gz1->bl_count[max_length]--;
     
     overflow -= 2;
 } while (overflow > 0);

 for (bits = max_length; bits != 0; bits--) {
     n = gz1->bl_count[bits];
     while (n != 0) {
         m = gz1->heap[--h];
         if (m > max_code) continue;
         if (tree[m].dl.len != (unsigned) bits) {
             gz1->opt_len += ((long)bits-(long)tree[m].dl.len)*(long)tree[m].fc.freq;
             tree[m].dl.len = (ush)bits;
         }
         n--;
     }
  }
}

void copy_block(
PGZ1      gz1,    
char     *buf,    
unsigned  len,    
int       header  
)
{
 #ifdef CRYPT
 int t;
 #endif

 bi_windup( gz1 ); 

 if ( header )
   {
    put_short((ush)len);
    put_short((ush)~len);
   }

 while( len-- )
   {
    #ifdef CRYPT
	if (key) zencode(*buf, t);
    #endif

    put_byte(*buf++);
   }
}

int file_read( PGZ1 gz1, char *buf, unsigned size )
{
 unsigned len = 0;
 unsigned bytes_to_copy = 0;

 if ( gz1->input_ismem )
   {
    if ( gz1->input_bytesleft > 0 )
      {
       bytes_to_copy = size;

       if ( bytes_to_copy > (unsigned) gz1->input_bytesleft )
         {
          bytes_to_copy = (unsigned) gz1->input_bytesleft;
         }

       memcpy( buf, gz1->input_ptr, bytes_to_copy );

       gz1->input_ptr       += bytes_to_copy;
       gz1->input_bytesleft -= bytes_to_copy;

       len = bytes_to_copy;
      }
    else
      {
       len = 0;
      }
   }
 else
   {
    len = read( gz1->ifd, buf, size );
   }

 if ( len == (unsigned)(-1) || len == 0 )
   {
	gz1->crc = gz1->crc ^ 0xffffffffL;
    return (int)len;
   }

 updcrc( gz1, (uch*)buf, len );
 gz1->bytes_in += (ulg)len;

 return (int)len;
}

void bi_windup( PGZ1 gz1 )
{
 if ( gz1->bi_valid > 8 )
   {
    put_short(gz1->bi_buf);
   }
 else if ( gz1->bi_valid > 0 )
   {
    put_byte(gz1->bi_buf);
   }

 gz1->bi_buf   = 0;
 gz1->bi_valid = 0;
}

void send_all_trees(
PGZ1 gz1,    
int  lcodes, 
int  dcodes, 
int  blcodes 
)
{
 int rank; 

 send_bits(gz1,lcodes-257, 5); 
 send_bits(gz1,dcodes-1,   5);
 send_bits(gz1,blcodes-4,  4); 

 for ( rank = 0; rank < blcodes; rank++ )
    {
     send_bits(gz1,gz1->bl_tree[bl_order[rank]].dl.len, 3 );
    }

 send_tree(gz1,(ct_data *)gz1->dyn_ltree, lcodes-1); 
 send_tree(gz1,(ct_data *)gz1->dyn_dtree, dcodes-1); 
}

void send_tree(
PGZ1     gz1,     
ct_data *tree,    
int      max_code 
)
{
 int n;                        
 int prevlen = -1;             
 int curlen;                   
 int nextlen = tree[0].dl.len; 
 int count = 0;                
 int max_count = 7;            
 int min_count = 4;            

 if (nextlen == 0) max_count = 138, min_count = 3;

 for ( n = 0; n <= max_code; n++ )
    {
     curlen  = nextlen;
     nextlen = tree[n+1].dl.len;

     if (++count < max_count && curlen == nextlen)
       {
        continue;
       }
     else if (count < min_count)
       {
        do { send_code(curlen, gz1->bl_tree); } while (--count != 0);
       }
     else if (curlen != 0)
       {
        if ( curlen != prevlen )
          {
           send_code(curlen, gz1->bl_tree); count--;
          }

        send_code( REP_3_6, gz1->bl_tree ); send_bits(gz1,count-3, 2);
       }
     else if (count <= 10)
       {
        send_code(REPZ_3_10, gz1->bl_tree); send_bits(gz1,count-3, 3);
       }
     else
       {
        send_code(REPZ_11_138, gz1->bl_tree); send_bits(gz1,count-11, 7);
       }

     count   = 0;
     prevlen = curlen;

     if (nextlen == 0)
       {
        max_count = 138, min_count = 3;
       }
     else if (curlen == nextlen)
       {
        max_count = 6, min_count = 3;
       }
     else
       {
        max_count = 7, min_count = 4;
       }
    }
}

void scan_tree(
PGZ1     gz1,     
ct_data *tree,    
int      max_code 
)
{
 int n;                        
 int prevlen = -1;             
 int curlen;                   
 int nextlen = tree[0].dl.len; 
 int count = 0;                
 int max_count = 7;            
 int min_count = 4;            

 if (nextlen == 0) max_count = 138, min_count = 3;

 tree[max_code+1].dl.len = (ush)0xffff; 

 for ( n = 0; n <= max_code; n++ )
    {
     curlen  = nextlen;
     nextlen = tree[n+1].dl.len;

     if ( ++count < max_count && curlen == nextlen )
       {
        continue;
       }
     else if ( count < min_count )
       {
        gz1->bl_tree[curlen].fc.freq += count;
       }
     else if ( curlen != 0 )
       {
        if ( curlen != prevlen ) gz1->bl_tree[curlen].fc.freq++;
        gz1->bl_tree[REP_3_6].fc.freq++;
       }
     else if ( count <= 10 )
       {
        gz1->bl_tree[REPZ_3_10].fc.freq++;
       }
     else
       {
        gz1->bl_tree[REPZ_11_138].fc.freq++;
       }

     count   = 0;
     prevlen = curlen;

     if ( nextlen == 0 )
       {
        max_count = 138;
        min_count = 3;
       }
     else if (curlen == nextlen)
       {
        max_count = 6;
        min_count = 3;
       }
     else
       {
        max_count = 7;
        min_count = 4;
       }
    }
}

void pqdownheap(
PGZ1     gz1,  
ct_data *tree, 
int      k     
)
{
 int v = gz1->heap[k];
 int j = k << 1;  

 while( j <= gz1->heap_len )
   {
    if (j < gz1->heap_len && smaller(tree, gz1->heap[j+1], gz1->heap[j])) j++;

    if (smaller(tree, v, gz1->heap[j])) break;

    gz1->heap[k] = gz1->heap[j];  k = j;

    j <<= 1;
   }

 gz1->heap[k] = v;
}

#define GZS_ZIP1      1
#define GZS_ZIP2      2
#define GZS_DEFLATE1  3
#define GZS_DEFLATE2  4

int gzs_fsp     ( PGZ1 gz1 ); 
int gzs_zip1    ( PGZ1 gz1 ); 
int gzs_zip2    ( PGZ1 gz1 ); 
int gzs_deflate1( PGZ1 gz1 ); 
int gzs_deflate2( PGZ1 gz1 ); 

int gzp_main( request_rec *r, GZP_CONTROL *gzp )
{
 char cn[]="gzp_main()";

 PGZ1 gz1 = 0;
 int  rc  = 0;
 int  final_exit_code = 0;
 int  ofile_flags = O_RDWR | O_CREAT | O_TRUNC | O_BINARY;

 gzp->result_code = 0; 
 gzp->bytes_out   = 0; 

 gz1 = (PGZ1) gz1_init();

 if ( gz1 == 0 )
   {
    return 0;
   }

 gz1->decompress      = gzp->decompress;

 mod_gzip_strcpy( gz1->ifname, gzp->input_filename  );
 mod_gzip_strcpy( gz1->ofname, gzp->output_filename );

 gz1->input_ismem     = gzp->input_ismem;
 gz1->input_ptr       = gzp->input_ismem_ibuf + gzp->input_offset;
 gz1->input_bytesleft = gzp->input_ismem_ibuflen;

 gz1->output_ismem    = gzp->output_ismem;
 gz1->output_ptr      = gzp->output_ismem_obuf;
 gz1->output_maxlen   = gzp->output_ismem_obuflen;

 if ( gz1->no_time < 0 ) gz1->no_time = gz1->decompress;
 if ( gz1->no_name < 0 ) gz1->no_name = gz1->decompress;

 work = zip; 

 if ( !gz1->input_ismem )
   {
    errno = 0;

    rc = stat( gz1->ifname, &gz1->istat );

    if ( rc != 0 ) 
      {
       ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, r->server,
       "%s: stat(gz1->ifname=%s) FAILED", cn, gz1->ifname );

       gz1_cleanup( gz1 );

       return 0; 
      }

    gz1->ifile_size = ( gz1->istat.st_size - gzp->input_offset );

    if ( gz1->ifile_size < 0 ) gz1->ifile_size = 0;

    gz1->ifd =
    OPEN(
    gz1->ifname,
    gz1->ascii && !gz1->decompress ? O_RDONLY : O_RDONLY | O_BINARY,
    RW_USER
    );

    if ( gz1->ifd == -1 )
      {
       ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, r->server,
       "%s: OPEN(gz1->ifname=%s) FAILED", cn, gz1->ifname );

       gz1_cleanup( gz1 );

       return 0; 
      }

    if ( gzp->input_offset > 0 )
      {
       SEEKFORWARD( gz1->ifd, gzp->input_offset );
      }
   }

 if ( !gz1->output_ismem ) 
   {
    if ( gz1->ascii && gz1->decompress )
      {
       ofile_flags &= ~O_BINARY; 
      }

    gz1->ofd = OPEN( gz1->ofname, ofile_flags, RW_USER );

    if ( gz1->ofd == -1 )
      {
       ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, r->server,
       "%s: OPEN(gz1->ofname=%s) FAILED", cn, gz1->ofname );

       if ( gz1->ifd )
         {
          close( gz1->ifd ); 
          gz1->ifd = 0;      
         }

       gz1_cleanup( gz1 ); 

       return 0; 
      }
   }

 gz1->outcnt    = 0;
 gz1->insize    = 0;
 gz1->inptr     = 0;
 gz1->bytes_in  = 0L;
 gz1->bytes_out = 0L; 
 gz1->part_nb   = 0;

 if ( gz1->decompress )
   {
    gz1->method = get_header( gz1, gz1->ifd );

    if ( gz1->method < 0 )
      {
       if ( gz1->ifd ) 
         {
          close( gz1->ifd ); 
          gz1->ifd = 0;      
         }

       if ( gz1->ofd ) 
         {
          close( gz1->ofd ); 
          gz1->ofd = 0;      
         }

       return 0; 
      }
   }

 gz1->save_orig_name = 0;

 gz1->state = GZS_ZIP1;

 for (;;) 
    {
     gzs_fsp( gz1 ); 

     if ( gz1->done == 1 ) break; 
    }

 if ( gz1->ifd ) 
   {
    close( gz1->ifd ); 
    gz1->ifd = 0;      
   }

 if ( gz1->ofd ) 
   {
    close( gz1->ofd ); 
    gz1->ofd = 0;      
   }

 gzp->result_code = gz1->exit_code;
 gzp->bytes_out   = gz1->bytes_out;

 final_exit_code = (int) gz1->exit_code;

 gz1_cleanup( gz1 );  

 return final_exit_code; 
}

int gzs_fsp( PGZ1 gz1 )
{
 int rc=0; 

 switch( gz1->state )
   {
    case GZS_ZIP1:

         rc = gzs_zip1( gz1 );

         break;

    case GZS_ZIP2:

         rc = gzs_zip2( gz1 );

         break;

    case GZS_DEFLATE1:

         rc = gzs_deflate1( gz1 );

         break;

    case GZS_DEFLATE2:

         rc = gzs_deflate2( gz1 );

         break;

    default: 

         gz1->done = 1;

         break;
   }

 return( rc );
}

int gzs_zip1( PGZ1 gz1 )
{
 uch  flags = 0;         

 #ifdef FUTURE_USE
 ush  attr          = 0;
 ush  deflate_flags = 0;
 #endif

 gz1->outcnt = 0;

 gz1->method = DEFLATED;

 put_byte(GZIP_MAGIC[0]); 
 put_byte(GZIP_MAGIC[1]);
 put_byte(DEFLATED);      

 if ( gz1->save_orig_name )
   {
	flags |= ORIG_NAME;
   }

 put_byte(flags);           
 put_long(gz1->time_stamp); 

 gz1->crc = -1; 

 updcrc( gz1, NULL, 0 ); 

 gz1->state = GZS_ZIP2;

 return 0;
}

int gzs_zip2( PGZ1 gz1 )
{
 #ifdef FUTURE_USE
 uch  flags = 0;
 #endif

 ush  attr          = 0;
 ush  deflate_flags = 0; 

 bi_init( gz1, gz1->ofd );
 ct_init( gz1, &attr, &gz1->method );
 lm_init( gz1, gz1->level, &deflate_flags );
 put_byte((uch)deflate_flags); 

 put_byte(OS_CODE); 

 if ( gz1->save_orig_name )
   {
    char *p = gz1_basename( gz1, gz1->ifname );

    do {
	    put_char(*p);

       } while (*p++);
   }

 gz1->header_bytes = (long)gz1->outcnt;

 gz1->state = GZS_DEFLATE1;

 return 0;
}

int gzs_deflate1( PGZ1 gz1 )
{
 if ( !gz1->deflate1_initialized )
   {
    gz1->deflate1_match_available = 0;           
    gz1->deflate1_match_length    = MIN_MATCH-1; 
    gz1->deflate1_initialized     = 1;
   }

 if ( gz1->compr_level <= 3 )
   {
    gz1->done = 1; 

    return 0;
   }

 if ( gz1->lookahead == 0 )
   {
    if ( gz1->deflate1_match_available )
      {
       ct_tally( gz1, 0, gz1->window[gz1->strstart-1] );
      }

    gz1->state = GZS_DEFLATE2;

    return (int) FLUSH_BLOCK(1); 
   }

 #ifdef STAY_HERE_FOR_A_CERTAIN_AMOUNT_OF_ITERATIONS
 
 while( iterations < max_iterations_per_yield )
   {
 #endif

    gz1->ins_h =
    (((gz1->ins_h)<<H_SHIFT) ^ (gz1->window[gz1->strstart+MIN_MATCH-1])) & HASH_MASK;

    prev[ gz1->strstart & WMASK ] = gz1->deflate1_hash_head = head[ gz1->ins_h ];

    head[ gz1->ins_h ] = gz1->strstart;

    gz1->prev_length = gz1->deflate1_match_length, gz1->deflate1_prev_match = gz1->match_start;
    gz1->deflate1_match_length = MIN_MATCH-1;

    if ( gz1->deflate1_hash_head != NIL && gz1->prev_length < gz1->max_lazy_match &&
         gz1->strstart - gz1->deflate1_hash_head <= MAX_DIST)
      {
       gz1->deflate1_match_length = longest_match( gz1, gz1->deflate1_hash_head );

       if ( gz1->deflate1_match_length > gz1->lookahead )
         {
          gz1->deflate1_match_length = gz1->lookahead;
         }

       if (gz1->deflate1_match_length == MIN_MATCH && gz1->strstart-gz1->match_start > TOO_FAR)
         {
          gz1->deflate1_match_length--;
         }
      }

    if ( gz1->prev_length >= MIN_MATCH && gz1->deflate1_match_length <= gz1->prev_length )
      {
       gz1->deflate1_flush =
       ct_tally(gz1,gz1->strstart-1-gz1->deflate1_prev_match, gz1->prev_length - MIN_MATCH);

       gz1->lookahead   -= ( gz1->prev_length - 1 );
       gz1->prev_length -= 2;

       do {
           gz1->strstart++;

           gz1->ins_h =
           (((gz1->ins_h)<<H_SHIFT) ^ (gz1->window[ gz1->strstart + MIN_MATCH-1])) & HASH_MASK;

           prev[ gz1->strstart & WMASK ] = gz1->deflate1_hash_head = head[gz1->ins_h];

           head[ gz1->ins_h ] = gz1->strstart;

          } while (--gz1->prev_length != 0);

       gz1->deflate1_match_available = 0;
       gz1->deflate1_match_length    = MIN_MATCH-1;

       gz1->strstart++;

       if (gz1->deflate1_flush) FLUSH_BLOCK(0), gz1->block_start = gz1->strstart;
      }

    else
      {
       if ( gz1->deflate1_match_available )
         {
          if ( ct_tally( gz1, 0, gz1->window[gz1->strstart-1] ) )
            {
             FLUSH_BLOCK(0), gz1->block_start = gz1->strstart;
            }

          gz1->strstart++;
          gz1->lookahead--;
         }
       else 
         {
          gz1->deflate1_match_available = 1;
          gz1->strstart++;
          gz1->lookahead--;
         }

       while (gz1->lookahead < MIN_LOOKAHEAD && !gz1->eofile )
         {
          fill_window(gz1);
         }
      }

 return 0;
}

int gzs_deflate2( PGZ1 gz1 )
{
 #if !defined(NO_SIZE_CHECK) && !defined(RECORD_IO)
 if (gz1->ifile_size != -1L && gz1->isize != (ulg)gz1->ifile_size)
   {
   }
 #endif

 put_long( gz1->crc      );
 put_long( gz1->bytes_in );

 gz1->header_bytes += 2*sizeof(long);

 flush_outbuf( gz1 );

 gz1->done = 1; 

 return OK;
}

/*--------------------------------------------------------------------------*/
/* COMPRESSION_SUPPORT: END                                                 */
/*--------------------------------------------------------------------------*/

