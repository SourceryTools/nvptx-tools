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
#include "obstack.h"
#define HAVE_DECL_BASENAME 1
#include "libiberty.h"

#include <list>
#include <string>
#include <iostream>

#include "version.h"

struct file_hash_entry;

typedef struct symbol_hash_entry
{
  /* The name of the symbol.  */
  const char *key;
  /* A linked list of unresolved referenced symbols.  */
  struct symbol_hash_entry **pprev, *next;
  /* The file in which it is defined.  */
  struct file_hash_entry *def;
  int included;
  int referenced;
} symbol;

typedef struct file_hash_entry
{
  struct file_hash_entry **pprev, *next;
  const char *name;
  const char *arname;
  const char *data;
  size_t len;
} file;

/* Hash and comparison functions for these hash tables.  */

static int hash_string_eq (const void *, const void *);
static hashval_t hash_string_hash (const void *);

static int
hash_string_eq (const void *s1_p, const void *s2_p)
{
  const char *const *s1 = (const char *const *) s1_p;
  const char *s2 = (const char *) s2_p;
  return strcmp (*s1, s2) == 0;
}

static hashval_t
hash_string_hash (const void *s_p)
{
  const char *const *s = (const char *const *) s_p;
  return (*htab_hash_string) (*s);
}

static htab_t symbol_table;

/* Look up an entry in the symbol hash table.  */

static struct symbol_hash_entry *
symbol_hash_lookup (const char *string, int create)
{
  void **e;
  e = htab_find_slot_with_hash (symbol_table, string,
                                (*htab_hash_string) (string),
                                create ? INSERT : NO_INSERT);
  if (e == NULL)
    return NULL;
  if (*e == NULL)
    {
      struct symbol_hash_entry *v;
      *e = v = XCNEW (struct symbol_hash_entry);
      v->key = string;
    }
  return (struct symbol_hash_entry *) *e;
}

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

