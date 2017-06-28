/* An "assembler" for PTX.
   Copyright (C) 2014, 2015 Mentor Graphics
   Contributed by Nathan Sidwell <nathan@codesourcery.com>
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

/* Munges GCC-generated PTX assembly so that it becomes acceptable for ptxas
   and the CUDA driver library.

   This is not a complete assembler.  We presume the source is well
   formed from the compiler and can die horribly if it is not.  */

#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <wait.h>
#include <unistd.h>
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include <errno.h>
#define obstack_chunk_alloc malloc
#define obstack_chunk_free free
#include <obstack.h>
#define HAVE_DECL_BASENAME 1
#include <libiberty.h>
#include <hashtab.h>

#include <list>

#include "version.h"

#ifndef R_OK
#define R_OK 4
#define W_OK 2
#define X_OK 1
#endif

#ifndef DIR_SEPARATOR
#  define DIR_SEPARATOR '/'
#endif

#if defined (_WIN32) || defined (__MSDOS__) \
    || defined (__DJGPP__) || defined (__OS2__)
#  define HAVE_DOS_BASED_FILE_SYSTEM
#  define HAVE_HOST_EXECUTABLE_SUFFIX
#  define HOST_EXECUTABLE_SUFFIX ".exe"
#  ifndef DIR_SEPARATOR_2 
#    define DIR_SEPARATOR_2 '\\'
#  endif
#  define PATH_SEPARATOR ';'
#else
#  define PATH_SEPARATOR ':'
#endif

#ifndef DIR_SEPARATOR_2
#  define IS_DIR_SEPARATOR(ch) ((ch) == DIR_SEPARATOR)
#else
#  define IS_DIR_SEPARATOR(ch) \
	(((ch) == DIR_SEPARATOR) || ((ch) == DIR_SEPARATOR_2))
#endif

#define DIR_UP ".."

static const char *outname = NULL;

static void __attribute__ ((format (printf, 1, 2)))
fatal_error (const char * cmsgid, ...)
{
  va_list ap;

  va_start (ap, cmsgid);
  fprintf (stderr, "nvptx-as: ");
  vfprintf (stderr, cmsgid, ap);
  fprintf (stderr, "\n");
  va_end (ap);

  unlink (outname);
  exit (1);
}

struct Stmt;

class symbol
{
 public:
  symbol (const char *k) : key (k), stmts (0), pending (0), emitted (0)
    { }

  /* The name of the symbol.  */
  const char *key;
  /* A linked list of dependencies for the initializer.  */
  std::list<symbol *> deps;
  /* The statement in which it is defined.  */
  struct Stmt *stmts;
  bool pending;
  bool emitted;
};

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

static symbol *
symbol_hash_lookup (const char *string)
{
  void **e;
  e = htab_find_slot_with_hash (symbol_table, string,
                                (*htab_hash_string) (string),
                                INSERT);
  if (e == NULL)
    return NULL;
  if (*e == NULL)
    *e = new symbol (string);

  return (symbol *) *e;
}

#define COMMENT_PREFIX "#"

typedef enum Kind
{
  /* 0-ff used for single char tokens */
  K_symbol = 0x100, /* a symbol */
  K_label,  /* a label defn (i.e. symbol:) */
  K_ident,  /* other ident */
  K_dotted, /* dotted identifier */
  K_number,
  K_string,
  K_comment
} Kind;

typedef struct Token
{
  unsigned short kind : 12;
  unsigned short space : 1; /* preceded by space */
  unsigned short end : 1;   /* succeeded by end of line */
  /* Length of token */
  unsigned short len;

  /* Token itself */
  char const *ptr;
} Token;

/* statement info */
typedef enum Vis
{
  V_dot = 0,  /* random pseudo */
  V_var = 1,  /* var decl/defn */
  V_func = 2, /* func decl/defn */
  V_insn = 3, /* random insn */
  V_label = 4, /* label defn */
  V_comment = 5,
  V_pred = 6,  /* predicate */
  V_mask = 0x7,
  V_global = 0x08, /* globalize */
  V_weak = 0x10,   /* weakly globalize */
  V_no_eol = 0x20, /* no end of line */
  V_prefix_comment = 0x40 /* prefixed comment */
} Vis;

