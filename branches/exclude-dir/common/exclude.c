
// MD5DEEP
//
// By Jesse Kornblum
//
// This is a work of the US Government. In accordance with 17 USC 105,
// copyright protection is not available for any work of the US Government.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
// $Id$

#include "main.h"


typedef struct exclude_table {
  TCHAR *name;
  struct exclude_table *next;
} exclude_table;

exclude_table *my_exclude = NULL;


// Debugging code
/*
static void dump_exclude(void)
{
  struct exclude_table *t = my_exclude;
  printf ("\nTable contents:\n");
  while (t != NULL)
{
    display_filename(stdout,t->name);
    printf ("\n");
    t = t->next;
  }
  print_status ("-- end of table --\n");
}
*/


void add_exclude_dir(state *s, char *fn)
{
  if (NULL == fn)
    return;

  exclude_table *new, *temp;
  TCHAR *d_name = (TCHAR *)malloc(sizeof(TCHAR) * PATH_MAX);

#ifdef _WIN32
  int t_size = MultiByteToWideChar(CP_ACP,0,fn,lstrlenA(fn),0,0);
  TCHAR *t_name = (TCHAR *)malloc(sizeof(TCHAR) * (t_size + 1));
  MultiByteToWideChar(CP_ACP,0,fn,lstrlenA(fn),t_name,t_size);
  t_name[t_size] = 0;
  _wfullpath(d_name,t_name,PATH_MAX);
  free(t_name);
#else
  realpath(fn,d_name);
#endif

  if (my_exclude == NULL)
  {
    my_exclude = (exclude_table*)malloc(sizeof(exclude_table));
    my_exclude->name = _tcsdup(d_name);
    my_exclude->next = NULL;

    free(d_name);
    return;
  }

  for(temp = my_exclude; temp->next; temp = temp->next);

  new = (exclude_table*)malloc(sizeof(exclude_table));
  new->name = _tcsdup(d_name);
  new->next = NULL;  
  temp->next = new;

  free(d_name);
  return;
}


int exclude_dir(TCHAR *fn)
{
  if (NULL == fn)
    return TRUE;

  exclude_table *temp;
  TCHAR *d_name;

  if (my_exclude == NULL)
    return FALSE;

  d_name = (TCHAR *)malloc(sizeof(TCHAR) * PATH_MAX);
#ifdef _WIN32
  _wfullpath(d_name,fn,PATH_MAX);
#else
  realpath(fn,d_name);
#endif

  temp = my_exclude;
  while (temp != NULL)
  {
    if (!_tcsncmp(temp->name,d_name,PATH_MAX))
    {
      free(d_name);
      return TRUE;
    }

    temp = temp->next;       
  }

  free(d_name);
  return FALSE;
}

