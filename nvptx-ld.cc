/* A "linker" for PTX.
   Copyright (C) 2014, 2015 Mentor Graphics
   Contributed by Bernd Schmidt <bernds@codesourcery.com>

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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <assert.h>

#include "hashtab.h"
#define HAVE_DECL_BASENAME 1
#include "libiberty.h"

#include <list>
#include <string>
#include <iostream>
#include <vector>
#include <utility>
#include <algorithm>

#include "version.h"

static bool verbose = false;

struct file_hash_entry;

struct symbol_hash_entry
{
  /* The name of the symbol.  */
  const char *key;
  /* A linked list of unresolved referenced symbols.  */
  struct symbol_hash_entry **pprev, *next;
  /* The file in which it is defined.  */
  struct file_hash_entry *def;
  int included;
  int referenced;
};

static void
symbol_hash_free (void *elt)
{
  symbol_hash_entry *e = (symbol_hash_entry *) elt;
  free ((void *) e->key);
  free (e);
}

struct file_hash_entry
{
  struct file_hash_entry **pprev, *next;
  const char *name;
  const char *arname;
  const char *data;
  size_t len;
};

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

/* Takes ownership of DATA.  */

static struct file_hash_entry *
file_hash_new (const char *data, size_t len, const char *arname, const char *name)
{
  struct file_hash_entry *v = XCNEW (struct file_hash_entry);
  v->data = data;
  v->len = len;
  v->name = xstrdup (name);
  v->arname = xstrdup (arname);
  return v;
}

static void
file_hash_free (struct file_hash_entry *v)
{
  free ((void *) v->data);
  free ((void *) v->name);
  free ((void *) v->arname);
  free (v);
}

#define ARMAG  "!<arch>\012"    /* For COFF and a.out archives.  */
#define SARMAG 8
#define ARFMAG "`\012"

struct ar_hdr
{
  char ar_name[16];             /* Name of this member.  */
  char ar_date[12];             /* File mtime.  */
  char ar_uid[6];               /* Owner uid; printed as decimal.  */
  char ar_gid[6];               /* Owner gid; printed as decimal.  */
  char ar_mode[8];              /* File mode, printed as octal.   */
  char ar_size[10];             /* File size, printed as decimal.  */
  char ar_fmag[2];              /* Should contain ARFMAG.  */
};

class archive
{
  FILE *f;
  off_t flen;
  off_t off;

  char name[17];
  char *contents;
  size_t len;

 public:
  archive () : f (NULL), contents (NULL) { }
  ~archive ()
  {
    discard_contents ();
  }
  void discard_contents ()
  {
    if (contents)
      delete[] contents;
    contents = NULL;
  }

  static bool is_archive (FILE *file)
  {
    char magic[SARMAG];
    if (fread (magic, 1, SARMAG, file) != SARMAG)
      return false;
    if (memcmp (magic, ARMAG, SARMAG) != 0)
      return false;
    return true;
  }

  bool init (FILE *file)
  {
    if (!is_archive (file))
      return false;

    f = file;
    fseek (f, 0, SEEK_END);
    flen = ftell (f);
    fseek (f, SARMAG, SEEK_SET);
    off = SARMAG;

    if (at_end ())
      /* Empty archive; valid.  */
      return true;

    struct ar_hdr hdr;
    if (fread (&hdr, sizeof hdr, 1, f) != 1)
      return false;
    if (hdr.ar_name[0] == '/' || hdr.ar_name[0] == ' ')
      {
	off += sizeof hdr;
	long l = atol (hdr.ar_size);
	if (l < 0 || off + l > flen)
	  return false;
	off += l;
      }

    fseek (f, off, SEEK_SET);
    return true;
  }

  bool at_end ()
  {
    return off == flen;
  }

