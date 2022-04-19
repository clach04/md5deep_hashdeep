
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
   
#include "md5deep.h"

int modeMatch = FALSE;
int modeSilent = FALSE;
int modeEstimate = FALSE;
int modeRecursive = FALSE;


void printError(char *fn) {
  if (!modeSilent) 
    fprintf (stderr,"%s: %s: %s\n", __progname,fn,strerror(errno));
}   


void author () {
  printf ("md5deep version %s by %s.\n", MD5DEEP_VERSION,MD5DEEP_AUTHOR);
}


void usage() {
  author();
  fprintf (stderr,"\n");
  fprintf (stderr,
	   "Usage: %s [-v] [-V] [-h] [-m <filename>] [-resbt] [FILES] \n",
	   __progname);

  fprintf (stderr,"-v  - display version number and exit\n");
  fprintf (stderr,"-V  - display copyright information and exit\n");
  fprintf (stderr,"-h  - display this help message and exit\n");
  fprintf (stderr,"-m  - enables matching mode. See README/man page for details\n");
  fprintf (stderr,"-r  - enables recursive mode. All subdirectories are traversed\n");
  fprintf (stderr,"-e  - compute estimated time remaining for each file\n");
  fprintf (stderr,"-s  - enables silent mode. Suppress all error messages\n");
  fprintf (stderr,"-b  - ignored. Present for compatibility with md5sum\n");
  fprintf (stderr,"-t  - ignored. Present for compatibility with md5sum\n");
}


/* The Windows equivalent to __progname, _pgmptr, gives the full path to
   the executable, not just the program's name. This function takes the
   full path and trims it down to just the executable's name. 
   The result is stored in __progname to make everything UNIX-based work.  */
#ifdef __WIN32
void setProgramName() {

  int count,pos = strlen(_pgmptr);

  while (_pgmptr[pos] != DIR_TRAIL_CHAR && pos > 0)
    pos--;

  if (pos == 0) {
    __progname = malloc(sizeof(char) * strlen(_pgmptr) + 1);
    strncpy(__progname,_pgmptr,strlen(_pgmptr));
    return;
  }

  /* Advance past the DIR_TRAIL_CHAR that we found */
  pos++;

  __progname = malloc(sizeof(char) * strlen(_pgmptr - pos + 1));
  for (count = pos ; count <= strlen(_pgmptr) ; count++) {
    __progname[count - pos] = _pgmptr[count];
  }
  return;
}
#endif  /* ifdef __WIN32 */


/* Return the size, in bytes of an open file stream. On error, return -1 */
unsigned long long measureOpenFile(FILE *f){

#ifdef __LINUX
  int descriptor = 0;
  struct stat *info;

  /* I don't know why, but if you don't initialize this value you'll
     get wildly innacurate results when you try to run this function */
  unsigned long long numsectors = 0;
#endif

  unsigned long long total = 0, original = ftello(f);

  if ((fseeko(f,0,SEEK_END)))
    return -1;
  total = ftello(f);
  if ((fseeko(f,original,SEEK_SET)))
    return -1;

#ifdef __LINUX

  /* Block devices, like /dev/hda, don't return a normal filesize.
     If we are working with a block device, we have to ask the operating
     system to tell us the true size of the device. 
     
     The following only works on Linux as far as I know. If you know
     how to port this code to another operating system, please contact
     the current maintainer of this program! */

  descriptor = fileno(f);
  info = (struct stat*)malloc(sizeof(struct stat));

  /* I'd prefer not to use fstat as it will follow symbolic links. We don't
     follow symbolic links. That being said, all symbolic links *should*
     have been caught before we got here. */

  fstat(descriptor,info);
  if (S_ISBLK(info->st_mode)) {
    if (ioctl(descriptor, BLKGETSIZE, &numsectors)){
#ifdef __DEBUG
      perror("BLKGETSIZE failed");
#endif
    } else {

      /* We're going to assume that this device has 512 byte sectors.
	 Eventually we should add a way to get the real number of sectors
	 from the device. */
      total = numsectors * 512;
    }
  }
  
  free(info);
#endif /* ifdef __LINUX */
  
  return (total - original);
}