typedef struct Stmt
{
  struct Stmt *next;
  Token *tokens;
  symbol *sym;
  unsigned char vis;
  unsigned len : 12;
} Stmt;

struct id_map
{
  id_map *next;
  char *ptx_name;
};

#define alloc_comment(S,E) alloc_stmt (V_comment, S, E, 0)
#define append_stmt(V, S) ((S)->next = *(V), *(V) = (S))

static Stmt *decls;
static Stmt *fns;

static id_map *func_ids, **funcs_tail = &func_ids;
static id_map *var_ids, **vars_tail = &var_ids;

static void
record_id (const char *p1, id_map ***where)
{
  const char *end = strchr (p1, '\n');
  if (!end)
    fatal_error ("malformed ptx file");

  id_map *v = XNEW (id_map);
  size_t len = end - p1;
  v->ptx_name = XNEWVEC (char, len + 1);
  memcpy (v->ptx_name, p1, len);
  v->ptx_name[len] = '\0';
  v->next = NULL;
  id_map **tail = *where;
  *tail = v;
  *where = &v->next;
}

/* Read the whole input file.  It will be NUL terminated (but
   remember, there could be a NUL in the file itself.  */

static const char *
read_file (FILE *stream)
{
  size_t alloc = 16384;
  size_t base = 0;
  char *buffer;

  if (!fseek (stream, 0, SEEK_END))
    {
      /* Get the file size.  */
      long s = ftell (stream);
      if (s >= 0)
	alloc = s + 100;
      fseek (stream, 0, SEEK_SET);
    }
  buffer = XNEWVEC (char, alloc);

  for (;;)
    {
      size_t n = fread (buffer + base, 1, alloc - base - 1, stream);

      if (!n)
	break;
      base += n;
      if (base + 1 == alloc)
	{
	  alloc *= 2;
	  buffer = XRESIZEVEC (char, buffer, alloc);
	}
    }
  buffer[base] = 0;
  return buffer;
}

/* Read a token, advancing ptr.
   If we read a comment, append it to the comments block. */

