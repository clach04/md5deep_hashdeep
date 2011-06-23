
/* MD5DEEP - hashTable.c
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

// $Id$

#include "main.h"
#include "md5deep.h"
#include "md5deep_hashtable.h"


/* These two functions are the "hash" functions for the hash table. 
   Because the original implementation of this code was for storing
   md5 hashes, I used the name "translate" to avoid confusion. */


/* Convert a single hexadecimal character to decimal. If c is not a valid
   hex character, returns 0. */
int translateChar(char c) 
{
  // If this is a digit
  if (c > 47 && c < 58) 
    return (c - 48);
  
  c = toupper(c);
  // If this is a letter... 'A' should be equal to 10
  if (c > 64 && c < 71) 
    return (c - 55);

  return 0;
}
    
/* Translates a hex value into it's appropriate index in the array.
   In reality, this just turns the first HASH_SIG_FIGS into decimal */
uint64_t translate(const char *n) 
{ 
  int count;
  uint64_t total = 0, power = 1;
  for (count = HASH_SIG_FIGS - 1 ; count >= 0 ; count--) {
    total += translateChar(n[count]) * power;
    power *= 16;
  }

  return total;
}


/* ---------------------------------------------------------------------- */


void hashTableInit(hashTable *knownHashes) 
{
  uint64_t count;
  for (count = 0 ; count < HASH_TABLE_SIZE ; ++count) 
    (*knownHashes)[count] = NULL;
}

int initialize_node(hashNode *newNode, const char *nfn, const char *fn)
{
  newNode->been_matched = FALSE;
  newNode->data         = strdup(nfn);
  newNode->filename     = strdup(fn);
  newNode->next         = NULL;

  if (newNode->data == NULL || newNode->filename == NULL)
    return TRUE;
  return FALSE;
}


int hashTableAdd(state *s, hashTable *knownHashes, const char *nfn, const char *fn) 
{
    assert(0);
#if 0
  uint64_t key = translate(nfn);
  hashNode *newNode, *temp;

  if (algorithm_t::valid_hash(s,nfn)) return HASHTABLE_INVALID_HASH;

  if ((*knownHashes)[key] == NULL) 
  {

    if ((newNode = (hashNode*)malloc(sizeof(hashNode))) == NULL)
      return HASHTABLE_OUT_OF_MEMORY;

    if (initialize_node(newNode,nfn,fn))
      return HASHTABLE_OUT_OF_MEMORY;

    (*knownHashes)[key] = newNode;
    return HASHTABLE_OK;
  }

  temp = (*knownHashes)[key];

  // If this value is already in the table, we don't need to add it again
  if (STRINGS_CASE_EQUAL(temp->data,nfn))
    return HASHTABLE_OK;;
  
  while (temp->next != NULL)
  {
    temp = temp->next;
    
    if (STRINGS_CASE_EQUAL(temp->data,nfn))
      return HASHTABLE_OK;
  }
  
  if ((newNode = (hashNode*)malloc(sizeof(hashNode))) == NULL)
    return HASHTABLE_OUT_OF_MEMORY;

  if (initialize_node(newNode,nfn,fn))
    return HASHTABLE_OUT_OF_MEMORY;

  temp->next = newNode;
#endif
  return FALSE;
}


/**
 * @param knownHashes - hash table of the hashes.
 * @param n - the hash being probed.
 * @param *known  - set to be the filename if found
 */
int hashTableContains(hashTable *knownHashes, const char *n, std::string *known) 
{
  uint64_t key = translate(n);
  hashNode *temp;

  if ((*knownHashes)[key] == NULL)
    return FALSE;

  /* Just because we matched keys doesn't mean we've found a hit yet.
     We still have to verify that we've found the real key. */
  temp = (*knownHashes)[key];

  do {
    if (STRINGS_CASE_EQUAL(temp->data,n))    {
	temp->been_matched = TRUE;
	if (known) (*known) = temp->filename;
	return TRUE;
    }

    temp = temp->next;
  }  while (temp != NULL);

  return FALSE;
}


#if 0
int hashTableDisplayNotMatched(hashTable *t, int display)
{
  int status = FALSE;
  uint64_t key;
  hashNode *temp;

  for (key = 0; key < HASH_TABLE_SIZE ; ++key)
  {
    if ((*t)[key] == NULL)
      continue;

    temp = (*t)[key];
    while (temp != NULL)
    {
      if (!(temp->been_matched))
      {
	if (!display)
	  return TRUE;

	// The 'return' above allows us to disregard the if statement.
	status = TRUE;
	
	printf ("%s%s", temp->filename, NEWLINE);
      }
      temp = temp->next;
    }
  } 

  return status;
}  
#endif