using namespace std;

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
  bool init (FILE *file)
  {
    char magic[SARMAG];
    if (fread (magic, 1, SARMAG, file) != SARMAG)
      return false;
    if (memcmp (magic, ARMAG, SARMAG) != 0)
      return false;
    f = file;
    fseek (f, 0, SEEK_END);
    flen = ftell (f);
    fseek (f, SARMAG, SEEK_SET);
    off = SARMAG;

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

FILE *
path_open (const char *filename, list<string> &paths)
{
  FILE *f = fopen (filename, "r");
  if (f)
    return f;
  if (strchr (filename, '/') != NULL)
    return NULL;

  for (list<string>::const_iterator iterator = paths.begin(), end = paths.end();
       iterator != end;
       ++iterator)
    {
      string tmp = *iterator;
      tmp += '/';
      tmp += filename;
      FILE *f = fopen (tmp.c_str (), "r");
      if (f)
	return f;
    }
  return NULL;
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
define_intrinsics ()
{
  static const char *const intrins[] =
    {"vprintf", "malloc", "free", NULL};
  unsigned ix;

  for (ix = 0; intrins[ix]; ix++)
    {
      struct symbol_hash_entry *e = symbol_hash_lookup (intrins[ix], 1);
      e->included = true;
    }
}

static void
process_refs_defs (file *f, const char *ptx)
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

	  const char *sym = xstrndup (ptx, end - ptx);
	  struct symbol_hash_entry *e = symbol_hash_lookup (sym, 1);

	  if (!e->included)
	    {
	      if (type == 1)
		{
		  if (f == NULL)
		    {
		      e->included = true;
		      dequeue_unresolved (e);
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
}

static const struct option long_options[] = {
  {"help", no_argument, 0, 'h' },
  {"version", no_argument, 0, 'V' },
  {0, 0, 0, 0 }
};

int
main (int argc, char **argv)
{
  const char *outname = NULL;
  list<string> libraries;
  list<string> libpaths;
  bool verbose = false;

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
	      cerr << "multiple output files specified\n";
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
	  printf ("\
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
	  exit (0);
	case 'V':
	  printf ("\
nvtpx-none-ld %s%s\n\
Copyright %s Mentor Graphics\n\
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>\n\
This program is free software; you may redistribute it under the terms of\n\
the GNU General Public License version 3 or later.\n\
This program has absolutely no warranty.\n",
		  PKGVERSION, NVPTX_TOOLS_VERSION, "2015");
	  exit (0);
	default:
	  break;
	}
    }

  libraries.sort ();
  libraries.unique ();
  libpaths.unique ();

  if (outname == NULL)
    outname = "a.out";

  symbol_table = htab_create (500, hash_string_hash, hash_string_eq,
                              NULL);

  define_intrinsics ();
  
  FILE *outfile = fopen (outname, "w");
  if (outfile == NULL)
    {
      cerr << "error opening output file\n";
      exit (1);
    }
  list<string> inputfiles;
  while (optind < argc)
    inputfiles.push_back (argv[optind++]);

  int idx = 0;
  for (list<string>::const_iterator iterator = inputfiles.begin(), end = inputfiles.end();
       iterator != end;
       ++iterator)
    {
      const string &name = *iterator;
      FILE *f = path_open (name.c_str (), libpaths);
      if (f == NULL)
	{
	  cerr << "error opening " << name << "\n";
	  goto error_out;
	}
      fseek (f, 0, SEEK_END);
      off_t len = ftell (f);
      fseek (f, 0, SEEK_SET);
      char *buf = new char[len + 1];
      size_t read_len = fread (buf, 1, len, f);
      buf[len] = '\0';
      if (read_len != len || ferror (f))
	{
	  cerr << "error reading " << name << "\n";
	  goto error_out;
	}
      size_t out = fwrite (buf, 1, len, outfile);
      if (out != len)
	{
	  cerr << "error writing to output file\n";
	  goto error_out;
	}
      process_refs_defs (NULL, buf);
      free (buf);
      if (verbose)
	cout << "Linking " << name << " as " << idx++ << "\n";
      fputc ('\0', outfile);
    }
  for (list<string>::const_iterator iterator = libraries.begin(), end = libraries.end();
       iterator != end;
       ++iterator)
    {
      const string &name = "lib" + *iterator + ".a";
      if (verbose)
	cout << "trying lib " << name << "\n";
      FILE *f = path_open (name.c_str (), libpaths);
      if (f == NULL)
	{
	  cerr << "error opening " << name << "\n";
	  goto error_out;
	}
      archive ar;
      if (!ar.init (f))
	{
	  cerr << name << " is not a valid archive\n";
	  goto error_out;
	}
      while (!ar.at_end ())
	{
	  if (!ar.next_file ())
	    {
	      cerr << "error reading from archive " << name << "\n";
	      goto error_out;
	    }
	  const char *p = xstrdup (ar.get_contents ());
	  size_t len = ar.get_len ();
	  file *f = file_hash_new (p, len, name.c_str (), ar.get_name ());
	  process_refs_defs (f, p);
	}
    }

  while (unresolved)
    {
      struct file_hash_entry *to_add = NULL;
      struct symbol_hash_entry *e;
      for (e = unresolved; e; e = e->next)
	{
	  struct file_hash_entry *f = e->def;
	  if (!f)
	    {
	      cerr << "unresolved symbol " << e->key << "\n";
	      goto error_out;
	    }
	  if (verbose)
	    cout << "Resolving " << e->key << "\n";
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
	    cout << "Linking " << f->arname << "::" << f->name << " as " << idx++ << "\n";
	  if (fwrite (f->data, 1, f->len, outfile) != f->len)
	    {
	      cerr << "error writing to output file\n";
	      goto error_out;
	    }
	  fputc ('\0', outfile);
	  process_refs_defs (NULL, f->data);
	}
    }
  return 0;

 error_out:
  fclose (outfile);
  unlink (outname);
  return 1;
}