static Token *
tokenize (const char *ptr)
{
  unsigned alloc = 1000;
  unsigned num = 0;
  Token *toks = XNEWVEC (Token, alloc);
  int in_comment = 0;
  int not_comment = 0;
  unsigned char c;

  for (;; num++)
    {
      const char *base;
      unsigned kind;
      int ws = 0;
      int eol = 0;

    again:
      base = ptr;
      if (in_comment)
	goto block_comment;
      c = (unsigned char)*ptr++;
      if (c > 127)
	fatal_error ("non-ascii character encountered: 0x%x", c);
      switch (kind = c)
	{
	default:
	  break;

	case '\n':
	  eol = 1;
	  /* Fall through */
	case ' ':
	case '\t':
	case '\r':
	case '\v':
	  /* White space */
	  ws = not_comment;
	  goto again;

	case '/':
	  {
	    if (*ptr == '/')
	      {
		/* line comment.  Do not include trailing \n */
		base += 2;
		for (; *ptr; ptr++)
		  if (*ptr == '\n')
		    break;
		kind = K_comment;
	      }
	    else if (*ptr == '*')
	      {
		/* block comment */
		base += 2;
		ptr++;

	      block_comment:
		eol = in_comment;
		in_comment = 1;
		for (; *ptr; ptr++)
		  {
		    if (*ptr == '\n')
		      {
			ptr++;
			break;
		      }
		    if (ptr[0] == '*' && ptr[1] == '/')
		      {
			in_comment = 2;
			ptr += 2;
			break;
		      }
		  }
		kind = K_comment;
	      }
	    else
	      break;
	  }
	  break;

	case '"':
	  /* quoted string */
	  kind = K_string;
	  while (*ptr)
	    if (*ptr == '"')
	      {
		ptr++;
		break;
	      }
	    else if (*ptr++ == '\\')
	      ptr++;
	  break;

	case '.':
	  if (*ptr < '0' || *ptr > '9')
	    {
	      kind = K_dotted;
	      ws = not_comment;
	      goto ident;
	    }
	  /* FALLTHROUGH */
	case '0'...'9':
	  kind = K_number;
	  goto ident;
	  break;

	case '$':  /* local labels.  */
	case '%':  /* register names, pseudoes etc */
	  kind = K_ident;
	  goto ident;

	case 'a'...'z':
	case 'A'...'Z':
	case '_':
	  kind = K_symbol; /* possible symbol name */
	ident:
	  for (; *ptr; ptr++)
	    {
	      if (*ptr >= 'A' && *ptr <= 'Z')
		continue;
	      if (*ptr >= 'a' && *ptr <= 'z')
		continue;
	      if (*ptr >= '0' && *ptr <= '9')
		continue;
	      if (*ptr == '_' || *ptr == '$')
		continue;
	      if (*ptr == '.' && kind != K_dotted)
		/* Idents starting with a dot, cannot have internal dots. */
		continue;
	      if ((*ptr == '+' || *ptr == '-')
		  && kind == K_number
		  && (ptr[-1] == 'e' || ptr[-1] == 'E'
		      || ptr[-1] == 'p' || ptr[-1] == 'P'))
		/* exponent */
		continue;
	      break;
	    }
	  if (*ptr == ':')
	    {
	      ptr++;
	      kind = K_label;
	    }
	  break;
	}

      if (alloc == num)
	{
	  alloc *= 2;
	  toks = XRESIZEVEC (Token, toks, alloc);
	}
      Token *tok = toks + num;

      tok->kind = kind;
      tok->space = ws;
      tok->end = 0;
      tok->ptr = base;
      tok->len = ptr - base - in_comment;
      in_comment &= 1;
      not_comment = kind != K_comment;
      if (eol && num)
	tok[-1].end = 1;
      if (!kind)
	break;
    }

  return toks;
}

/* Write an encoded token. */

static void
write_token (FILE *out, Token const *tok)
{
  if (tok->space)
    fputc (' ', out);

  switch (tok->kind)
    {
    case K_string:
      {
	const char *c = tok->ptr + 1;
	size_t len = tok->len - 2;

	fputs ("\"", out);
	while (len)
	  {
	    const char *bs = (const char *)memchr (c, '\\', len);
	    size_t l = bs ? bs - c : len;

	    fprintf (out, "%.*s", (int)l, c);
	    len -= l;
	    c += l;
	    if (bs)
	      {
		fputs ("\\\\", out);
		len--, c++;
	      }
	  }
	fputs ("\"", out);
      }
      break;

    default:
      /* All other tokens shouldn't have anything magic in them */
      fprintf (out, "%.*s", tok->len, tok->ptr);
      break;
    }

  if (tok->end)
    fputs ("\n", out);
}

static Stmt *
alloc_stmt (unsigned vis, Token *tokens, Token *end, symbol *sym)
{
  static unsigned alloc = 0;
  static Stmt *heap = 0;

  if (!alloc)
    {
      alloc = 1000;
      heap = XNEWVEC (Stmt, alloc);
    }

  Stmt *stmt = heap++;
  alloc--;

  tokens->space = 0;
  stmt->next = 0;
  stmt->vis = vis;
  stmt->tokens = tokens;
  stmt->len = end - tokens;
  stmt->sym = sym;

  return stmt;
}

static Stmt *
rev_stmts (Stmt *stmt)
{
  Stmt *prev = 0;
  Stmt *next;

  while (stmt)
    {
      next = stmt->next;
      stmt->next = prev;
      prev = stmt;
      stmt = next;
    }

  return prev;
}

static void
write_stmt (FILE *out, const Stmt *stmt)
{
  for (int i = 0; i < stmt->len; i++)
    {
      if ((stmt->vis & V_mask) == V_comment)
	fprintf (out, "//");
      write_token (out, stmt->tokens + i);
      if ((stmt->vis & V_mask) == V_pred)
	fputc (' ', out);
    }
}

