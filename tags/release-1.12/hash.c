
/* MD5DEEP - hash.c
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

#define MD5DEEP_IDEAL_BLOCK 8192

uint32_t MD5DEEP_BLOCK_SIZE;

#ifndef HASH_ALGORITHM
#error No hash algorithm defined! See algorithms.h
#endif

typedef struct hash_info {
  char *full_name;
  char *short_name;

  // The size of the input file, in megabytes
  uint64_t total_megs;

  uint64_t total_bytes;
  uint64_t bytes_read;

  char *result;

  FILE *handle;
  int is_stdin;
} hash_info;


void update_display(time_t elapsed, hash_info *h)
{
  char msg[LINE_LENGTH];
  uint8_t hour,min;
  uint64_t seconds, mb_read;

  memset(msg,0,LINE_LENGTH);
  
  /* If we've read less than one MB, then the computed value for mb_read 
     will be zero. Later on we may need to divide the total file size, 
     total_megs, by mb_read. Dividing by zero can create... problems */
  if (h->bytes_read < ONE_MEGABYTE)
    mb_read = 1;
  else
    mb_read = h->bytes_read / ONE_MEGABYTE;

  if (h->total_megs == 0) 
  {
    snprintf(msg,LINE_LENGTH,
	     "%s: %"PRIu64"MB done. Unable to estimate remaining time.",
	     h->short_name,mb_read);
  }
  else 
  {
    // Our estimate of the number of seconds remaining
    seconds = (off_t)floor(((double)h->total_megs/mb_read - 1) * elapsed);

    /* We don't care if the remaining time is more than one day.
       If you're hashing something that big, to quote the movie Jaws:
       
              "We're gonna need a bigger boat."                   */
    
    hour = floor((double)seconds/3600);
    seconds -= (hour * 3600);
    
    min = floor((double)seconds/60);
    seconds -= min * 60;
    
    snprintf(msg,LINE_LENGTH,
	     "%s: %"PRIu64"MB of %"PRIu64"MB done, %02"PRIu8":%02"PRIu8":%02"PRIu64" left%s",
	     h->short_name,mb_read,h->total_megs,hour,min,seconds,BLANK_LINE);
  }    

  fprintf(stderr,"\r%s",msg);
}


void shorten_filename(char *dest, char *src)
{
  char *chopped, *basen;

  if (strlen(src) < MAX_FILENAME_LENGTH)
  {
    strncpy(dest,src,MAX_FILENAME_LENGTH);
    return;
  }

  basen = strdup(src);
  chopped = basename(basen);  
  free(basen);

  if (strlen(chopped) < MAX_FILENAME_LENGTH)
  {
    strncpy(dest,chopped,MAX_FILENAME_LENGTH);
    return;
  }
  
  chopped[MAX_FILENAME_LENGTH - 3] = 0;
  snprintf(dest,MAX_FILENAME_LENGTH,"%s...",chopped);
}


/* Returns TRUE if errno is currently set to a fatal error. That is,
   an error that can't possibly be fixed while trying to read this file */
int fatal_error(void)
{
  switch(errno) 
    {
    case EACCES:   // Permission denied
    case ENODEV:   // Operation not supported (e.g. trying to read 
                   //   a write only device such as a printer)
    case EBADF:    // Bad file descriptor
    case EFBIG:    // File too big
    case ETXTBSY:  // Text file busy
      /* The file is being written to by another process.
	 This happens with Windows system files */

      return TRUE;  
    }
  
  return FALSE;
}