  bool next_file ()
  {
    discard_contents ();

    struct ar_hdr hdr;
    if (fread (&hdr, sizeof hdr, 1, f) != 1)
      return false;
    off += sizeof hdr;
    long l = atol (hdr.ar_size);
    if (l <= 0 || l > flen)
      return false;
    /* <https://pubs.opengroup.org/onlinepubs/9699919799/utilities/ar.html>:
       "Objects in the archive are always an even number of bytes long; files
       that are an odd number of bytes long are padded with a <newline>,
       although the size in the header does not reflect this."  */
    size_t read_len = l + (l & 1);
    len = l;
    contents = new char[read_len];
    if (contents == NULL)
      return false;
    if (fread (contents, 1, read_len, f) != read_len)
      return false;
    off += read_len;
    memcpy (name, hdr.ar_name, sizeof hdr.ar_name);
    name[16] = '\0';
    return true;
  }
  const char *get_contents () { return contents; }
  const char *get_name () { return name; }
  size_t get_len () { return len; }
};

static std::string
path_resolve (const std::string &filename, const std::list<std::string> &paths)
{
  for (std::list<std::string>::const_iterator iterator = paths.begin(), end = paths.end();
       iterator != end;
       ++iterator)
    {
      std::string tmp = *iterator;
      tmp += '/';
      tmp += filename;
      if (access (tmp.c_str (), F_OK) == 0)
	return tmp;
    }
  return "";
}

/* Global constructor/destructor support */

/* See GCC 'gcc/tree.cc:get_file_function_name'.  */
static std::list<const char *> special_purpose_functions;

class cdtor
{
public:
  const char *name;
  size_t sequence;

  int priority;

  cdtor () = delete;
  cdtor (const char *name, size_t sequence)
    : name (name), sequence (sequence)
  {
    /* Parse '_GLOBAL__I_00500_0_c1' -> '500', for example.  */
    const char *priority_str = name + strlen ("_GLOBAL__ _");
    char *priority_str_end;
    errno = 0;
    priority = strtol (priority_str, &priority_str_end, 10);
    if (errno != 0
	|| priority_str_end != name + strlen ("_GLOBAL__ _     "))
      priority = -1;
  }
  ~cdtor ()
  {
  }

  bool validate () const
  {
    if (priority < 0)
      {
	std::cerr << "unable to extract priority from special-purpose function '" << name << "'\n";
	return false;
      }
    return true;
  }

  /* Note that this sorts in the reverse direction -- as it's usually done for
     '__CTOR_LIST__', '__DTOR_LIST__'.  */
  static class
  {
  public:
    bool operator() (const cdtor &a, const cdtor &b) const
    {
      if (a.priority < b.priority)
	return false;
      else if (a.priority == b.priority
	       && a.sequence < b.sequence)
	return false;
      else
	return true;
    }
  } sorter;
};