static void
write_stmts (FILE *out, const Stmt *stmts)
{
  for (; stmts; stmts = stmts->next)
    write_stmt (out, stmts);
}

#define is_keyword(T,S) \
  (sizeof (S) == (T)->len && !memcmp ((T)->ptr + 1, (S), (T)->len - 1))

/* Parse a line until the end, regardless of semicolons.  */

static Token *
parse_line_nosemi (Token *tok, Stmt **append_where)
{
  Token *start = tok;

  do
    tok++;
  while (!tok[-1].end);

  Stmt *stmt = alloc_stmt (V_dot, start, tok, 0);
  append_stmt (append_where, stmt);

  return tok;
}

static Token *
parse_insn (Token *tok)
{
  unsigned depth = 0;

  do
    {
      Stmt *stmt;
      unsigned s = V_insn;
      Token *start = tok;

      if (tok->kind == K_dotted)
	{
	  if (is_keyword (tok, "file"))
	    {
	      tok = parse_line_nosemi (tok, &decls);
	      continue;
	    }
	  if (is_keyword (tok, "loc"))
	    {
	      tok = parse_line_nosemi (tok, &fns);
	      continue;
	    }
	}
      switch (tok++->kind)
	{
	case K_comment:
	  while (tok->kind == K_comment)
	    tok++;
	  stmt = alloc_comment (start, tok);
	  append_stmt (&fns, stmt);
	  continue;

	case '{':
	  depth++;
	  break;

	case '}':
	  depth--;
	  break;

	case K_label:
	  tok[-1].end = 1;
	  s = V_label;
	  break;

	case '@':
	  tok->space = 0;
	  if (tok->kind == '!')
	    tok++;
	  tok++;
	  s = V_pred;
	  break;

	default:
	  for (; tok->kind != ';'; tok++)
	    {
	      if (tok->kind == ',')
		tok[1].space = 0;
	    }
	  tok++->end = 1;
	  break;
	}

      stmt = alloc_stmt (s, start, tok, 0);
      append_stmt (&fns, stmt);

      if (!tok[-1].end && tok[0].kind == K_comment)
	{
	  stmt->vis |= V_no_eol;
	  stmt = alloc_comment (tok, tok + 1);
	  append_stmt (&fns, stmt);
	  tok++;
	}
    }
  while (depth);

  return tok;
}

static Token *
parse_init (Token *tok, symbol *sym)
{
  for (;;)
    {
      Token *start = tok;
      Token *def_tok = 0;
      Stmt *stmt;

      if (tok->kind == K_comment)
	{
	  while (tok->kind == K_comment)
	    tok++;
	  stmt = alloc_comment (start, tok);
	  append_stmt (&sym->stmts, stmt);
	  start = tok;
	}

      if (tok->kind == '{')
	tok[1].space = 0;
      /* Find the last symbol before the next comma.  This allows us
	 to do the right thing for constructs like "generic (sym)".  */
      for (; tok->kind != ',' && tok->kind != ';'; tok++)
	if (tok->kind == K_symbol || tok->kind == K_ident)
	  def_tok = tok;
      if (def_tok)
	sym->deps.push_back (symbol_hash_lookup (xstrndup (def_tok->ptr,
							   def_tok->len)));
      tok[1].space = 0;
      int end = tok++->kind == ';';
      stmt = alloc_stmt (V_insn, start, tok, 0);
      append_stmt (&sym->stmts, stmt);
      if (!tok[-1].end && tok->kind == K_comment)
	{
	  stmt->vis |= V_no_eol;
	  stmt = alloc_comment (tok, tok + 1);
	  append_stmt (&sym->stmts, stmt);
	  tok++;
	}
      if (end)
	break;
    }
  return tok;
}

