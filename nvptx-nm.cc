/* List symbols.
   Copyright (C) 1991-2022 Free Software Foundation, Inc.
   Copyright (C) 2014, 2015 Mentor Graphics
   Copyright (C) 2022, 2023 Siemens
   Copyright (C) 2024 BayLibre

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 3, or (at
   your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with nvptx-tools; see the file COPYING3.  If not see
   <http://www.gnu.org/licenses/>.  */

/* This is a somewhat simplistic implementation, loosely similar in behavior to
   GNU Binutils 'nm'; primarily just satisfying needs of GCC 'collect2'.  */

/* This file started as a copy of 'nvptx-ld.cc' (and, we're keeping the overall
   structure of that file), with sprinkles of GNU Binutils 'binutils/nm.c' at
   commit 370426d0da768345fb53683c803d6d5a20558065 (to facilitate compatibility
   with that one, as far as implemented), where that codes is modified only as
   much as is necessary.  That does bring in a bit of "bulkiness" that's not
   actually technically necessary here, and "odd" terminology, but this should
   simplify future maintenance and porting of additional features.  This file
   therefore is a somewhat chaotic mix of different programming styles.  */

/* Define this so that inttypes.h defines the PRI?64 macros even
   when compiling with a C++ compiler.  Define it here so in the
   event inttypes.h gets pulled in by another header it is already
   defined.  */
#define __STDC_FORMAT_MACROS

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <getopt.h>
#include <assert.h>
#include <locale.h>

#include "hashtab.h"
#define HAVE_DECL_BASENAME 1
#include "libiberty.h"

#include <list>
#include <string>
#include <iostream>

#include "version.h"

ATTRIBUTE_NORETURN static void usage (std::ostream &, int);

static const char *print_format_string = NULL;

static int no_sort = 0;		/* Don't sort; print syms in order found.  */
static int reverse_sort = 0;	/* Sort in downward(alpha or numeric) order.  */
static int sort_numerically = 0;/* Sort in numeric rather than alpha order.  */
static int quiet = 0;		/* Suppress "no symbols" diagnostic.  */

static int filename_per_file = 0;	/* Once per file, on its own line.  */

static int print_width = 0;
static int print_radix = 16;

struct symbol_hash_entry
{
  /* The name of the symbol.  */
  const char *key;
  char type;
};

static void
symbol_hash_free (void *elt)
{
  symbol_hash_entry *e = (symbol_hash_entry *) elt;
  free ((void *) e->key);
  free (e);
}

/* Hash and comparison functions for these hash tables.  */

static int hash_string_eq (const void *, const void *);
static hashval_t hash_string_hash (const void *);

static int
hash_string_eq (const void *e1_p, const void *s2_p)
{
  const symbol_hash_entry *she1_p = (const symbol_hash_entry *) e1_p;
  const char *s2 = (const char *) s2_p;
  return strcmp (she1_p->key, s2) == 0;
}

static hashval_t
hash_string_hash (const void *e_p)
{
  const symbol_hash_entry *she_p = (const symbol_hash_entry *) e_p;
  return (*htab_hash_string) (she_p->key);
}

/* Look up an entry in the symbol hash table.

   Takes ownership of STRING.  */

static struct symbol_hash_entry *
symbol_hash_lookup (htab_t symbol_table, char *string, int create)
{
  void **e;
  e = htab_find_slot_with_hash (symbol_table, string,
                                (*htab_hash_string) (string),
                                create ? INSERT : NO_INSERT);
  if (e == NULL)
    {
      free (string);
      return NULL;
    }
  if (*e == NULL)
    {
      struct symbol_hash_entry *v;
      *e = v = XCNEW (struct symbol_hash_entry);
      v->key = string;
    }
  else
    free (string);
  return (struct symbol_hash_entry *) *e;
}

/* Process GCC/nvptx-generated linker markers.

   Capture in 'symbol_table_global'/'symbol_table_local' the type of symbols in
   'ptx'.  */