void updateDisplay(time_t start, time_t now, 
		   unsigned long long bytesRead,
		   unsigned long long totalBytes) {

  int hour,min, elapsed  = now - start;
  unsigned long remaining;
  char hourS[4], minS[4], secS[4], output[10];
  long mbRead  = bytesRead  / ONE_MEGABYTE;
  long mbTotal = totalBytes / ONE_MEGABYTE;

  /* If we couldn't compute the input file size, punt */
  if (mbTotal == 0) {
    fprintf(stderr,
	    "\r%ldMB complete. Unable to estimate remaining time.",mbRead);
    return;
  }

  /* Our estimate of the number of seconds remaining */
  remaining = (unsigned long long)floor(((double)mbTotal/(double)mbRead - 1) * elapsed);

  /* We don't care if it's going to take us more than 24 hours. If you're
     hashing something that big, to quote the movie Jaws:

                   "We're gonna need a bigger boat."                   */

  hour = floor((double)remaining/3600);
  remaining -= (hour * 3600);
  if (hour < 10) 
    snprintf(hourS,3,"0%d",hour);
  else 
    snprintf(hourS,3,"%d",hour);

  min = floor((double)remaining/60);
  remaining -= min * 60;
  if (min < 10) 
    snprintf(minS,3,"0%d",min);
  else 
    snprintf(minS,3,"%d",min);

  if (remaining < 10) 
    snprintf(secS,3,"0%ld",remaining);
  else 
    snprintf(secS,3,"%ld",remaining);
  
  snprintf(output,10,"%s:%s:%s",hourS,minS,secS);
  fprintf(stderr,
	  "\r%ldMB of %ldMB complete. %s remaining",mbRead,mbTotal,output);
}



char *MD5File(FILE *fp) {

  time_t start,now,last = 0;
  unsigned long long total = 0,fileSize = 0;
  MD5_CTX md;
  unsigned char sum[MD5_HASH_LENGTH];
  unsigned char buf[BUFSIZ];
  static char result[2 * MD5_HASH_LENGTH + 1];
  static char hex[] = "0123456789abcdef";
  int     i,buflen;
  bool estimateThisFile = FALSE;

  if (modeEstimate) {
    fileSize = measureOpenFile(fp);

    /* If were are unable to compute the file size, we can't very well
       estimate how long it's going to take us, now can we! That being
       said, we don't want to change the global variable in case there
       are other files later on that we can compute estimates for. */
    if (fileSize != -1) {
      estimateThisFile = TRUE;
    }
  }

  MD5Init(&md);

  if (estimateThisFile) {
    time(&start);
    last = start;
  }
 
  while ((buflen = fread(buf, 1, BUFSIZ, fp)) > 0) {

    MD5Update(&md, buf, buflen);

    if (estimateThisFile) {
       total += BUFSIZ;
       time(&now);

       /* We only update the on-screen display if it's taking us more
	  than one second to do this round of operations. */
       if (last != now) {
         last = now;
         updateDisplay(start,now,total,fileSize);
       }
    }
  }

  /* If we've been printing, we now need to clear the line. */
  if (estimateThisFile)   
    fprintf(stderr,"\r                                                   \r");

  MD5Final(sum, &md);
  
  for (i = 0; i < MD5_HASH_LENGTH; i++) {
    result[2 * i] = hex[(sum[i] >> 4) & 0xf];
    result[2 * i + 1] = hex[sum[i] & 0xf];
  }
  return (result);
}


void md5(char *filename) {
  FILE *f;
  if ((f = fopen(filename,"rb")) == NULL) {
    printError(filename);
    return;
  }

  if (modeMatch) { 
    if (isKnownHash(MD5File(f))) {
      printf ("%s\n", filename);
    } 
  } else {
    printf("%s  %s\n",MD5File(f),filename);
  }
  fclose(f);
}


int isDirectory (char *f) {
  DIR *dir;
  if ((dir = opendir(f)) == NULL)
    return FALSE;
  closedir(dir);
  return TRUE;
}


int examineFile(char *fn) { 

  /* By default we "fail open." That is, any file type we can't
     identify as something we *shouldn't* process is something that 
     we must process */
  int status = FILE_REGULAR;
  struct stat *info = (struct stat*)malloc(sizeof(struct stat));
  if (lstat(fn,info)) {
    printError(fn);
    free(info);
    return FILE_ERROR;
  }

  /* These POSIX macros are defined on the stat(2) man page */
  if (S_ISDIR(info->st_mode)) {
    status = FILE_DIRECTORY;
  }

  /* There are no symbolic links on Windows */
#ifndef __WIN32
  if (S_ISLNK(info->st_mode)) {
    status = FILE_SYMLINK;
  }
#endif
  
  free(info);
  return status;
}



/* Returns TRUE if the directory is either "." or ".."
   The strlen calls are necessary to avoid flagging ".foo" et al. */
int isSpecialDirectory (char *d) {
  return ((!strncmp(d,".",1) && (strlen(d) == 1)) ||
	  (!strncmp(d,"..",2) && (strlen(d) == 2)));
}

void processFile(char *path,char *dir);

void processDirectory(char *fn) {

  DIR *currentDir;
  struct dirent *entry;

  if ((currentDir = opendir(fn)) == NULL) {
    printError(fn);
    return;
  }    
  
  while ((entry = readdir(currentDir)) != NULL) 
    processFile(fn,entry->d_name);
  closedir(currentDir);
}