static Token *
parse_file (Token *tok)
{
  Stmt *comment = 0;

  if (tok->kind == K_comment)
    {
      Token *start = tok;

      while (tok->kind == K_comment)
	{
	  if (strncmp (tok->ptr, ":VAR_MAP ", 9) == 0)
	    record_id (tok->ptr + 9, &vars_tail);
	  if (strncmp (tok->ptr, ":FUNC_MAP ", 10) == 0)
	    record_id (tok->ptr + 10, &funcs_tail);
	  tok++;
	}
      comment = alloc_comment (start, tok);
      comment->vis |= V_prefix_comment;
    }

  if (tok->kind == K_dotted)
    {
      if (is_keyword (tok, "version")
	  || is_keyword (tok, "target")
	  || is_keyword (tok, "address_size")
	  || is_keyword (tok, "file"))
	{
	  if (comment)
	    append_stmt (&decls, comment);
	  tok = parse_line_nosemi (tok, &decls);
	}
      else
	{
	  unsigned vis = 0;
	  symbol *def = 0;
	  unsigned is_decl = 0;
	  Token *start, *def_token = 0;

	  for (start = tok;
	       tok->kind && tok->kind != '=' && tok->kind != K_comment
		 && tok->kind != '{' && tok->kind != ';'; tok++)
	    {
	      if (is_keyword (tok, "global")
		  || is_keyword (tok, "const"))
		vis |= V_var;
	      else if (is_keyword (tok, "func")
		       || is_keyword (tok, "entry"))
		vis |= V_func;
	      else if (is_keyword (tok, "visible"))
		vis |= V_global;
	      else if (is_keyword (tok, "extern"))
		is_decl = 1;
	      else if (is_keyword (tok, "weak"))
		vis |= V_weak;
	      if (tok->kind == '(')
		{
		  tok[1].space = 0;
		  tok[0].space = 1;
		}
	      else if (tok->kind == ')' && tok[1].kind != ';')
		tok[1].space = 1;

	      if (tok->kind == K_symbol || tok->kind == K_ident)
		def_token = tok;
	    }
	  if (def_token)
	    def = symbol_hash_lookup (xstrndup (def_token->ptr, def_token->len));

	  if (!tok->kind)
	    {
	      /* end of file */
	      if (comment)
		append_stmt (&fns, comment);
	    }
	  else if (tok->kind == '{'
		   || tok->kind == K_comment)
	    {
	      /* function defn */
	      Stmt *stmt = alloc_stmt (vis, start, tok, def);
	      if (comment)
		{
		  append_stmt (&fns, comment);
		  stmt->vis |= V_prefix_comment;
		}
	      append_stmt (&fns, stmt);
	      tok = parse_insn (tok);
	    }
	  else
	    {
	      int assign = tok->kind == '=';

	      tok++->end = 1;
	      if ((vis & V_mask) == V_var && !is_decl)
		{
		  if (def == NULL)
		    {
		      const char *eol = strchr (start->ptr, '\n');
		      const char *line
			= ((eol == NULL)
			   ? start->ptr
			   : strndup (start->ptr, eol - start->ptr));
		      fatal_error ("expected identifier in line '%s'", line);
		    }
		  /* variable */
		  Stmt *stmt = alloc_stmt (vis, start, tok, def);
		  if (comment)
		    {
		      append_stmt (&def->stmts, comment);
		      stmt->vis |= V_prefix_comment;
		    }
		  append_stmt (&def->stmts, stmt);
		  if (assign)
		    tok = parse_init (tok, def);
		}
	      else
		{
		  /* declaration */
		  Stmt *stmt = alloc_stmt (vis, start, tok, 0);
		  if (comment)
		    {
		      append_stmt (&decls, comment);
		      stmt->vis |= V_prefix_comment;
		    }
		  append_stmt (&decls, stmt);
		}
	    }
	}
    }
  else
    {
      /* Something strange.  Ignore it.  */
      if (comment)
	append_stmt (&fns, comment);

      while (tok->kind && !tok->end)
	tok++;
      if (tok->kind)
	tok++;
    }
  return tok;
}

static void
output_symbol (FILE *out, symbol *e)
{
  if (e->emitted)
    return;
  if (e->pending)
    fatal_error ("circular reference in variable initializers");
  e->pending = true;
  std::list<symbol *>::iterator i;
  for (i = e->deps.begin (); i != e->deps.end (); i++)
    output_symbol (out, *i);
  e->pending = false;
  write_stmts (out, rev_stmts (e->stmts));
  e->emitted = true;
}

