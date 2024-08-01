/* Estimate the length of the string generated by a vprintf-like
   function.  Used by vasprintf and xvasprintf.
   Copyright (C) 1994-2024 Free Software Foundation, Inc.

This file is part of the libiberty library.
Libiberty is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

Libiberty is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with libiberty; see the file COPYING.LIB.  If not, write
to the Free Software Foundation, Inc., 51 Franklin Street - Fifth
Floor, Boston, MA 02110-1301, USA.  */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <ansidecl.h>
#include <stdarg.h>
#if !defined (va_copy) && defined (__va_copy)
# define va_copy(d,s)  __va_copy((d),(s))
#endif
#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#else
extern unsigned long strtoul ();
#endif
#include "libiberty.h"

int
libiberty_vprintf_buffer_size (const char *format, va_list args)
{
  const char *p = format;
  /* Add one to make sure that it is never zero, which might cause malloc
     to return NULL.  */
  int total_width = strlen (format) + 1;
  va_list ap;

#ifdef va_copy
  va_copy (ap, args);
#else
  memcpy ((void *) &ap, (void *) &args, sizeof (va_list));
#endif

  while (*p != '\0')
    {
      if (*p++ == '%')
	{
	  int prec = 0;
	  while (strchr ("-+ #0", *p))
	    ++p;
	  if (*p == '*')
	    {
	      ++p;
	      total_width += abs (va_arg (ap, int));
	    }
	  else
	    total_width += strtoul (p, (char **) &p, 10);
	  if (*p == '.')
	    {
	      ++p;
	      if (*p == '*')
		{
		  ++p;
		  total_width += abs (va_arg (ap, int));
		}
	      else
	      total_width += strtoul (p, (char **) &p, 10);
	    }
	  do
	    {
	      switch (*p)
		{
		case 'h':
		  ++p;
		  continue;
		case 'l':
		case 'L':
		  ++prec;
		  ++p;
		  continue;
		case 'z':
		  prec = 3;
		  ++p;
		  continue;
		case 't':
		  prec = 4;
		  ++p;
		  continue;
#ifdef _WIN32
		case 'I':
		  if (p[1] == '6' && p[2] == '4')
		    {
		      prec = 2;
		      p += 3;
		      continue;
		    }
		  break;
#endif
		default:
		  break;
		}
	      break;
	    }
	  while (1);

	  /* Should be big enough for any format specifier except %s and floats.  */
	  total_width += 30;
	  switch (*p)
	    {
	    case 'd':
	    case 'i':
	    case 'o':
	    case 'u':
	    case 'x':
	    case 'X':
	      switch (prec)
		{
		case 0: (void) va_arg (ap, int); break;
		case 1: (void) va_arg (ap, long int); break;
		case 2: (void) va_arg (ap, long long int); break;
		case 3: (void) va_arg (ap, size_t); break;
		case 4: (void) va_arg (ap, ptrdiff_t); break;
		}
	      break;
	    case 'c':
	      (void) va_arg (ap, int);
	      break;
	    case 'f':
	    case 'e':
	    case 'E':
	    case 'g':
	    case 'G':
	      if (!prec)
		{
		  (void) va_arg (ap, double);
		  /* Since an ieee double can have an exponent of 308, we'll
		     make the buffer wide enough to cover the gross case. */
		  total_width += 308;
		}
	      else
		{
		  (void) va_arg (ap, long double);
		  total_width += 4932;
		}
	      break;
	    case 's':
	      total_width += strlen (va_arg (ap, char *));
	      break;
	    case 'p':
	    case 'n':
	      (void) va_arg (ap, char *);
	      break;
	    }
	  p++;
	}
    }
#ifdef va_copy
  va_end (ap);
#endif
  return total_width;
}