static int
handle_special_purpose_functions (htab_t symbol_table, FILE *outfile)
{
  if (special_purpose_functions.empty ())
    return 0;

  int ret = 1;

  std::vector<cdtor> ctors;
  std::vector<cdtor> dtors;
  for (std::list<const char *>::iterator iterator = special_purpose_functions.begin(),
	 end = special_purpose_functions.end();
       iterator != end;
       ++iterator)
    {
      const char *name = *iterator;
      if (strncmp (name, "_GLOBAL__I_", 11) == 0)
	{
	  static size_t sequence = 0;
	  cdtor ctor (name, sequence++);
	  ctors.push_back (ctor);
	}
      else if (strncmp (name, "_GLOBAL__D_", 11) == 0)
	{
	  static size_t sequence = 0;
	  cdtor dtor (name, sequence++);
	  dtors.push_back (dtor);
	}
      else
	{
	  std::cerr << "unexpected special-purpose function name: '" << name << "'\n";
	  goto error_out;
	}
    }

  fprintf (outfile,
	   /* We only have to define two global variables here; GCC's
	      historically lowest '.version' and '.target' should do.  */
	   ".version 3.1\n"
	   ".target sm_30\n"
	   ".address_size 64\n");

  fprintf (outfile,
	   "\n"
	   "// Constructors.\n"
	   "\n");

  for (const auto &ctor : ctors)
    {
      if (!ctor.validate ())
	goto error_out;
      if (verbose)
	std::cerr << "special-purpose function: constructor "
		  << "with priority: " << ctor.priority
		  << ", sequence: " << ctor.sequence
		  << ": '" << ctor.name << "'\n";
      fprintf (outfile, ".extern .func %s;\n", ctor.name);
    }

  std::sort (ctors.begin(), ctors.end(), cdtor::sorter);

  fprintf (outfile,
	   "\n"
	   ".visible .global .u64 __CTOR_LIST__[] = {\n");
  fprintf (outfile,
	   "  %zu,\n", (size_t) ctors.size ());
  for (const auto &ctor : ctors)
    {
      fprintf (outfile, "  // priority: %d, sequence: %zu\n", ctor.priority, ctor.sequence);
      fprintf (outfile, "  %s,\n", ctor.name);
    }
  fprintf (outfile,
	   "  0\n"
	   "};\n");

  {
    struct symbol_hash_entry *e
      = symbol_hash_lookup (symbol_table, xstrdup ("__CTOR_LIST__"), 1);
    e->included = true;
  }

  fprintf (outfile,
	   "\n"
	   "// Destructors.\n"
	   "\n");

  for (const auto &dtor : dtors)
    {
      if (!dtor.validate ())
	goto error_out;
      if (verbose)
	std::cerr << "special-purpose function: destructor "
		  << "with priority: " << dtor.priority
		  << ", sequence: " << dtor.sequence
		  << ": '" << dtor.name << "'\n";
      fprintf (outfile, ".extern .func %s;\n", dtor.name);
    }

  std::sort (dtors.begin(), dtors.end(), cdtor::sorter);

  fprintf (outfile,
	   "\n"
	   ".visible .global .u64 __DTOR_LIST__[] = {\n");
  fprintf (outfile,
	   "  %zu,\n", (size_t) dtors.size ());
  for (const auto &dtor : dtors)
    {
      fprintf (outfile, "  // priority: %d, sequence: %zu\n", dtor.priority, dtor.sequence);
      fprintf (outfile, "  %s,\n", dtor.name);
    }
  fprintf (outfile,
	   "  0\n"
	   "};\n");

  {
    struct symbol_hash_entry *e
      = symbol_hash_lookup (symbol_table, xstrdup ("__DTOR_LIST__"), 1);
    e->included = true;
  }

  fprintf (outfile,
	   "\n"
	   "/* For example with old Nvidia Tesla K20c, Driver Version: 361.93.02, the\n"
	   "   function pointers stored in the '__CTOR_LIST__', '__DTOR_LIST__' arrays\n"
	   "   evidently evaluate to NULL in JIT compilation.  Defining a dummy function\n"
	   "   next to the arrays apparently does work around this issue...  */\n"
	   "\n"
	   ".func dummy\n"
	   "{\n"
	   "  ret;\n"
	   "}\n");

  fputc ('\0', outfile);

 out:
  return ret;

 error_out:
  ret = -1;

  goto out;
}

static struct symbol_hash_entry *unresolved;

static void
enqueue_as_unresolved (struct symbol_hash_entry *e)
{
  e->pprev = &unresolved;
  e->next = unresolved;
  if (e->next)
    e->next->pprev = &e->next;
  unresolved = e;
  e->referenced = true;
}

static void
dequeue_unresolved (struct symbol_hash_entry *e)
{
  if (e->pprev != NULL)
    {
      if (e->next)
	e->next->pprev = e->pprev;
      *e->pprev = e->next;
    }
  e->pprev = NULL;
}

static void
define_intrinsics (htab_t symbol_table)
{
  static const char *const intrins[] =
    {"vprintf", "malloc", "free", NULL};
  unsigned ix;

  for (ix = 0; intrins[ix]; ix++)
    {
      struct symbol_hash_entry *e
	= symbol_hash_lookup (symbol_table, xstrdup (intrins[ix]), 1);
      e->included = true;
    }
}

