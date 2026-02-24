/* An "assembler" for PTX.
   Copyright (C) 2014, 2015 Mentor Graphics
   Copyright (C) 2017 Red Hat
   Copyright (C) 2017, 2021, 2022, 2023 Siemens
   Copyright (C) 2020 SUSE
   Copyright (C) 2024, 2026 BayLibre

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

/* Munges GCC-generated PTX assembly so that it becomes compliant.

   This is not a complete assembler.  We presume the source is well
   formed from the compiler and can die horribly if it is not.  */

#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include <errno.h>
#include <assert.h>

#define HAVE_DECL_BASENAME 1
#include <libiberty.h>
#include <hashtab.h>

#include <list>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>

#include "version.h"

#include "interface-libnvrtc.h"

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

static bool verbose = false;

static const char *outname = NULL;

enum class verify_mode
{
  unset,
  no,
  yes,
  ptxas_libnvrtc,
  ptxas,
};

static void
fatal_error (const std::string &error_string)
{
  std::cerr << "nvptx-as: " << error_string << "\n";

  if (outname)
    unlink (outname);

  exit (1);
}

struct Stmt;

class symbol
{
 public:
  /* Takes ownership of K.  */
  symbol (const char *k) : key (k), stmts (0), pending (0), emitted (0)
    { }
  ~symbol () { free ((void *) key); }

  /* The name of the symbol.  */
  const char *key;
  /* A linked list of dependencies for the initializer.  */
  std::list<symbol *> deps;
  /* The statement in which it is defined.  */
  struct Stmt *stmts;
  bool pending;
  bool emitted;
};

static void
symbol_hash_free (void *elt)
{
  symbol *e = (symbol *) elt;
  delete e;
}

/* Hash and comparison functions for these hash tables.  */

static int hash_string_eq (const void *, const void *);
static hashval_t hash_string_hash (const void *);

static int
hash_string_eq (const void *e1_p, const void *s2_p)
{
  const symbol *s1_p = (const symbol *) e1_p;
  const char *s2 = (const char *) s2_p;
  return strcmp (s1_p->key, s2) == 0;
}

static hashval_t
hash_string_hash (const void *e_p)
{
  const symbol *s_p = (const symbol *) e_p;
  return (*htab_hash_string) (s_p->key);
}

/* Look up an entry in the symbol hash table.

   Takes ownership of STRING.  */

static symbol *
symbol_hash_lookup (htab_t symbol_table, std::vector<symbol *> &symbols, char *string)
{
  symbol **e;
  e = (symbol **) htab_find_slot_with_hash (symbol_table, string,
					    (*htab_hash_string) (string),
					    INSERT);
  if (e == NULL)
    {
      free (string);
      return NULL;
    }
  if (*e == NULL)
    {
      *e = new symbol (string);
      symbols.push_back (*e);
    }
  else
    free (string);

  return *e;
}

enum Kind
{
  /* 0-ff used for single char tokens */
  K_symbol = 0x100, /* a symbol */
  K_label,  /* a label defn (i.e. symbol:) */
  K_ident,  /* other ident */
  K_dotted, /* dotted identifier */
  K_number,
  K_string,
  K_comment
};

struct Token
{
  unsigned short kind : 12;
  unsigned short space : 1; /* preceded by space */
  unsigned short end : 1;   /* succeeded by end of line */
  /* Length of token */
  unsigned short len;

  /* Token itself */
  char const *ptr;
};

#define is_keyword(T,S) \
  (sizeof (S) == (T)->len && !memcmp ((T)->ptr + 1, (S), (T)->len - 1))

/* statement info */
enum Vis
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
};

struct Stmt
{
  struct Stmt *next;
  Token *tokens;
  Token *tokens_end;
  unsigned char vis;
};

#define append_stmt(V, S) ((S)->next = *(V), *(V) = (S))

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
tokenize (const char *ptr, std::ostream &error_stream)
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
	{
	  error_stream << "non-ascii character encountered: " << std::hex << std::showbase << (int) c;
	  XDELETEVEC (toks);
	  return NULL;
	}
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
		eol |= in_comment;
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
write_token (std::ostream &out_stream, Token const *tok)
{
  if (tok->space)
    out_stream << ' ';

  switch (tok->kind)
    {
    case K_string:
      {
	const char *c = tok->ptr + 1;
	size_t len = tok->len - 2;

	out_stream << '"';
	while (len)
	  {
	    const char *bs = (const char *)memchr (c, '\\', len);
	    size_t l = bs ? bs - c : len;

	    out_stream.write (c, l);
	    len -= l;
	    c += l;
	    if (bs)
	      {
		out_stream << "\\\\";
		len--, c++;
	      }
	  }
	out_stream << '"';
      }
      break;

    default:
      /* All other tokens shouldn't have anything magic in them.  */
      out_stream.write (tok->ptr, tok->len);
      break;
    }

  if (tok->end)
    out_stream << '\n';
}