static const char *
process_refs_defs (htab_t symbol_table_global, htab_t symbol_table_local, const char *ptx)
{
  /* There is no actual symbol table in GCC/nvptx.  We only look for the
     GCC/nvptx-generated linker markers, not for actual PTX-level symbols.
     (We do look for PTX '.weak', though.)
     We therefore only present the information conveyed by those markers.  */
  while (*ptx != '\0')
    {
      const char *ptx_begin = ptx;
      bool global_p;
      if ((global_p = (strncmp (ptx, "\n// BEGIN GLOBAL ", 17) == 0))
	  || (strncmp (ptx, "\n// BEGIN ", 10) == 0))
	{
	  if (global_p)
	    ptx += 17;
	  else
	    ptx += 10;
	  char type;
	  if (strncmp (ptx, "VAR DEF: ", 9) == 0)
	    {
	      type = global_p ? 'D' : 'd';
	      ptx += 9;
	    }
	  else if (strncmp (ptx, "FUNCTION DEF: ", 14) == 0)
	    {
	      type = global_p ? 'T' : 't';
	      ptx += 14;
	    }
	  else if (strncmp (ptx, "VAR DECL: ", 10) == 0)
	    {
	      type = 'U';
	      ptx += 10;
	    }
	  else if (strncmp (ptx, "FUNCTION DECL: ", 15) == 0)
	    {
	      type = 'U';
	      ptx += 15;
	    }
	  else
	    /* Unknown marker: ignore, and hope for the best.  */
	    type = '\0';

	  const char *ptx_sym_name_begin = ptx;
	  const char *ptx_lf = strchr (ptx, '\n');
	  if (!ptx_lf)
	    {
	      assert (ptx_begin[0] == '\n');
	      std::cerr << "error, truncated marker line:" << ptx_begin << "\n";
	      return NULL;
	    }
	  const char *ptx_sym_name_end = ptx_lf;
	  ptx = ptx_lf;

	  if (type == '\0')
	    continue;

	  /* GCC/nvptx currently emits the PTX '.weak' linking directive
	     without special annotation in the corresponding marker line.
	     Per 'as', '.weak ' follows directly after the marker line.  */
	  bool ptx_weak_p = strncmp (ptx, "\n.weak ", 7) == 0;
	  if (ptx_weak_p)
	    ptx += 7;

	  char *sym_name = xstrndup (ptx_sym_name_begin, ptx_sym_name_end - ptx_sym_name_begin);

	  if (ptx_weak_p)
	    {
	      if (!global_p)
		{
		  const char *type_s;
		  if (type == 'd')
		    type_s = "VAR DEF";
		  else if (type == 't')
		    type_s = "FUNCTION DEF";
		  else if (type == 'U')
		    type_s = "DECL";
		  else
		    abort ();
		  std::cerr << "error, non-GLOBAL " << type_s << " is \"weak\": '" << sym_name << "'\n";

		  free (sym_name);

		  return NULL;
		}
	      else if (type == 'D')
		type = 'V';
	      else if (type == 'T')
		type = 'W';
	      else if (type == 'U')
		type = 'w';
	      else
		abort ();
	    }

	  htab_t symbol_table
	    = global_p ? symbol_table_global : symbol_table_local;
	  struct symbol_hash_entry *e
	    = symbol_hash_lookup (symbol_table, sym_name, 1);

	  if (e->type == '\0')
	    {
	      /* First time we're seeing this.  */
	      e->type = type;
	    }
	  else if (e->type == 'U'
		   || e->type == 'w')
	    {
	      /* We've seen this before, but not a definition.  */
	      e->type = type;
	    }
	  else if (type == 'U'
		   || type == 'w')
	    {
	      /* We've already seen a definition.  */
	    }
	  else if ((type == 'V' && e->type == 'D')
		   || (type == 'W' && e->type == 'T'))
	    {
	      /* This "weak" definition doesn't affect the former "strong" one.  */
	    }
	  else if ((type == 'D' && e->type == 'V')
		   || (type == 'T' && e->type == 'W'))
	    {
	      /* This "strong" definition overrides the former "weak" one.  */
	      e->type = type;
	    }
	  else
	    {
	      /* We've already seen a definition; verify it's the same now.  */
	      assert (e->type == type);
	    }

	  continue;
	}

      ptx++;
    }
  /* Callers may use this return value to detect NUL-separated parts.  */
  return ptx + 1;
}

