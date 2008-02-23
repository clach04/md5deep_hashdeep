
/* MD5DEEP
 *
 * By Jesse Kornblum
 *
 * This is a work of the US Government. In accordance with 17 USC 105,
 * copyright protection is not available for any work of the US Government.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

   
#ifndef __MD5DEEP_H
#define __MD5DEEP_H

/* Version information is defined in the Makefile */

#define AUTHOR      "Jesse Kornblum"
#define COPYRIGHT   "This program is a work of the US Government. "\
"In accordance with 17 USC 105,\n"\
"copyright protection is not available for any work of the US Government.\n"\
"This is free software; see the source for copying conditions. There is NO\n"\
"warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n"


#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef __LINUX
#include <sys/ioctl.h>
#include <sys/mount.h>
#endif 


#define TRUE   1
#define FALSE  0
#define ONE_MEGABYTE  1048576

/* Strings have to be long enough to handle inputs from matched hashing files.
   The NSRL is already larger than 256 bytes. We go longer to be safer. */
#define MAX_STRING_LENGTH   1024
#define HASH_STRING_LENGTH    32

/* The length of BLANK_LINE define should correspond to the line length! */
#ifdef __WIN32
#define LINE_LENGTH 72
#define BLANK_LINE \
"                                                                        "
#else
#define LINE_LENGTH 74
#define BLANK_LINE \
"                                                                          "
#endif

#define MAX_FILENAME_LENGTH   LINE_LENGTH - 41

/* These are the types of files that we can match against */
#define TYPE_PLAIN        0
#define TYPE_HASHKEEPER   1
#define TYPE_NSRL_15      2
#define TYPE_NSRL_20      3
#define TYPE_ILOOK        4
#define TYPE_UNKNOWN    254


#define mode_none          0x00
#define mode_recursive     0x01
#define mode_estimate      0x02
#define mode_silent        0x04
#define mode_match         0x08
#define mode_match_neg     0x10
#define mode_display_hash  0x20

/* Modes 64 and 128 (0x40 and 0x80) are reserved for future use */

#define mode_regular       0x100
#define mode_directory     0x200
#define mode_door          0x400
#define mode_block         0x800
#define mode_character     0x1000
#define mode_pipe          0x2000
#define mode_socket        0x4000
#define mode_symlink       0x8000
#define mode_expert        0x10000


/* These are the types of files we can encounter while hashing */
#define file_regular    0
#define file_directory  1
#define file_door       3
#define file_block      4
#define file_character  5
#define file_pipe       6
#define file_socket     7
#define file_symlink    8
#define file_unknown  254

#define M_MATCH(A)         A & mode_match
#define M_MATCHNEG(A)      A & mode_match_neg
#define M_RECURSIVE(A)     A & mode_recursive
#define M_ESTIMATE(A)      A & mode_estimate
#define M_SILENT(A)        A & mode_silent
#define M_DISPLAY_HASH(A)  A & mode_display_hash

#define M_EXPERT(A)        A & mode_expert
#define M_REGULAR(A)       A & mode_regular
#define M_BLOCK(A)         A & mode_block
#define M_CHARACTER(A)     A & mode_character
#define M_PIPE(A)          A & mode_pipe
#define M_SOCKET(A)        A & mode_socket
#define M_DOOR(A)          A & mode_door
#define M_SYMLINK(A)       A & mode_symlink




#ifdef __SOLARIS
#define   u_int32_t   unsigned int
#define   u_int64_t   unsigned long
#endif 


/* The only time we're *not* on a UNIX system is when we're on Windows */
#ifndef __WIN32
#ifndef __UNIX
#define __UNIX
#endif  /* ifndef __UNIX */
#endif  /* ifndef __WIN32 */


#ifdef __UNIX

#include <libgen.h>

/* This avoids compiler warnings on older systems */
int fseeko(FILE *stream, off_t offset, int whence);
off_t ftello(FILE *stream);

#define  DIR_SEPARATOR   '/'

#endif /* #ifdef __UNIX */



/* Code specific to Microsoft Windows */
#ifdef __WIN32

/* Allows us to open standard input in binary mode by default 
   See http://gnuwin32.sourceforge.net/compile.html for more */
#include <fcntl.h>

/* By default BUFSIZ is 512 on Windows. We make it 8192 so that it's 
   the same as UNIX. While that shouldn't mean anything in terms of
   computing the hash values, it should speed us up a little bit. */
#ifdef BUFSIZ
#undef BUFSIZ
#endif
#define BUFSIZ 8192

/* It would be nice to use 64-bit file lengths in Windows */
#define ftello   ftell
#define fseeko   fseek

#define  snprintf         _snprintf

#define  DIR_SEPARATOR   '\\'
#define  u_int32_t        unsigned long

/* We create macros for the Windows equivalent UNIX functions.
   No worries about lstat to stat; Windows doesn't have symbolic links */
#define lstat(A,B)      stat(A,B)

#define realpath(A,B)   _fullpath(B,A,PATH_MAX) 

/* Not used in md5deep anymore, but left in here in case I 
   ever need it again. Win32 documentation searches are evil.
   int asprintf(char **strp, const char *fmt, ...);
*/

char *basename(char *a);
extern char *optarg;
extern int optind;
int getopt(int argc, char *const argv[], const char *optstring);

#endif   /* ifdef _WIN32 */


/* On non-glibc systems we have to manually set the __progname variable */
#ifdef __GLIBC__
extern char *__progname;
#else
char *__progname;
#endif /* ifdef __GLIBC__ */


/* -----------------------------------------------------------------
   Function definitions
   ----------------------------------------------------------------- */


/* Functions from matching (match.c) */
int load_match_file(off_t mode, char *filename);
int is_known_hash(char *h);

/* Functions for file evaluation (files.c) */
int hash_file_type(FILE *f);
int findHashValueinLine(char *buf, int fileType);

/* Dig into file hierarchies */
void process(off_t mode, char *input);

/* Hashing functions */
char *md5_file(off_t mode, FILE *fp, char *file_name);
void md5(off_t mode, char *filename);

/* Miscellaneous helper functions */
void shift_string(char *fn, int start, int new_start);
void print_error(off_t mode, char *fn, char *msg);

/* Return the size, in bytes of an open file stream. On error, return -1 */
off_t find_file_size(FILE *f);


/* -----------------------------------------------------------------
   MD5 definitions
   ----------------------------------------------------------------- */

#define MD5_HASH_LENGTH  16

struct MD5Context {
	u_int32_t buf[4];
	u_int32_t bits[2];
	unsigned char in[64];
};

void MD5Init(struct MD5Context *context);
void MD5Update(struct MD5Context *context, unsigned char const *buf,
	       unsigned len);
void MD5Final(unsigned char digest[16], struct MD5Context *context);
void MD5Transform(u_int32_t buf[4], u_int32_t const in[16]);

/*
 * This is needed to make RSAREF happy on some MS-DOS compilers.
 */
typedef struct MD5Context MD5_CTX;



#endif /* __MD5DEEP_H */



