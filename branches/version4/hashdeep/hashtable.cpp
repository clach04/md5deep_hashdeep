#include "main.h"

/* $Id$ */

status_t file_data_compare(state *s, file_data_t *a, file_data_t *b)
{
    int partial_match = FALSE, partial_failure = FALSE;
  
  if (NULL == a || NULL == b || NULL == s)
    return status_unknown_error;  
  
  /* We first compare the hashes because they should tell us the fastest if we're
     looking at different files. Then the file size and finally the file name. */
  for (int i = 0 ; i < NUM_ALGORITHMS ; ++i)  {
    if (s->hashes[i].inuse)
    {
      /* We have to avoid calling STRINGS_EQUAL on NULL values, but
         don't have to worry about capitalization */
      if (STRINGS_CASE_EQUAL(a->hash_hex[i],b->hash_hex[i])) partial_match = TRUE;
      else partial_failure = TRUE;
    }
  }

  /* Check for when there are no intersecting hashes */
  if (!partial_match && !partial_failure)
    return status_no_match;

  if (partial_failure)  {
    if (partial_match)
      return status_partial_match;
    else
      return status_no_match;
  }
  
  if (a->file_size != b->file_size){
    return status_file_size_mismatch;
  }
  
  if (a->file_name != b->file_name){
    return status_file_name_mismatch;
  }
  
  return status_match;
}


/* These two functions are the "hash" functions for the hash table. 
   Because the original implementation of this code was for storing
   md5 hashes, I used the name "translate" to avoid confusion. */


/* Convert a single hexadecimal character to decimal. If c is not a valid
   hex character, returns 0. */
static int translate_char(char c) 
{
  /* If this is a digit */
  if (c > 47 && c < 58) 
    return (c - 48);
  
  c = toupper(c);
  /* If this is a letter... 'A' should be equal to 10 */
  if (c > 64 && c < 71) 
    return (c - 55);

  return 0;
}
    
/* Translates a hex value into it's appropriate index in the array.
   In reality, this just turns the first HASH_SIG_FIGS into decimal */
static uint64_t translate(const std::string &n) 
{ 
  int count;
  uint64_t total = 0, power = 1;

  for (count = HASH_TABLE_SIG_FIGS - 1 ; count >= 0 ; count--)   {
    total += translate_char(n[count]) * power;
    power *= 16;
  }

  return total;
}



void hashtable_init(hashtable_t *t)
{
  uint64_t i;
  for (i = 0 ; i < HASH_TABLE_SIZE ; ++i)
    t->member[i] = NULL;
}



status_t hashtable_add(state *s, hashid_t alg, file_data_t *f)
{
  hashtable_entry_t *n, *temp;
  hashtable_t *t = s->hashes[alg].known;

  if (NULL == t || NULL == f)
    return status_unknown_error;
  
  uint64_t key = translate(f->hash_hex[alg]);
  
  if (NULL == t->member[key])
  {
    n = (hashtable_entry_t *)malloc(sizeof(hashtable_entry_t));
    if (NULL == n)
      return status_out_of_memory;
    
    n->next = NULL;
    n->data = f;
    
    t->member[key] = n;
      return status_ok;
  }

  temp = t->member[key];

  /* If this value is already in the table, we don't need to add it again */
  if (file_data_compare(s,temp->data,f) == status_match)
    return status_ok;

  while (temp->next != NULL)
  {
    temp = temp->next;
    
    if (file_data_compare(s,temp->data,f) == status_match)
      return status_ok;
  }

  n = (hashtable_entry_t *)malloc(sizeof(hashtable_entry_t));
  if (NULL == n)
    return status_out_of_memory;
  
  n->next = NULL;
  n->data = f;
  
  temp->next = n;
  return status_ok;
}



void hashtable_destroy(hashtable_entry_t *e)
{
  hashtable_entry_t *tmp;

  if (NULL == e)
    return;
  
 while (e != NULL)
  {
    tmp = e->next;
    free(e);
    e = tmp;
  }
}


hashtable_entry_t * 
hashtable_contains(state *s, hashid_t alg)
{
  hashtable_entry_t *ret = NULL, *n, *temp, *last = NULL;
  hashtable_t *t; 
  uint64_t key;
  file_data_t * f;
  status_t status;

  f = s->current_file;
  key = translate(f->hash_hex[alg]);
  t   = s->hashes[alg].known;

  //  print_status("key: %"PRIx64, key);

  if (NULL == t->member[key])
    return NULL;

  //  print_status("Found one or more possible hits.");
  
  temp = t->member[key];

  status = file_data_compare(s,temp->data,f);
  //  print_status("First entry %d", status);
  if (status != status_no_match)
    {
      //      print_status("hit on first entry %d", status);
      ret = (hashtable_entry_t *)malloc(sizeof(hashtable_entry_t));
      ret->next = NULL;
      ret->status = status;
      ret->data = temp->data;
      last = ret;
    }

  while (temp->next != NULL)
    {
      temp = temp->next;
    
      status = file_data_compare(s,temp->data,f);
      
      if (status != status_no_match)
	{
	  if (NULL == ret)
	    {
	      ret = (hashtable_entry_t *)malloc(sizeof(hashtable_entry_t));
	      ret->next = NULL;
	      ret->data = temp->data;
	      ret->status = status;
	      last = ret;
	    }
	  else
	    {
	      n = (hashtable_entry_t *)malloc(sizeof(hashtable_entry_t));
	      n->next = NULL;
	      n->data = temp->data;
	      n->status = status;
	      last->next = n;
	      last = n;
	    }
	}
    }
  
  return ret;
}