static int
symbol_table_into_array (void **slot, void *info)
{
  symbol_hash_entry *e = *(symbol_hash_entry **) slot;
  symbol_hash_entry ***arrayp = (symbol_hash_entry ***) info;

  /* Put 'symbol_hash_entry *' into array slot.  */
  **arrayp = e;
  /* ..., and prepare next iteration.  */
  ++*arrayp;

  return 1;
}

static void
set_output_format (const char *f)
{
  switch (*f)
    {
    case 'b':
    case 'B':
      /* No-op.  */
      break;
    default:
      std::cerr << f << ": invalid output format\n";
      usage (std::cerr, 1);
    }
}

/* Symbol-sorting predicates */

/* Numeric sorts.  Undefined symbols are always considered "less than"
   defined symbols with zero values.  */

static int
non_numeric_forward (const void *P_x, const void *P_y)
{
  const symbol_hash_entry *xe = *(const symbol_hash_entry **) P_x;
  const symbol_hash_entry *ye = *(const symbol_hash_entry **) P_y;

  const char *xn = xe->key;
  const char *yn = ye->key;

#if 0
  if (yn == NULL)
    return xn != NULL;
  if (xn == NULL)
    return -1;
#else
  assert (xn != NULL
	  && yn != NULL);
#endif

#if 0
  /* Solaris 2.5 has a bug in strcoll.
     strcoll returns invalid values when confronted with empty strings.  */
  if (*yn == '\0')
    return *xn != '\0';
  if (*xn == '\0')
    return -1;
#else
  assert (*xn != '\0'
	  && *yn != '\0');
#endif

  return strcoll (xn, yn);
}

static int
non_numeric_reverse (const void *x, const void *y)
{
  return - non_numeric_forward (x, y);
}

static int
numeric_forward (const void *P_x, const void *P_y)
{
  const symbol_hash_entry *xe = *(const symbol_hash_entry **) P_x;
  const symbol_hash_entry *ye = *(const symbol_hash_entry **) P_y;

  if (xe->type == 'U'
      || xe->type == 'w')
    {
      if (!(ye->type == 'U'
	    || ye->type == 'w'))
	return -1;
    }
  else if (ye->type == 'U'
	   || ye->type == 'w')
    return 1;
#if 0
  /* There are no symbol values in GCC/nvptx.  */
  else if (valueof (x) != valueof (y))
    return valueof (x) < valueof (y) ? -1 : 1;
#endif

  return non_numeric_forward (P_x, P_y);
}

static int
numeric_reverse (const void *x, const void *y)
{
  return - numeric_forward (x, y);
}

static int (*sorters[2][2]) (const void *, const void *) =
{
  { non_numeric_forward, non_numeric_reverse },
  { numeric_forward, numeric_reverse }
};

/* Print a symbol value.  */

static void
print_value (void *val)
{
  switch (print_width)
    {
    case 32:
    case 64:
      printf (print_format_string, (uint64_t) val);
      break;

    default:
      abort ();
      break;
    }
}

/* Print a line of information about a symbol.  */

static void
print_symbol_info_bsd (symbol_hash_entry *sym)
{
  if (sym->type == 'U'
      || sym->type == 'w')
    {
      if (print_width == 64)
	printf ("        ");
      printf ("        ");
    }
  else
    {
      /* There are no symbol values in GCC/nvptx; print a dummy.  */
      print_value (0);
    }

  printf (" %c", sym->type);

  printf (" %s", sym->key);
}

/* Print a single symbol.  */

static void
print_symbol (symbol_hash_entry *sym)
{
#if 0
  print_symbol_filename ();
#endif

  print_symbol_info_bsd (sym);

  putchar ('\n');
}

/* Print the symbols that are held in MINISYMS.
   SYMCOUNT is the number of symbols in MINISYMS.  */