int compute_hash(uint64_t mode, hash_info *h, HASH_CONTEXT *md)
{
  time_t start_time,current_time,last_time = 0;
  uint64_t current_read;
  unsigned char buffer[MD5DEEP_IDEAL_BLOCK];
  uint32_t mysize, remaining;

  /* Although we need to read MD5DEEP_BLOCK_SIZE bytes before
     we exit this
     function, we may not be able to do that in one fell swoop.
     Instead we read in blocks of 8192 bytes (or as needed) to 
     get how many bytes we need */

  if (MD5DEEP_BLOCK_SIZE < MD5DEEP_IDEAL_BLOCK)
    mysize = MD5DEEP_BLOCK_SIZE;
  else
    mysize = MD5DEEP_IDEAL_BLOCK;

  remaining = MD5DEEP_BLOCK_SIZE;

  if (M_ESTIMATE(mode))
  {
    time(&start_time);
    last_time = start_time;
  }

  while (TRUE) 
  {    
    /* clear the buffer in case we hit an error and need to pad the hash */
    memset(buffer,0,mysize);
    
    current_read = fread(buffer, 1, mysize, h->handle);

    /* If an error occured, display a message but still add this block */
    if (ferror(h->handle))
    {
      if (!M_SILENT(mode))
	fprintf(stderr,"%s: %s: error at offset %"PRIu64": %s%s", 
		__progname,h->full_name,h->bytes_read,strerror(errno),NEWLINE);
      
      if (fatal_error())
	return FALSE;

      HASH_Update(md, buffer, mysize);
      h->bytes_read += mysize;

      clearerr(h->handle);
      
      /* The file pointer's position is now undefined. We have to manually
	 advance it to the start of the next buffer to read. */
      fseek(h->handle,SEEK_SET,h->bytes_read);
    } 
    else
    {
      /* If we hit the end of the file, we read less than MD5DEEP_BLOCK_SIZE bytes
         and must reflect that in how we update the hash. */
      HASH_Update(md, buffer, current_read);
      h->bytes_read += current_read;
    }
    
    /* Check if we've hit the end of the file */
    if (feof(h->handle))
    {	
      /* If we've been printing, we now need to clear the line. */
      if (M_ESTIMATE(mode))
	fprintf(stderr,"\r%s\r",BLANK_LINE);

      return TRUE;
    }

    // In piecewise mode we only hash one block at a time
    if (M_PIECEWISE(mode))
    {
      remaining -= mysize;
      if (remaining == 0)
	return TRUE;

      if (remaining < MD5DEEP_IDEAL_BLOCK)
	mysize = remaining;
    }
    
    if (M_ESTIMATE(mode)) 
    {
      time(&current_time);
      
      /* We only update the display only if a full second has elapsed */
      if (last_time != current_time) 
      {
	last_time = current_time;
	update_display(current_time - start_time,h);
      }
    }
  }      
}


void display_size(uint64_t mode, hash_info *h)
{
  if (M_DISPLAY_SIZE(mode))
  {
    // We reserve ten characters for digits followed by two spaces
    if (h->bytes_read > 9999999999LL)
      printf ("9999999999  ");
    else
      printf ("%10"PRIu64"  ", h->bytes_read);      
  }	
}


char display_asterisk(uint64_t mode)
{
  if (M_ASTERISK(mode))
    return '*';
  return ' ';
}


int display_match_result(uint64_t mode, hash_info *h)
{  
  int known_hash;
  char *known_fn;

  if ((known_fn = (char *)malloc(sizeof(char) * PATH_MAX)) == NULL)
  {
    print_error(mode,h->full_name,"Out of memory");
    return TRUE;;
  }

  known_hash = is_known_hash(h->result,known_fn);
  if ((known_hash && M_MATCH(mode)) || (!known_hash && M_MATCHNEG(mode)))
  {
    display_size(mode,h);

    if (M_DISPLAY_HASH(mode))
    {
      printf ("%s %c",h->result,display_asterisk(mode));
    }

    if (M_WHICH(mode))
    {
      if (known_hash && M_MATCH(mode))
	printf ("%s matched %s", h->full_name,known_fn);
      else
	printf ("%s does NOT match", h->full_name);
    }
    else
      printf ("%s", h->full_name);

    make_newline(mode);
  }
  
  free(known_fn);
  return FALSE;
}



int display_hash(uint64_t mode, hash_info *h)
{
  /* We can't call display_size here because we don't know if we're
     going to display *anything* yet. If we're in matching mode, we
     have to evaluate if there was a match first. */
  if (M_MATCH(mode) || M_MATCHNEG(mode))
    return display_match_result(mode,h);

  display_size(mode,h);
  printf ("%s", h->result);

  if (M_QUIET(mode))
    printf ("  ");
  else
  {
    if (M_PIECEWISE(mode) ||
	!(h->is_stdin))
      printf(" %c%s", display_asterisk(mode),h->full_name);
  }

  make_newline(mode);

  return FALSE;
}


