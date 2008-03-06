
/* $Id$ */

#include "main.h"

static filetype_t 
identify_file(state *s, char *fn, FILE *handle)
{
  
  
  return file_hashdeep_10;
}



status_t add_hash_to_algorithm(state *s,
			       hashname_t alg,
			       file_data_t *f)
{
  if (NULL == s || NULL == f)
    return status_unknown_error;
  
  if (hashtable_add(s,alg,f))
    return status_unknown_error;
  

  return status_ok;
}



status_t add_hash(state *s, file_data_t *f)
{
  hashname_t i;

  if (NULL == s || NULL == s)
    return status_unknown_error;

  for (i = 0 ; i < NUM_ALGORITHMS ; ++i)
    {
      if (s->hashes[i]->inuse)
	add_hash_to_algorithm(s,i,f);
    }

  return status_ok;

}


status_t load_match_file(state *s, char *fn)
{
  FILE * handle;
  filetype_t type;

  if (NULL == s || NULL == fn)
    return status_omg_ponies;

  handle = fopen(fn,"rb");
  if (NULL == handle)
  {
    print_error(s,"%s: %s: %s", __progname, fn, strerror(errno));
    return status_file_error;
  }
  
  type = identify_file(s,fn,handle);
  if (file_unknown == type)
  {
    print_error(s,"%s: %s: Unable to identify file format", __progname, fn);
    fclose(handle);
    return status_file_error;
  }
  

  fclose(handle);
  return status_ok;
}


status_t is_known_file(state *s)
{
  hashname_t i;

  for (i = 0 ; i < NUM_ALGORITHMS; ++i)
  {
    if (s->hashes[i]->inuse)
    {

    }
  }

  return status_no_match;
}