static const char *
process_refs_defs (htab_t symbol_table, file_hash_entry *f, const char *ptx)
{
  while (*ptx != '\0')
    {
      if (strncmp (ptx, "\n// BEGIN GLOBAL ", 17) == 0)
	{
	  int type = 0;
	  ptx += 17;
	  if (strncmp (ptx, "VAR DEF: ", 9) == 0)
	    {
	      type = 1;
	      ptx += 9;
	    }
	  else if (strncmp (ptx, "FUNCTION DEF: ", 14) == 0)
	    {
	      type = 1;
	      ptx += 14;
	    }
	  if (strncmp (ptx, "VAR DECL: ", 10) == 0)
	    {
	      type = 2;
	      ptx += 10;
	    }
	  else if (strncmp (ptx, "FUNCTION DECL: ", 15) == 0)
	    {
	      type = 2;
	      ptx += 15;
	    }
	  if (type == 0)
	    continue;
	  const char *end = strchr (ptx, '\n');
	  if (end == 0)
	    end = ptx + strlen (ptx);

	  char *sym = xstrndup (ptx, end - ptx);
	  struct symbol_hash_entry *e
	    = symbol_hash_lookup (symbol_table, sym, 1);

	  if (!e->included)
	    {
	      if (type == 1)
		{
		  if (f == NULL)
		    {
		      e->included = true;
		      dequeue_unresolved (e);

		      if (strncmp (e->key, "_GLOBAL__", 9) == 0)
			/* Capture special-purpose function names already here,
			   in order of appearance, instead of later traversing
			   the whole 'symbol_table'.  */
			special_purpose_functions.push_back (e->key);
		    }
		  else
		    e->def = f;
		}
	      else
		{
		  if (f == NULL)
		    {
		      if (!e->referenced)
			enqueue_as_unresolved (e);
		    }
		}
	    }
	}
      ptx++;
    }
  /* Callers may use this return value to detect NUL-separated parts.  */
  return ptx + 1;
}

ATTRIBUTE_NORETURN static void
usage (FILE *stream, int status)
{
  fprintf (stream, "\
Usage: nvptx-none-ld [option...] [files]\n\
Options:\n\
  -o FILE               Write output to FILE\n\
  -v                    Be verbose\n\
  -l LIBRARY            Link with LIBRARY\n\
  -L DIR                Search for libraries in DIR\n\
  --help                Print this help and exit\n\
  --version             Print version number and exit\n\
\n\
Report bugs to %s.\n",
	  REPORT_BUGS_TO);
  exit (status);
}

#define OPT_hash_style 256

static const struct option long_options[] = {
  {"hash-style", required_argument, 0, OPT_hash_style },
  {"help", no_argument, 0, 'h' },
  {"version", no_argument, 0, 'V' },
  {0, 0, 0, 0 }
};