/* The preamble '.target' directive's argument.  */
static char *preamble_target_arg;

/* Minimalistic verification of the preamble as generated by GCC.  */

static bool
verify_preamble (const Token *tok, std::ostream &error_stream)
{
  while (tok->kind == K_comment)
    tok++;
  if (tok->kind == K_dotted && is_keyword (tok, "version"))
    tok++;
  else
    {
      error_stream << "missing .version directive";
      return false;
    }
  /* 'ptxas' doesn't seem to allow comments or line breaks here, but it's
     not documented.  */
  while (tok->kind == K_comment)
    tok++;
  if (tok->kind == K_number)
    tok++;
  else
    {
      error_stream << "malformed .version directive";
      return false;
    }
  while (tok->kind == K_comment)
    tok++;
  if (tok->kind == K_dotted && is_keyword (tok, "target"))
    tok++;
  else
    {
      error_stream << "missing .target directive";
      return false;
    }
  while (tok->kind == K_comment)
    tok++;
  if (tok->kind == K_symbol)
    {
      assert (!preamble_target_arg);
      preamble_target_arg = xstrndup (tok->ptr, tok->len);
      tok++;
    }
  else
    {
      error_stream << "malformed .target directive";
      return false;
    }
  /* PTX allows here a "comma separated list of target specifiers", but GCC
     doesn't generate that, and we don't support that.  */
  if (tok->kind == ',')
    {
      error_stream << "unsupported list in .target directive";
      return false;
    }
  return true;
}

static std::list<void *> heaps;

static Stmt *
alloc_stmt (unsigned vis, Token *tokens, Token *tokens_end)
{
  static unsigned alloc = 0;
  static Stmt *heap = 0;

  if (!alloc)
    {
      alloc = 1000;
      heap = XNEWVEC (Stmt, alloc);
      heaps.push_back (heap);
    }

  Stmt *stmt = heap++;
  alloc--;

  tokens->space = 0;
  stmt->next = 0;
  stmt->vis = vis;
  stmt->tokens = tokens;
  stmt->tokens_end = tokens_end;

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
write_stmt (std::ostream &out_stream, const Stmt *stmt)
{
  for (Token *tok = stmt->tokens; tok != stmt->tokens_end; ++tok)
    {
      if ((stmt->vis & V_mask) == V_comment)
	out_stream << "//";
      write_token (out_stream, tok);
    }
  if ((stmt->vis & V_mask) == V_pred)
    /* Space after the guard predicate.  */
    out_stream << ' ';
}

static void
write_stmts (std::ostream &out_stream, const Stmt *stmts)
{
  for (; stmts; stmts = stmts->next)
    write_stmt (out_stream, stmts);
}

/* Parse a line until the end, regardless of semicolons.  */

static Token *
parse_line_nosemi (unsigned vis, Token *tok, Stmt **append_where)
{
  Token *start = tok;

  do
    tok++;
  while (!tok[-1].end);

  Stmt *stmt = alloc_stmt (vis, start, tok);
  append_stmt (append_where, stmt);

  return tok;
}

static Token *
parse_insn (Stmt *&decls, Stmt *&fns, Token *tok)
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
	      tok = parse_line_nosemi (V_dot, tok, &decls);
	      continue;
	    }
	  if (is_keyword (tok, "loc"))
	    {
	      tok = parse_line_nosemi (V_dot, tok, &fns);
	      continue;
	    }
	}
      switch (tok++->kind)
	{
	case K_comment:
	  while (tok->kind == K_comment)
	    tok++;
	  stmt = alloc_stmt (V_comment, start, tok);
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
	  if (tok->kind == '!')
	    {
	      /* No space before '!'.  */
	      tok->space = 0;
	      tok++;
	    }
	  /* No space before the predicate variable.  */
	  tok->space = 0;
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

      stmt = alloc_stmt (s, start, tok);
      append_stmt (&fns, stmt);

      if (!tok[-1].end && tok[0].kind == K_comment)
	{
	  stmt->vis |= V_no_eol;
	  stmt = alloc_stmt (V_comment, tok, tok + 1);
	  append_stmt (&fns, stmt);
	  tok++;
	}
    }
  while (depth);

  return tok;
}