static void
print_symbols (symbol_hash_entry **minisyms,
	       long symcount)
{
  for (symbol_hash_entry **minisym = minisyms;
       minisym != minisyms + symcount;
       ++minisym)
    {
      symbol_hash_entry *sym = *minisym;
      print_symbol (sym);
    }
}

static void
display_rel_file (std::list<htab_t> &symbol_tables, const std::string &/*name*/)
{
  size_t symcount = 0;
  for (std::list<htab_t>::iterator it = symbol_tables.begin ();
       it != symbol_tables.end();
       ++it)
    symcount += htab_elements (*it);

#if 0
  /* The following conditional 'symcount == 0' is not correct.  For example,
     for an "empty" object file, GNU Binutils 'nm' would usually still see
     "debugger-only symbols" symbols (not printed without '--debug-syms'); it
     runs into 'symcount == 0' if the object file doesn't have a symbol table,
     because it got 'strip'ped, for example.  This concept doesn't apply here;
     there is no actual symbol table in GCC/nvptx, and therefore, we simply
     never print this message.  */
  if (symcount == 0)
    {
      if (!quiet)
	std::cerr << name << ": no symbols\n";
      return;
    }
#endif

  symbol_hash_entry **minisyms = XNEWVEC (symbol_hash_entry *, symcount);
  {
    symbol_hash_entry **minisym = minisyms;
    for (std::list<htab_t>::iterator it = symbol_tables.begin ();
	 it != symbol_tables.end();
	 ++it)
      htab_traverse_noresize (*it, symbol_table_into_array, &minisym);
    assert (minisym == minisyms + symcount);
  }

  if (! no_sort)
    {
      qsort (minisyms, symcount, sizeof *minisyms,
	     sorters[sort_numerically][reverse_sort]);
    }

  print_symbols (minisyms, symcount);

  XDELETEVEC (minisyms);
}

/* Construct a formatting string for printing symbol values.  */

static const char *
get_print_format (void)
{
  const char * padding;
  if (print_width == 32)
    {
      padding ="08";
    }
  else /* print_width == 64 */
    {
      padding = "016";
    }

  const char * radix = NULL;
  switch (print_radix)
    {
    case 8:  radix = PRIo64; break;
    case 10: radix = PRId64; break;
    case 16: radix = PRIx64; break;
    }

  return concat ("%", padding, radix, NULL);
}

static void
set_print_width (const char *buf)
{
  /* We'd find it at the beginning of 'buf', but: GCC/nvptx always is
     '.address_size 64'.  */
  (void) buf;
  print_width = 64;

  free ((char *) print_format_string);
  print_format_string = get_print_format ();
}

/* Print the name of an object file given on the command line.  */

static void
print_object_filename_bsd (const char *filename)
{
  if (filename_per_file /* && !filename_per_symbol */)
    printf ("\n%s:\n", filename);
}

ATTRIBUTE_NORETURN static void
usage (std::ostream &out_stream, int status)
{
  out_stream << "\
Usage: nvptx-none-nm [option...] [files...]\n\
Options:\n\
  -B                    Same as --format=bsd\n\
  -f, --format=FORMAT   Use the output format FORMAT.  FORMAT can be `bsd'.\n\
  -n, --numeric-sort    Sort symbols numerically by address\n\
  -p, --no-sort         Do not sort the symbols\n\
  -r, --reverse-sort    Reverse the sense of the sort\n\
  --quiet               Suppress \"no symbols\" diagnostic\n\
  --help                Print this help and exit\n\
  --version             Print version number and exit\n\
\n\
If no files are given, `a.out' is defaulted.\n\
\n\
Report bugs to " << REPORT_BUGS_TO << ".\n";
  exit (status);
}

enum long_option_values
{
  OPTION_QUIET = 207
};

static const struct option long_options[] = {
  {"format", required_argument, 0, 'f'},
  {"no-sort", no_argument, 0, 'p'},
  {"numeric-sort", no_argument, 0, 'n'},
  {"quiet", no_argument, 0, OPTION_QUIET},
  {"reverse-sort", no_argument, &reverse_sort, 1},
  {"help", no_argument, 0, 'h' },
  {"version", no_argument, 0, 'V' },
  {0, 0, 0, 0 }
};