int
main (int argc, char **argv)
{
  const char *outname = NULL;
  std::list<std::string> libraries;
  std::list<std::string> libpaths;

  int o;
  int option_index = 0;
  while ((o = getopt_long (argc, argv, "L:l:o:v", long_options, &option_index)) != -1)
    {
      switch (o)
	{
	case 'v':
	  verbose = true;
	  break;
	case 'o':
	  if (outname != NULL)
	    {
	      std::cerr << "multiple output files specified\n";
	      exit (1);
	    }
	  outname = optarg;
	  break;
	case 'l':
	  libraries.push_back (optarg);
	  break;
	case 'L':
	  libpaths.push_back (optarg);
	  break;
	case 'h':
	  usage (stdout, 0);
	  break;
	case 'V':
	  printf ("\
nvptx-none-ld %s%s\n\
Copyright %s Mentor Graphics\n\
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>\n\
This program is free software; you may redistribute it under the terms of\n\
the GNU General Public License version 3 or later.\n\
This program has absolutely no warranty.\n",
		  PKGVERSION, NVPTX_TOOLS_VERSION, "2015");
	  exit (0);
	case OPT_hash_style:
	  /* Ignore '--hash-style'; see
	     <https://github.com/SourceryTools/nvptx-tools/pull/44>
	     "Handle --hash-style argument in nvptx-ld".  */
	  break;
	default:
	  usage (stderr, 1);
	  break;
	}
    }

  libpaths.unique ();

  if (outname == NULL)
    outname = "a.out";

  htab_t symbol_table
    = htab_create (500, hash_string_hash, hash_string_eq, symbol_hash_free);
  /* List of 'file_hash_entry' instances to clean up when we're done with the
     'symbol_table'.  */
  std::list<file_hash_entry *> f_to_clean_up;

  define_intrinsics (symbol_table);
  
  FILE *outfile = fopen (outname, "w");
  if (outfile == NULL)
    {
      std::cerr << "error opening output file\n";
      exit (1);
    }
  std::list<std::string> inputfiles;
  while (optind < argc)
    inputfiles.push_back (argv[optind++]);

  int idx = 0;

  for (std::list<std::string>::iterator iterator = libraries.begin(), end = libraries.end();
       iterator != end;
       ++iterator)
    {
      const std::string &name = "lib" + *iterator + ".a";
      if (verbose)
	std::cerr << "resolving lib " << name << "\n";
      const std::string &name_resolved = path_resolve (name, libpaths);
      if (name_resolved.empty ())
	{
	  std::cerr << "error resolving " << name << "\n";
	  goto error_out;
	}
      *iterator = name_resolved;
    }

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

      /* Archives appearing here are not resolved via 'libpaths'.  */
      if (archive::is_archive (f))
	{
	  /* (Pre-existing problem of) non-standard Unix 'ld' semantics; see
	     <https://github.com/SourceryTools/nvptx-tools/issues/41>
	     "ld: non-standard handling of options which refer to files".  */
	  libraries.push_back (name);

	  fclose (f);
	  f = NULL;
	  continue;
	}

      fseek (f, 0, SEEK_END);
      off_t len = ftell (f);
      fseek (f, 0, SEEK_SET);
      char *buf = new char[len + 1];
      size_t read_len = fread (buf, 1, len, f);
      buf[len] = '\0';
      if (read_len != len || ferror (f))
	{
	  std::cerr << "error reading " << name << "\n";
	  fclose (f);
	  goto error_out;
	}
      fclose (f);
      f = NULL;
      size_t out = fwrite (buf, 1, len, outfile);
      if (out != len)
	{
	  std::cerr << "error writing to output file\n";
	  goto error_out;
	}
      const char *buf_ = process_refs_defs (symbol_table, NULL, buf);
      assert (buf_ == &buf[len + 1]);
      delete[] buf;
      if (verbose)
	std::cerr << "Linking " << name << " as " << idx++ << "\n";
      fputc ('\0', outfile);
    }

  /* This de-duplication is best-effort only; it doesn't consider that the same
     file may be found via different paths.  */
  libraries.sort ();
  libraries.unique ();
  for (std::list<std::string>::const_iterator iterator = libraries.begin(), end = libraries.end();
       iterator != end;
       ++iterator)
    {
      const std::string &name = *iterator;
      if (verbose)
	std::cerr << "trying lib " << name << "\n";
      FILE *f = fopen (name.c_str (), "r");
      if (f == NULL)
	{
	  std::cerr << "error opening " << name << "\n";
	  goto error_out;
	}
      archive ar;
      if (!ar.init (f))
	{
	  std::cerr << name << " is not a valid archive\n";
	  fclose (f);
	  goto error_out;
	}
      while (!ar.at_end ())
	{
	  if (!ar.next_file ())
	    {
	      std::cerr << "error reading from archive " << name << "\n";
	      fclose (f);
	      goto error_out;
	    }

	  size_t len = ar.get_len ();
	  char *p = XNEWVEC (char, len + 1);
	  memcpy (p, ar.get_contents (), len);
	  p[len] = '\0';

	  file_hash_entry *f = file_hash_new (p, len, name.c_str (), ar.get_name ());
	  f_to_clean_up.push_front (f);
	  const char *p_ = process_refs_defs (symbol_table, f, p);
	  assert (p_ == &p[len + 1]);
	}
      fclose (f);
    }

  /* Resolve.  */
  {
  bool first_resolve_run = true;
 resolve:
  if (verbose)
    std::cerr << "Starting "
	      << (first_resolve_run ? "first" : "second")
	      << " resolve run\n";
  while (unresolved)
    {
      struct file_hash_entry *to_add = NULL;
      struct symbol_hash_entry *e;
      for (e = unresolved; e; e = e->next)
	{
	  struct file_hash_entry *f = e->def;
	  if (!f)
	    {
	      std::cerr << "unresolved symbol " << e->key << "\n";
	      goto error_out;
	    }
	  if (verbose)
	    std::cerr << "Resolving " << e->key << "\n";
	  if (!f->pprev)
	    {
	      f->pprev = &to_add;
	      f->next = to_add;
	      to_add = f;
	    }
	  e->included = true;
	  e->pprev = NULL;
	}
      unresolved = NULL;
      assert (to_add != NULL);
      struct file_hash_entry *f;
      for (f = to_add; f; f = f->next)
	{
	  f->pprev = NULL;
	  if (verbose)
	    std::cerr << "Linking " << f->arname << "::" << f->name << " as " << idx++ << "\n";
	  if (fwrite (f->data, 1, f->len, outfile) != f->len)
	    {
	      std::cerr << "error writing to output file\n";
	      goto error_out;
	    }
	  fputc ('\0', outfile);
	  const char *f_data_ = process_refs_defs (symbol_table, NULL, f->data);
	  assert (f_data_ == &f->data[f->len + 1]);
	}
    }

  /* Global constructor/destructor support.  */
  {
    /* See GCC 'libgcc/config/nvptx/gbl-ctors.c'.  */
    const char *trigger_gbl_ctors = "__trigger_gbl_ctors";
    struct symbol_hash_entry *e_trigger_gbl_ctors
      = symbol_hash_lookup (symbol_table, xstrdup (trigger_gbl_ctors), 0);
    if (e_trigger_gbl_ctors)
      /* We expect link-in of the relevant libgcc object file to correspond to
	 presence of special-purpose functions for global
	 constructor/destructor support.  */
      assert (first_resolve_run == !e_trigger_gbl_ctors->included);
    else
      {
	if (verbose)
	  std::cerr << "Disabling handling of special-purpose functions; '" << trigger_gbl_ctors << "' not available\n";
	/* Disable the processing here, for backwards compatibility with GCC
	   versions not supporting this interface for global
	   constructor/destructor support.  */
	special_purpose_functions.clear ();
      }

    if (first_resolve_run)
      {
	/* Handle special-purpose functions...  */
	int ret = handle_special_purpose_functions (symbol_table, outfile);
	special_purpose_functions.clear ();
	if (ret < 0)
	  goto error_out;
	else if (ret > 0)
	  {
	    /* ..., and if we found any, trigger link-in of the relevant libgcc
	       object file (and its dependencies).  */
	    unresolved = e_trigger_gbl_ctors;
	    first_resolve_run = false;
	    goto resolve;
	  }
      }
    else
      {
	if (!special_purpose_functions.empty ())
	  {
	    /* This means that we found additional special-purpose functions
	       during link-in of the relevant libgcc object file (and its
	       dependencies).  */
	    std::cerr << "unhandled additional special-purpose functions\n";
	    goto error_out;
	  }
      }
  }
  }

  /* Clean up.  */

  htab_delete (symbol_table);
  while (!f_to_clean_up.empty())
    {
      struct file_hash_entry *f = f_to_clean_up.front ();
      file_hash_free (f);
      f_to_clean_up.pop_front ();
    }

  fclose (outfile);

  return 0;

 error_out:
  htab_delete (symbol_table);
  while (!f_to_clean_up.empty())
    {
      struct file_hash_entry *f = f_to_clean_up.front ();
      file_hash_free (f);
      f_to_clean_up.pop_front ();
    }

  fclose (outfile);
  unlink (outname);

  return 1;
}