static int
traverse (void **slot, void *data)
{
  symbol *e = *(symbol **)slot;
  output_symbol ((FILE *)data, e);
  return 1;
}

static void
process (FILE *in, FILE *out, int verify, const char *outname)
{
  symbol_table = htab_create (500, hash_string_hash, hash_string_eq,
                              NULL);

  const char *input = read_file (in);
  Token *tok = tokenize (input);

  /* By default, when ptxas is not in PATH, do minimalistic verification,
     just require that the first non-comment directive is .version.  */
  if (verify < 0)
    {
      size_t i;
      for (i = 0; tok[i].kind == K_comment; i++)
	;
      if (tok[i].kind != K_dotted || !is_keyword (&tok[i], "version"))
	fatal_error ("missing .version directive at start of file '%s'",
		     outname);
    }

  do
    tok = parse_file (tok);
  while (tok->kind);

  write_stmts (out, rev_stmts (decls));
  htab_traverse (symbol_table, traverse, (void *)out);
  write_stmts (out, rev_stmts (fns));
}

/* Wait for a process to finish, and exit if a nonzero status is found.  */

int
collect_wait (const char *prog, struct pex_obj *pex)
{
  int status;

  if (!pex_get_status (pex, 1, &status))
    fatal_error ("can't get program status: %m");
  pex_free (pex);

  if (status)
    {
      if (WIFSIGNALED (status))
	{
	  int sig = WTERMSIG (status);
	  fatal_error ("%s terminated with signal %d [%s]%s",
		       prog, sig, strsignal(sig),
		       WCOREDUMP(status) ? ", core dumped" : "");
	}

      if (WIFEXITED (status))
	return WEXITSTATUS (status);
    }
  return 0;
}

static void
do_wait (const char *prog, struct pex_obj *pex)
{
  int ret = collect_wait (prog, pex);
  if (ret != 0)
    {
      fatal_error ("%s returned %d exit status", prog, ret);
    }
}


/* Execute a program, and wait for the reply.  */
static void
fork_execute (const char *prog, char *const *argv)
{
  struct pex_obj *pex = pex_init (0, "nvptx-as", NULL);
  if (pex == NULL)
    fatal_error ("pex_init failed: %m");

  int err;
  const char *errmsg;

  errmsg = pex_run (pex, PEX_LAST | PEX_SEARCH, argv[0], argv, NULL,
		    NULL, &err);
  if (errmsg != NULL)
    {
      if (err != 0)
	{
	  errno = err;
	  fatal_error ("%s: %m", errmsg);
	}
      else
	fatal_error ("%s", errmsg);
    }
  do_wait (prog, pex);
}

/* Determine if progname is available in PATH.  */
static bool
program_available (const char *progname)
{
  char *temp = getenv ("PATH");
  if (temp)
    {
      char *startp, *endp, *nstore, *alloc_ptr = NULL;
      size_t prefixlen = strlen (temp) + 1;
      size_t len;
      if (prefixlen < 2)
	prefixlen = 2;

      len = prefixlen + strlen (progname) + 1;
#ifdef HAVE_HOST_EXECUTABLE_SUFFIX
      len += strlen (HOST_EXECUTABLE_SUFFIX);
#endif
      if (len < MAX_ALLOCA_SIZE)
	nstore = (char *) alloca (len);
      else
	alloc_ptr = nstore = (char *) malloc (len);

      startp = endp = temp;
      while (1)
	{
	  if (*endp == PATH_SEPARATOR || *endp == 0)
	    {
	      if (endp == startp)
		{
		  nstore[0] = '.';
		  nstore[1] = DIR_SEPARATOR;
		  nstore[2] = '\0';
		}
	      else
		{
		  memcpy (nstore, startp, endp - startp);
		  if (! IS_DIR_SEPARATOR (endp[-1]))
		    {
		      nstore[endp - startp] = DIR_SEPARATOR;
		      nstore[endp - startp + 1] = 0;
		    }
		  else
		    nstore[endp - startp] = 0;
		}
	      strcat (nstore, progname);
	      if (! access (nstore, X_OK)
#ifdef HAVE_HOST_EXECUTABLE_SUFFIX
		  || ! access (strcat (nstore, HOST_EXECUTABLE_SUFFIX), X_OK)
#endif
		 )
		{
#if defined (HAVE_SYS_STAT_H) && defined (S_ISREG)
		  struct stat st;
		  if (stat (nstore, &st) >= 0 && S_ISREG (st.st_mode))
#endif
		    {
		      free (alloc_ptr);
		      return true;
		    }
		}

	      if (*endp == 0)
		break;
	      endp = startp = endp + 1;
	    }
	  else
	    endp++;
	}
      free (alloc_ptr);
    }
  return false;
}