static Token *
parse_init (htab_t symbol_table, std::vector<symbol *> &symbols, Token *tok, symbol *sym)
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
	  stmt = alloc_stmt (V_comment, start, tok);
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
	sym->deps.push_back (symbol_hash_lookup (symbol_table, symbols,
						 xstrndup (def_tok->ptr,
							   def_tok->len)));
      tok[1].space = 0;
      int end = tok++->kind == ';';
      stmt = alloc_stmt (V_insn, start, tok);
      append_stmt (&sym->stmts, stmt);
      if (!tok[-1].end && tok->kind == K_comment)
	{
	  stmt->vis |= V_no_eol;
	  stmt = alloc_stmt (V_comment, tok, tok + 1);
	  append_stmt (&sym->stmts, stmt);
	  tok++;
	}
      if (end)
	break;
    }
  return tok;
}

static Token *
parse_file (Stmt *&decls, htab_t symbol_table, std::vector<symbol *> &symbols, Stmt *&fns, Token *tok, std::ostream &error_stream)
{
  Stmt *comment = 0;
  bool is_map_directive = false;

  if (tok->kind == K_comment)
    {
      Token *start = tok;

      while (tok->kind == K_comment)
	{
	  /* Instead of 'K_comment', a point could be made to have these be
	     represented as their own 'Kind'.  */
	  if (strncmp (tok->ptr, ":VAR_MAP ", 9) == 0
	      || strncmp (tok->ptr, ":FUNC_MAP ", 10) == 0
	      || strncmp (tok->ptr, ":IND_FUNC_MAP ", 14) == 0)
	    {
	      is_map_directive = true;
	      break;
	    }
	  tok++;
	}
      if (start != tok)
	{
	  comment = alloc_stmt (V_comment, start, tok);
	  comment->vis |= V_prefix_comment;
	}
    }

  if (is_map_directive)
    {
      /* GCC 'mkoffload' requires these to be emitted in order of appearance;
	 handle via 'decls'.  */
      if (comment)
	append_stmt (&decls, comment);
      tok = parse_line_nosemi (V_comment, tok, &decls);
    }
  else if (tok->kind == K_dotted)
    {
      if (is_keyword (tok, "version")
	  || is_keyword (tok, "target")
	  || is_keyword (tok, "address_size")
	  || is_keyword (tok, "file"))
	{
	  if (comment)
	    append_stmt (&decls, comment);
	  tok = parse_line_nosemi (V_dot, tok, &decls);
	}
      else
	{
	  unsigned vis = 0;
	  bool is_decl = false;
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
		/*TODO It's not clear why '.extern' are handled via 'decls' instead of via 'symbol_table'.  */
		is_decl = true;
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
	      Stmt *stmt = alloc_stmt (vis, start, tok);
	      if (comment)
		{
		  append_stmt (&fns, comment);
		  stmt->vis |= V_prefix_comment;
		}
	      append_stmt (&fns, stmt);
	      tok = parse_insn (decls, fns, tok);
	    }
	  else
	    {
	      int assign = tok->kind == '=';

	      tok++->end = 1;
	      if ((vis & V_mask) == V_var && !is_decl)
		{
		  if (!def_token)
		    {
		      const char *eol = strchr (start->ptr, '\n');
		      const char *line
			= ((eol == NULL)
			   ? start->ptr
			   : strndup (start->ptr, eol - start->ptr));
		      {
			error_stream << "expected identifier in line '" << line << "'";
			if (line != start->ptr)
			  free ((void *) line);
			return NULL;
		      }
		    }

		  /* variable */
		  symbol *def = symbol_hash_lookup (symbol_table, symbols,
						    xstrndup (def_token->ptr, def_token->len));
		  Stmt *stmt = alloc_stmt (vis, start, tok);
		  if (comment)
		    {
		      append_stmt (&def->stmts, comment);
		      stmt->vis |= V_prefix_comment;
		    }
		  append_stmt (&def->stmts, stmt);
		  if (assign)
		    tok = parse_init (symbol_table, symbols, tok, def);
		}
	      else
		{
		  /* declaration */
		  Stmt *stmt = alloc_stmt (vis, start, tok);
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

static bool
symbol_order_deps (std::vector<const symbol *> &symbols_out, symbol *e, std::ostream &error_stream)
{
  if (e->emitted)
    return true;
  if (e->pending)
    {
      error_stream << "circular reference in variable initializers";
      return false;
    }
  e->pending = true;
  std::list<symbol *>::iterator i;
  for (i = e->deps.begin (); i != e->deps.end (); i++)
    if (!symbol_order_deps (symbols_out, *i, error_stream))
      return false;
  e->pending = false;
  symbols_out.push_back (e);
  e->emitted = true;
  return true;
}

static void
process (FILE *in, std::ostream &out_stream, verify_mode &verify, const char *inname)
{
  std::ostringstream error_stream;

  const char *input = read_file (in);

  /* As expected by GCC, handle an empty input file specially.  See
     <https://github.com/SourceryTools/nvptx-tools/pull/26> "[nvptx-as] Allow
     empty input file" for reference.  */
  if (*input == '\0')
    {
      /* Produce an empty output file.  */

      /* An empty file isn't a valid PTX file.  */
      verify = verify_mode::no;

      XDELETEVEC (input);

      return;
    }

  Token *tok = tokenize (input, error_stream);
  if (!tok)
    fatal_error (error_stream.str ());
  Token *tok_to_free = tok;

  /* Do minimalistic verification, so that we reliably reject (certain classes
     of) invalid input.  (Do this here, in case we're not able, later on, to
     verify the whole output file.)  */
  if (verify != verify_mode::no)
    {
      if (!verify_preamble (tok, error_stream))
	{
	  error_stream << " at start of file '" << inname << "'";
	  fatal_error (error_stream.str ());
	}
    }

  Stmt *decls = NULL;
  /* Initial symbol table size.  */
  const size_t n_symbols_init = 500;
  htab_t symbol_table = htab_create (n_symbols_init, hash_string_hash, hash_string_eq, symbol_hash_free);
  /* Symbols, in order of appearance.  */
  std::vector<symbol *> symbols;
  symbols.reserve (n_symbols_init);
  Stmt *fns = NULL;

  do
    {
      tok = parse_file (decls, symbol_table, symbols, fns, tok, error_stream);
      if (!tok)
	fatal_error (error_stream.str ());
    }
  while (tok->kind);

  /* Actual number of symbols.  */
  const size_t n_symbols = htab_elements (symbol_table);
  /* All symbols handled.  */
  assert (symbols.size () == n_symbols);

  /* Symbols, in order of output.  */
  std::vector<const symbol *> symbols_out;
  symbols_out.reserve (n_symbols);
  for (symbol *e : symbols)
    if (!symbol_order_deps (symbols_out, e, error_stream))
      fatal_error (error_stream.str ());
  /* All symbols emitted.  */
  assert (symbols_out.size () == n_symbols);

  write_stmts (out_stream, rev_stmts (decls));
  for (const symbol *e : symbols_out)
    write_stmts (out_stream, rev_stmts (e->stmts));
  write_stmts (out_stream, rev_stmts (fns));

  symbols_out.clear ();
  symbols.clear ();
  htab_delete (symbol_table);

  while (!heaps.empty ())
    {
      void *heap = heaps.front ();
      XDELETEVEC (heap);
      heaps.pop_front ();
    }

  XDELETEVEC (tok_to_free);

  XDELETEVEC (input);
}

/* Wait for a process to finish, and exit if a nonzero status is found.  */

static int
collect_wait (const char *prog, struct pex_obj *pex)
{
  int status;

  if (!pex_get_status (pex, 1, &status))
    {
      std::ostringstream error_stream;
      error_stream << "can't get program status: " << strerror (errno);
      fatal_error (error_stream.str ());
    }
  pex_free (pex);

  if (status)
    {
      if (WIFSIGNALED (status))
	{
	  std::ostringstream error_stream;
	  int sig = WTERMSIG (status);
	  error_stream << prog << " terminated with signal " << sig << " [" << strsignal (sig) << "]";
	  if (WCOREDUMP (status))
	    error_stream << ", core dumped";
	  fatal_error (error_stream.str ());
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
      std::ostringstream error_stream;
      error_stream << prog << " returned " << ret << " exit status";
      fatal_error (error_stream.str ());
    }
}


/* Execute a program, and wait for the reply.  */
static void
fork_execute (const char *prog, const char *const *argv)
{
  if (verbose)
    {
      for (const char *const *arg = argv; *arg; ++arg)
	{
	  if (**arg == '\0')
	    std::cerr << " ''";
	  else
	    std::cerr << " " << *arg;
	}
      std::cerr << "\n";
    }

  struct pex_obj *pex = pex_init (0, "nvptx-as", NULL);
  if (pex == NULL)
    {
      std::ostringstream error_stream;
      error_stream << "pex_init failed: " << strerror (errno);
      fatal_error (error_stream.str ());
    }

  int err;
  const char *errmsg;

  errmsg = pex_run (pex, PEX_LAST | PEX_SEARCH,
		    argv[0], const_cast<char *const *>(argv),
		    NULL, NULL, &err);
  if (errmsg != NULL)
    {
      std::ostringstream error_stream;
      error_stream << "error trying to exec '" << argv[0] << "': ";
      error_stream << errmsg;
      if (err != 0)
	error_stream << ": " << strerror (err);
      fatal_error (error_stream.str ());
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

/* Is the NVRTC library usable?

   If successful, has initialized NVRTC library interfacing.  */

static bool
is_libnvrtc_usable (std::ostream &error_stream, bool inhibit)
{
  if (inhibit)
    {
      error_stream << "use of NVRTC library inhibited";
      return false;
    }

  std::ostringstream error_stream_;

  if (!interface_libnvrtc_init (error_stream_))
    {
      error_stream << "NVRTC library not usable: " << error_stream_.str ();
      return false;
    }

  return true;
}

/* Is 'ptxas' usable?  */

static bool
is_ptxas_usable (std::ostream &error_stream, bool inhibit)
{
  if (inhibit)
    {
      error_stream << "use of 'ptxas' inhibited";
      return false;
    }

  if (!program_available ("ptxas"))
    {
      error_stream << "'ptxas' not available.";
      return false;
    }

  return true;
}

ATTRIBUTE_NORETURN static void
usage (std::ostream &out_stream, int status)
{
  out_stream << "\
Usage: nvptx-none-as [option...] [asmfile]\n\
Options:\n\
  -m TARGET             Override target architecture used for verification\n\
                        (default: deduce from input's preamble)\n\
  -o FILE               Write output to FILE\n\
  -v                    Be verbose\n\
  --verify              Verify output for PTX compliance\n\
  [default]             If possible, verify output for PTX compliance\n\
  --no-verify           Don't verify output for PTX compliance\n\
  --help                Print this help and exit\n\
  --version             Print version number and exit\n\
\n\
Report bugs to " << REPORT_BUGS_TO << ".\n";
  exit (status);
}

#define OPT_verify 256
#define OPT_no_verify 257
#define OPT_query 258
#define OPT_inhibit 259

static struct option long_options[] = {
  {"traditional-format",     no_argument, 0,  0 },
  {"save-temps",  no_argument,       0,  0 },
  {"verify", no_argument, 0, OPT_verify },
  {"no-verify", no_argument, 0, OPT_no_verify },
  /* internal-use; undocumented */ {"query", required_argument, 0, OPT_query },
  /* internal-use; undocumented */ {"inhibit", required_argument, 0, OPT_inhibit },
  {"help", no_argument, 0, 'h' },
  {"version", no_argument, 0, 'V' },
  {0,         0,                 0,  0 }
};

int
main (int argc, char **argv)
{
  FILE *in = stdin;
  const char *inname = "{standard input}";
  std::ostream *out_stream = &std::cout;
  verify_mode verify = verify_mode::unset;
  bool inhibit_libnvrtc = false;
  bool inhibit_ptxas = false;
  const char *target_arg_force = NULL;

  int o;
  int option_index = 0;
  while ((o = getopt_long (argc, argv, "o:I:m:v", long_options, &option_index)) != -1)
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
	case 'm':
	  target_arg_force = optarg;
	  break;
	case 'I':
	  /* Ignore include paths.  */
	  break;
	case 'h':
	  usage (std::cout, 0);
	  break;
	case 'V':
	  std::cout << "\
nvptx-none-as " << PKGVERSION << NVPTX_TOOLS_VERSION << "\n\
Copyright (C) 2026 The nvptx-tools Developers\n\
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>\n\
This program is free software; you may redistribute it under the terms of\n\
the GNU General Public License version 3 or later.\n\
This program has absolutely no warranty.\n";
	  exit (0);
	case OPT_verify:
	  verify = verify_mode::yes;
	  break;
	case OPT_no_verify:
	  verify = verify_mode::no;
	  break;
	case OPT_query:
	  {
	    std::ostringstream error_stream;
	    assert (optarg);
	    if (!strcmp (optarg, "libnvrtc_usable"))
	      {
		if (!is_libnvrtc_usable (error_stream, false))
		  fatal_error (error_stream.str ());
		exit (0);
	      }
	    else if (!strcmp (optarg, "libnvrtc_supported_archs"))
	      {
		if (!interface_libnvrtc_init (error_stream))
		  fatal_error (error_stream.str ());

		int n_archs, *archs;
		if (!interface_libnvrtc_supported_archs (error_stream, &n_archs, &archs))
		  fatal_error (error_stream.str ());

		for (size_t i = 0; i < (size_t) n_archs; ++i)
		  std::cout << archs[i] << '\n';

		delete[] archs;

		exit (0);
	      }
	    else if (!strcmp (optarg, "ptxas_usable"))
	      {
		if (!is_ptxas_usable (error_stream, false))
		  fatal_error (error_stream.str ());
		exit (0);
	      }
	    else
	      {
		error_stream << "invalid '--" << long_options[option_index].name << "=" << optarg << "'";
		fatal_error (error_stream.str ());
	      }
	  }
	  break;
	case OPT_inhibit:
	  {
	    std::ostringstream error_stream;
	    assert (optarg);
	    if (!strcmp (optarg, "libnvrtc"))
	      inhibit_libnvrtc = true;
	    else if (!strcmp (optarg, "ptxas"))
	      inhibit_ptxas = true;
	    else
	      {
		error_stream << "invalid '--" << long_options[option_index].name << "=" << optarg << "'";
		fatal_error (error_stream.str ());
	      }
	  }
	  break;
	default:
	  usage (std::cerr, 1);
	  break;
	}
    }

  if (optind + 1 < argc)
    fatal_error ("too many input files specified");

  if (outname)
    out_stream = new std::ofstream (outname);
  if (out_stream->fail ())
    {
      std::ostringstream error_stream;
      error_stream << "cannot open '" << outname << "'";
      fatal_error (error_stream.str ());
    }

  if (argc > optind)
    {
      inname = argv[optind];
      in = fopen (inname, "r");
    }
  if (!in)
    fatal_error ("cannot open input ptx file");

  process (in, *out_stream, verify, inname);

  if (in != stdin)
    {
      fclose (in);
      in = NULL;
    }
  if (out_stream != &std::cout)
    {
      delete out_stream;
      out_stream = NULL;
    }

  assert (verify == verify_mode::unset
	  || verify == verify_mode::no
	  || verify == verify_mode::yes);
  if (outname == NULL)
    /* We don't have an output file; skip verification.  */
    verify = verify_mode::no;
  if (verify == verify_mode::unset
      || verify == verify_mode::yes)
    {
      /* Determine what verification to use.  In order of preference:
	  1. 'ptxas' with NVRTC library,
	  2. 'ptxas' without NVRTC library,
	  3. none/error.  */

      bool ptxas_usable = false;
      bool libnvrtc_usable = false;

      if (/*TODO*/ true)
	{
	  std::ostringstream error_stream_ptxas;
	  ptxas_usable = is_ptxas_usable (error_stream_ptxas, inhibit_ptxas);
	  if (verbose && !ptxas_usable)
	    std::cerr << error_stream_ptxas.str () << '\n';

	  if (ptxas_usable)
	    {
	      std::ostringstream error_stream_libnvrtc;
	      libnvrtc_usable = is_libnvrtc_usable (error_stream_libnvrtc, inhibit_libnvrtc);
	      if (verbose && !libnvrtc_usable)
		std::cerr << error_stream_libnvrtc.str () << '\n';
	    }
	}

      if (libnvrtc_usable && ptxas_usable)
	verify = verify_mode::ptxas_libnvrtc;
      else if (ptxas_usable)
	verify = verify_mode::ptxas;
      else
	{
	  const char *msg = "not able to verify output";
	  if (verify == verify_mode::yes)
	    fatal_error (msg);
	  else
	    {
	      assert (verify == verify_mode::unset);
	      if (verbose)
		std::cerr << msg << '\n';
	      verify = verify_mode::no;
	    }
	}
    }
  else
    assert (verify == verify_mode::no);
  assert (verify == verify_mode::no
	  || verify == verify_mode::ptxas_libnvrtc
	  || verify == verify_mode::ptxas);

  if (verify != verify_mode::no)
    {
      const char *target_arg;
      bool free_target_arg = false;
      if (target_arg_force)
	target_arg = target_arg_force;
      else
	{
	  assert (preamble_target_arg);

	  /* We'd like to verify per what we deduced from from the input's
	     preamble.  For 'ptxas', the default '--gpu-name' may
	     not be sufficient for what is requested in the '.target' directive
	     in the input's preamble:

	         ptxas fatal   : SM version specified by .target is higher than default SM version assumed
	  */
	  target_arg = preamble_target_arg;

	  if (verify == verify_mode::ptxas_libnvrtc)
	    {
	      std::ostringstream error_stream;

	      if (!interface_libnvrtc_init (error_stream))
		assert (!"unreachable");

	      int n_archs, *archs;
	      if (!interface_libnvrtc_supported_archs (error_stream, &n_archs, &archs))
		fatal_error (error_stream.str ());

	      int arch, n, r;
	      r = sscanf (target_arg, "sm_%d%n", &arch, &n);
	      if (r == EOF
		  || r != 1
		  || target_arg[n] != '\0')
		{
		  error_stream << "unsupported '.target " << target_arg << "' directive in preamble";
		  fatal_error (error_stream.str ());
		}
	      /* Per tokenization/'verify_preamble'.  */
	      assert (arch >= 0);

	      /* If necessary, raise 'arch' to the minimum version
		 supported.  */
	      if (arch < archs[0])
		{
		  if (verbose)
		    std::cerr << "Verifying " << target_arg << " code";
		  target_arg = xasprintf ("sm_%d", archs[0]);
		  free_target_arg = true;
		  if (verbose)
		    std::cerr << " with " << target_arg << " code generation.\n";
		}
	      else
		{
		  /* Assume that 'target_arg' ('sm_[arch]') is supported, that
		     is, 'arch' exists in 'archs'.  In case it doesn't, 'ptxas'
		     is going to error out.  */
		}

	      delete[] archs;
	    }
	  else if (verify == verify_mode::ptxas)
	    {
	      /* In CUDA 11.0, "Support for Kepler 'sm_30' and 'sm_32'
		 architecture based products is dropped", and in CUDA 12.0,
		 "Kepler architecture support is removed" (that is, sm_35,
		 sm_37), and these may no longer be specified in '--gpu-name'
		 of 'ptxas':

		     ptxas fatal   : Value 'sm_30' is not defined for option 'gpu-name'

		     ptxas fatal   : Value 'sm_32' is not defined for option 'gpu-name'

		     ptxas fatal   : Value 'sm_35' is not defined for option 'gpu-name'

		     ptxas fatal   : Value 'sm_37' is not defined for option 'gpu-name'

		 ..., but we need to continue supporting GCC emitting
		 '.target sm_30' code, for example.  */
	      if ((strcmp ("sm_30", target_arg) == 0)
		  || (strcmp ("sm_32", target_arg) == 0)
		  || (strcmp ("sm_35", target_arg) == 0)
		  || (strcmp ("sm_37", target_arg) == 0))
		{
		  if (verbose)
		    std::cerr << "Verifying " << target_arg << " code";
		  target_arg = "sm_50";
		  if (verbose)
		    std::cerr << " with " << target_arg << " code generation.\n";
		}
	    }
	  else
	    assert (!"unreachable");
	}

      if (verify == verify_mode::ptxas_libnvrtc
	  || verify == verify_mode::ptxas)
	{
	  const char *const ptxas_argv[] = {
	    "ptxas",
	    "-c",
	    "-o",
	    "/dev/null",
	    outname,
	    "--gpu-name",
	    target_arg,
	    "-O0",
	    NULL,
	  };
	  fork_execute (ptxas_argv[0], ptxas_argv);
	}
      else
	assert (!"unreachable");

      if (free_target_arg)
	free (const_cast<char *>(target_arg));
    }

  free (preamble_target_arg);

  return 0;
}