int hash(uint64_t mode, hash_info *h)
{
  int done = FALSE, status = FALSE;
  HASH_CONTEXT md;
  unsigned char sum[HASH_LENGTH];
  static char result[2 * HASH_LENGTH + 1];
  static char hex[] = "0123456789abcdef";
  int i;
  char *tmp_name = NULL;

  h->bytes_read = 0;
  
  MD5DEEP_BLOCK_SIZE = MD5DEEP_IDEAL_BLOCK;

  if (M_PIECEWISE(mode))
  {
    MD5DEEP_BLOCK_SIZE = piecewise_block;

    // We copy out the original file name and saved it in tmp_name
    tmp_name = h->full_name;
    h->full_name = (char *)malloc(sizeof(char) * PATH_MAX);
    if (NULL == h->full_name)
      return TRUE;
  }

  while (!done)
  {
    h->result = NULL;
    HASH_Init(&md);
    
    if (M_PIECEWISE(mode))
    {
      /* If the BLOCK_SIZE is larger than the file, this keeps the
	 offset values correct */
      if (h->bytes_read + MD5DEEP_BLOCK_SIZE >  h->total_bytes)
	snprintf(h->full_name,PATH_MAX,"%s offset %"PRIu64"-%"PRIu64,
		 tmp_name, h->bytes_read, h->total_bytes);
      else
	snprintf(h->full_name,PATH_MAX,"%s offset %"PRIu64"-%"PRIu64,
		 tmp_name, h->bytes_read, h->bytes_read + MD5DEEP_BLOCK_SIZE);
    }

    if (!compute_hash(mode,h,&md))
      return TRUE;

    HASH_Final(sum, &md);  
    for (i = 0; i < HASH_LENGTH; ++i) 
    {
      result[2 * i] = hex[(sum[i] >> 4) & 0xf];
      result[2 * i + 1] = hex[sum[i] & 0xf];
    }
  
    h->result = strdup(result);
    if (h->result == NULL)
    {
      print_error(mode,h->full_name,"Out of memory");
      return TRUE;
    }
  
    /* Under not matched mode, we only display those known hashes that
       didn't match any input files. Thus, we don't display anything now.
       The lookup is to mark those known hashes that we do encounter */
    if (M_NOT_MATCHED(mode))
      is_known_hash(h->result,NULL);
    else
      status = display_hash(mode,h);

    free(h->result);
    if (M_PIECEWISE(mode))
      done = feof(h->handle);
    else
      done = TRUE;
  }

  if (M_PIECEWISE(mode))
  {
    free(h->full_name);
    h->full_name = tmp_name;
  }

  return status;
}


/* The basename function kept misbehaving on OS X, so I rewrote it.
   This function isn't perfect, nor is it designed to be. Because
   we're guarenteed to be working with a file here, there's no way
   that s will end with a DIR_SEPARATOR (e.g. /foo/bar/). This function
   will not work properly for a string that ends in a DIR_SEPARATOR */
int my_basename(char *s)
{
  size_t pos = strlen(s);
  if (0 == pos || pos > PATH_MAX)
    return TRUE;
  
  while(s[pos] != DIR_SEPARATOR && pos > 0)
    --pos;
  
  // If there were no DIR_SEPARATORs in the string, we were still successful!
  if (0 == pos)
    return FALSE;

  // The current character is a DIR_SEPARATOR. We advance one to ignore it
  ++pos;
  shift_string(s,0,pos);
  return FALSE;
}


int setup_barename(uint64_t mode, hash_info *h, char *file_name)
{
  char *basen = strdup(file_name);
  if (basen == NULL)
  {
    print_error(mode,file_name,"Out of memory");
    return TRUE;
  }

  if (my_basename(basen))
  {
    free(basen);
    print_error(mode,file_name,"Illegal filename");
    return TRUE;
  }
  h->full_name = basen;
  return FALSE;
}


int hash_file(uint64_t mode, char *file_name)
{
  int status = STATUS_OK;
  hash_info *h = (hash_info *)malloc(sizeof(hash_info));
  if (h == NULL)
  {
    print_error(mode,file_name,"Out of memory");
    return TRUE;
  }

  h->is_stdin = FALSE;

  if (M_BARENAME(mode))
  {
    if (setup_barename(mode,h,file_name))
      return TRUE;
  }
  else
    h->full_name = file_name;

  if ((h->handle = fopen(file_name,"rb")) != NULL)
  {

    h->total_bytes = find_file_size(h->handle);

    if (M_ESTIMATE(mode))
    {
      // The find file size returns a value of type off_t, so we must cast it
      h->total_megs = h->total_bytes / ONE_MEGABYTE;
      h->short_name = (char *)malloc(sizeof(char) * MAX_FILENAME_LENGTH);
      shorten_filename(h->short_name,h->full_name);    
    }    

    status = hash(mode,h);
    fclose(h->handle);

    if (M_ESTIMATE(mode))
      free(h->short_name);

    if (M_BARENAME(mode))
      free(h->full_name);
  }
  else
  {
    print_error(mode,file_name,strerror(errno));
    status = TRUE;
  }
  
  free(h);
  return status;
}


int hash_stdin(uint64_t mode)
{
  int status;
  hash_info *h = (hash_info *)malloc(sizeof(hash_info));
  if (NULL == h)
  {
    print_error(mode,"stdin","Out of memory");
    return TRUE;
  }

  h->full_name = strdup("stdin");
  h->is_stdin  = TRUE;
  h->handle    = stdin;

  if (M_ESTIMATE(mode))
  {
    h->short_name = h->full_name;
    h->total_megs = 0LL;
  }

  status = hash(mode,h);

  free(h->full_name);
  free(h);
  return status;
}