static struct option long_options[] = {
  {"traditional-format",     no_argument, 0,  0 },
  {"save-temps",  no_argument,       0,  0 },
  {"verify",  no_argument,       0,  0 },
  {"no-verify",  no_argument,       0,  0 },
  {"help", no_argument, 0, 'h' },
  {"version", no_argument, 0, 'V' },
  {0,         0,                 0,  0 }
};

int
main (int argc, char **argv)
{
  FILE *in = stdin;
  FILE *out = stdout;
  bool verbose __attribute__((unused)) = false;
  int verify = -1;
  const char *smver = "sm_30";

  int o;
  int option_index = 0;
  while ((o = getopt_long (argc, argv, "o:I:m:v", long_options, &option_index)) != -1)
    {
      switch (o)
	{
	case 0:
	  if (option_index == 2)
	    verify = 1;
	  else if (option_index == 3)
	    verify = 0;
	  break;
	case 'v':
	  verbose = true;
	  break;
	case 'o':
	  if (outname != NULL)
	    {
	      fprintf (stderr, "multiple output files specified\n");
	      exit (1);
	    }
	  outname = optarg;
	  break;
	case 'm':
	  smver = optarg;
	  break;
	case 'I':
	  /* Ignore include paths.  */
	  break;
	case 'h':
	  printf ("\
Usage: nvptx-none-as [option...] [asmfile]\n\
Options:\n\
  -o FILE               Write output to FILE\n\
  -v                    Be verbose\n\
  --verify              Do verify output is acceptable to ptxas\n\
  --no-verify           Do not verify output is acceptable to ptxas\n\
  --help                Print this help and exit\n\
  --version             Print version number and exit\n\
\n\
Report bugs to %s.\n",
		  REPORT_BUGS_TO);
	  exit (0);
	case 'V':
	  printf ("\
nvtpx-none-as %s%s\n\
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

  if (optind + 1 < argc)
    fatal_error ("too many input files specified");

  if (outname)
    out = fopen (outname, "w");
  if (!out)
    fatal_error ("cannot open '%s'", outname);

  if (argc > optind)
    in = fopen (argv[optind], "r");
  if (!in)
    fatal_error ("cannot open input ptx file");

  if (outname == NULL)
    verify = 0;
  else if (verify == -1)
    if (program_available ("ptxas"))
      verify = 1;

  process (in, out, verify, outname);
  if (outname)
    fclose (out);

  if (verify > 0)
    {
      struct obstack argv_obstack;
      obstack_init (&argv_obstack);
      obstack_ptr_grow (&argv_obstack, "ptxas");
      obstack_ptr_grow (&argv_obstack, "-c");
      obstack_ptr_grow (&argv_obstack, "-o");
      obstack_ptr_grow (&argv_obstack, "/dev/null");
      obstack_ptr_grow (&argv_obstack, outname);
      obstack_ptr_grow (&argv_obstack, "--gpu-name");
      obstack_ptr_grow (&argv_obstack, smver);
      obstack_ptr_grow (&argv_obstack, "-O0");
      obstack_ptr_grow (&argv_obstack, NULL);
      char *const *new_argv = XOBFINISH (&argv_obstack, char *const *);
      fork_execute (new_argv[0], new_argv);
    }
  return 0;
}
