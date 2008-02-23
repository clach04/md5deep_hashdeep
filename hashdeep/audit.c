
/* $Id: audit.c,v 1.5 2007/12/27 20:52:03 jessekornblum Exp $ */

#include "main.h"

void setup_audit(state *s)
{
  s->match_exact   = 0;
  s->match_partial = 0;
  s->match_moved   = 0;
  s->match_unused  = s->next_known_id;
  s->match_unknown = 0;
  s->match_total   = 0;
}


int audit_status(state *s)
{
  return (0 == s->match_unused  && 
	  0 == s->match_unknown && 
	  0 == s->match_moved);
}


int display_audit_results(state *s)
{
  if (NULL == s)
    return TRUE;

  int status = FALSE;
  
  if (!audit_status(s))
    {
      print_status("%s: Audit failed", __progname);
      status = TRUE;
    }
  else
    print_status("%s: Audit passed", __progname);
  
  if (s->mode & mode_verbose || status)
    {
      print_status("          Files matched: %"PRIu64, s->match_exact);
      print_status("Files paritally matched: %"PRIu64, s->match_partial);
      print_status("            Files moved: %"PRIu64, s->match_moved);
      print_status("  Known files not found: %"PRIu64, s->match_unused);
      print_status("        New files found: %"PRIu64, s->match_unknown);
      print_status("   Total files examined: %"PRIu64, s->match_total);
    }
  
  return status;
}


int audit_update(state *s)
{
  if (NULL == s)
    return TRUE;

  status_t st = is_known_file(s);
  
  switch (st)
    {
    case status_match:
      s->match_exact++;
      break;

    case status_no_match:
      if (s->mode & mode_more_verbose)
	{
	  display_filename(stdout,s->full_name);
	  print_status(": did not match");
	}

      s->match_unknown++;
      break;

    case status_file_name_mismatch:
      /* Implies all hashes and file size match */   
      if (s->mode & mode_more_verbose)
	{
	  print_status("%s: moved to ", s->current_file->file_name);
	  display_filename(stdout,s->full_name);	  
	}
      s->match_moved++;
      break;

      /* RBF - Display a message here? */
    case status_partial_match:
      s->match_partial++;
      break;

      /* RBF - Display a message here? */
      /* RBF - What do we do here? */
    case status_file_size_mismatch:
      /* Implies all hashes match */

    default:
      return TRUE;

    }

  return FALSE;
}