int
main (int argc, char **argv)
{
  setlocale (LC_COLLATE, "");

  int o;
  int option_index = 0;
  while ((o = getopt_long (argc, argv, "Bf:npr", long_options, &option_index)) != -1)
    {
      switch (o)
	{
	case 'B':
	  set_output_format ("bsd");
	  break;
	case OPTION_QUIET:
	  quiet = 1;
	  break;
	case 'f':
	  set_output_format (optarg);
	  break;
	case 'n':
	  no_sort = 0;
	  sort_numerically = 1;
	  break;
	case 'p':
	  no_sort = 1;
	  sort_numerically = 0;
	  break;
	case 'r':
	  reverse_sort = 1;
	  break;
	case 'h':
	  usage (std::cout, 0);
	  break;
	case 'V':
	  std::cout << "\
nvptx-none-nm " << PKGVERSION << NVPTX_TOOLS_VERSION << "\n\
Copyright (C) 2024 The nvptx-tools Developers\n\
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>\n\
This program is free software; you may redistribute it under the terms of\n\
the GNU General Public License version 3 or later.\n\
This program has absolutely no warranty.\n";
	  exit (0);
	case 0:		/* A long option that just sets a flag.  */
	  break;
	default:
	  usage (std::cerr, 1);
	  break;
	}
    }

  std::list<std::string> inputfiles;
  while (optind < argc)
    inputfiles.push_back (argv[optind++]);
  if (!inputfiles.size ())
    inputfiles.push_back ("a.out");
  if (inputfiles.size () > 1)
    filename_per_file = 1;

  /* Scan 'inputfiles'.  */
  for (std::list<std::string>::const_iterator iterator = inputfiles.begin(), end = inputfiles.end();
       iterator != end;
       ++iterator)
    {
      const std::string &name = *iterator;
      FILE *f = fopen (name.c_str (), "r");
      if (f == NULL)
	{
	  std::cerr << "error opening " << name << "\n";
	  goto error_out;
	}
      fseek (f, 0, SEEK_END);
      off_t len = ftell (f);
      fseek (f, 0, SEEK_SET);
      char *buf = new char[len + 1];
      size_t read_len = fread (buf, 1, len, f);
      buf[len] = '\0';
      if (read_len != static_cast<size_t>(len) || ferror (f))
	{
	  std::cerr << "error reading " << name << "\n";
	  fclose (f);
	  goto error_out;
	}
      fclose (f);
      f = NULL;

      /* Applies to the whole file.  */
      set_print_width (buf);

      std::list<htab_t> symbol_tables;
      /* Initial symbol table size.  */
      const size_t n_symbols_init = 500;
      htab_t symbol_table_global = htab_create (n_symbols_init, hash_string_hash, hash_string_eq, symbol_hash_free);
      symbol_tables.push_back (symbol_table_global);
      const char *buf_ = buf;
      /* Per nvptx-tools 'ld', there are several NUL-separated parts.  These we
	 have to process unified for global symbols, but individually for local
	 symbols.  */
      do
	{
	  htab_t symbol_table_local = htab_create (n_symbols_init, hash_string_hash, hash_string_eq, symbol_hash_free);
	  symbol_tables.push_back (symbol_table_local);
	  buf_ = process_refs_defs (symbol_table_global, symbol_table_local, buf_);
	  if (buf_ == NULL)
	    {
	      std::cerr << "while scanning '" << name << "'\n";
	      break;
	    }
	}
      while (buf_ != &buf[len + 1]);
      delete[] buf;
      if (buf_ != NULL)
	{
	  print_object_filename_bsd (name.c_str ());
	  display_rel_file (symbol_tables, name);
	}
      while (!symbol_tables.empty ())
	{
	  htab_t symbol_table = symbol_tables.front ();
	  htab_delete (symbol_table);
	  symbol_tables.pop_front ();
	}
      if (buf_ == NULL)
	goto error_out;
    }

  free ((char *) print_format_string);

  return 0;

 error_out:

  free ((char *) print_format_string);

  return 1;
}