void processFile(char *path,char *dir) {

  int status;
  char *fn;

#ifdef __DEBUG
  printf ("Checking: path: %s   dir: %s\n",path,dir);
#endif

  /* The path should not have a trailing slash character as we're
     going to add our own to create the full filename */
  if (strlen(path) > 1) {
    if (path[strlen(path)-1] == DIR_TRAIL_CHAR) {
      path[strlen(path)-1] = 0;
    }
  }

  /* Check that we're not trying to look at "." or ".."
     These special directories would cause us to move back UP
     the directory tree. We only want to go down. */
  if (isSpecialDirectory(dir))
    return;

  /* We have to add two extra characters to the current string lengths
     in order to hold the '/' we insert and the terminator character */
  fn = (char *)malloc(sizeof(char) * (strlen(path) + strlen(dir) + 2));
  sprintf(fn,"%s%c%s",path,DIR_TRAIL_CHAR,dir);
  
  status = examineFile(fn);
  switch (status) {

    /* There are no symlinks on Windows. Because we're going to go 
       through this loop time and time again, we should do everything
       we can to save time. Yes, the amount of time saved will pale in 
       comparison to how long it takes to read files from the disk,
       but hey, those nanoseconds add up after a while! :) */
#ifndef __WIN32
  case FILE_SYMLINK:
    if (!modeSilent) 
      fprintf (stderr,"%s: %s: Is a symbolic link\n", __progname,fn);
    break;
#endif

  case FILE_REGULAR:
    md5(fn);
    break;

  case FILE_DIRECTORY:
    /* The fact that we're in this function means that we're in
       recursive mode and therefore we don't have to check for it. */
    processDirectory(fn);
    break;
    
  case FILE_ERROR:
    /* An error message has already been printed */
    break;
    
  case FILE_UNKNOWN:
  default:
    /* This should never happen */
    if (!modeSilent) 
      fprintf (stderr,
	       "%s: %s: Unknown file type. Ignored.\n", __progname, fn);
  }

  free(fn);
  return;
}


void addMatchingFile(char *fn) {
  if (!loadMatchFile(fn)) {
    if (!modeSilent) {
      fprintf(stderr,"Unable to load known hashes file %s\n", fn);
    }
  }
}


void processCommandLine(int argc, char **argv) {

  char i;

#ifndef MD5DEEP_GETOPT_END
    #ifdef __TOS_AIX__
        #define MD5DEEP_GETOPT_END (255)
    #else
        #define MD5DEEP_GETOPT_END (-1)
    #endif
#endif
  while ((i=getopt(argc,argv,"m:serhvVbt")) != MD5DEEP_GETOPT_END) {
    switch (i) {

    case 'm':
      modeMatch = TRUE;
      addMatchingFile(optarg);
      break;

    case 's':
      modeSilent = TRUE;
      break;

    case 'e':
      modeEstimate = TRUE;
      break;

    case 'r':
      modeRecursive = TRUE;
      break;

    case 'h':
      usage();
      exit (1);

    case 'v':
      author();
      exit (1);

    case 'V':
      printf (MD5DEEP_COPYRIGHT);
      exit (1);

      /* These are only for compatibility with md5sum */
    case 'b':
    case 't':
      break;

    default:
      usage();
      exit (1);

    }
  }
}


/* The variable fn has been resolved to its full path. If the user
   really specified a symlink, we won't know that unless we have
   the argument as well. We use the arg to check for symlinks */
void processInput(char *arg, char *fn) {


  int status = examineFile(arg);
  
#ifdef __DEBUG
  printf("Checking input: %s (%s)\n", arg,fn);
#endif 

  switch (status) {

#ifndef __WIN32
  case FILE_SYMLINK:
    if (!modeSilent) 
      fprintf (stderr,"%s: %s: Is a symbolic link\n", __progname,arg);
    return;
#endif

  case FILE_REGULAR:
    md5(fn);
    return;

  case FILE_DIRECTORY:
    if (modeRecursive) 
      processDirectory(fn);
    else
      if (!modeSilent) 
	fprintf (stderr,"%s: %s: Is a directory\n", __progname,fn);
    return;

  case FILE_ERROR:
    /* An error message has already been printed */
    return;

    /* This should never happen */
  case FILE_UNKNOWN:
  default:
    if (!modeSilent) 
      fprintf (stderr,"%s: %s: Unknown file type. Ignored.\n", __progname,fn);
  }
}
  


int main(int argc, char **argv) {

  char *fn = (char*)malloc(sizeof(char)* PATH_MAX);

#ifdef __WIN32
  setProgramName();
#endif

  processCommandLine(argc,argv);

  /* Anything left on the command line at this point is a file
     or directory we're supposed to process */
  argv += optind;
  while (*argv != NULL) {

    if (realpath(*argv,fn) == NULL)
      printError(fn);
    else 
      processInput(*argv,fn);
    
    argv++;
  }

  free(fn);
  return 0;
}
