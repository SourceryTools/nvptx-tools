/* Interface to the NVIDIA/CUDA nvJitLink library.

   Copyright (C) 2026 BayLibre

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

#include "interface-libnvjitlink.h"
#include "interface-libnvrtc.h"

#include <string>
#include <cstring>

#ifndef NVPTX_AS_INCLUDE_SYSTEM_NVJITLINK_H
# include "cuda/nvJitLink.h"
#else
# include <nvJitLink.h>
#endif

#define DO_PRAGMA(x) _Pragma (#x)

#ifndef NVPTX_AS_LINK_LIBNVJITLINK

static struct libnvjitlink_s {
# define LIBNVJITLINK_SYMBOL(name) \
  __typeof (::name) *name;
# define LIBNVJITLINK_SYMBOL_MAYBE_NULL(name) \
  LIBNVJITLINK_SYMBOL (name)
# include "interface-libnvjitlink-symbols.def"
# undef LIBNVJITLINK_SYMBOL
# undef LIBNVJITLINK_SYMBOL_MAYBE_NULL
} libnvjitlink_s;
# define LIBNVJITLINK_SYMBOL_PREFIX libnvjitlink_s.

# ifdef HAVE_DLFCN_H

#  include <dlfcn.h>

/* -1 if libnvjitlink_init has not been called yet, false
   if it has been and failed, true if it has been and succeeded.  */
static signed char libnvjitlink_inited = -1;

/* Dynamically load the NVIDIA/CUDA nvJitLink library and initialize function
   pointers, return false if unsuccessful, true if successful.  */
static bool
libnvjitlink_init (std::ostream &error_stream)
{
  if (libnvjitlink_inited != -1)
    return libnvjitlink_inited;
  const char *libnvjitlink = "libnvJitLink.so";
  void *h = dlopen (libnvjitlink, RTLD_LAZY);
  libnvjitlink_inited = false;
  if (h == NULL)
    {
      error_stream << "couldn't dlopen " << libnvjitlink;
      return false;
    }

#  define LIBNVJITLINK_SYMBOL(name) LIBNVJITLINK_SYMBOL_1 (name, false)
#  define LIBNVJITLINK_SYMBOL_MAYBE_NULL(name) LIBNVJITLINK_SYMBOL_1 (name, true)
#  define LIBNVJITLINK_SYMBOL_1(name, allow_null) \
  libnvjitlink_s.name = (__typeof (name) *) dlsym (h, #name); \
  if (!allow_null && libnvjitlink_s.name == NULL) \
    { \
      error_stream << "couldn't find " << #name << " in " << libnvjitlink; \
      return false; \
    }
#  include "interface-libnvjitlink-symbols.def"
#  undef LIBNVJITLINK_SYMBOL
#  undef LIBNVJITLINK_SYMBOL_1
#  undef LIBNVJITLINK_SYMBOL_MAYBE_NULL

  libnvjitlink_inited = true;
  return true;
}

# else /* !HAVE_DLFCN_H */

static bool
libnvjitlink_init (std::ostream &error_stream)
{
  error_stream << "Don't know how to load dynamic shared objects (nvJitLink library)";
  return false;
}

# endif /* HAVE_DLFCN_H */

#else /* NVPTX_AS_LINK_LIBNVJITLINK */

# define LIBNVJITLINK_SYMBOL_PREFIX

# define LIBNVJITLINK_SYMBOL(name)
# define LIBNVJITLINK_SYMBOL_MAYBE_NULL(name) DO_PRAGMA (weak name)
# include "interface-libnvjitlink-symbols.def"
# undef LIBNVJITLINK_SYMBOL_MAYBE_NULL
# undef LIBNVJITLINK_SYMBOL

static bool
libnvjitlink_init (std::ostream &error_stream)
{
  (void) error_stream;
  return true;
}

#endif /* NVPTX_AS_LINK_LIBNVJITLINK */

# define LIBNVJITLINK_SYMBOL_CALL_NOCHECK(FN, ...) \
  LIBNVJITLINK_SYMBOL_PREFIX FN (__VA_ARGS__)
# define LIBNVJITLINK_SYMBOL_EXISTS(FN) \
  LIBNVJITLINK_SYMBOL_PREFIX FN


bool
interface_libnvjitlink_init (std::ostream &error_stream)
{
  if (!libnvjitlink_init (error_stream))
    return false;

  return true;
}

bool
interface_libnvjitlink_version (std::ostream &error_stream, unsigned int *major, unsigned int *minor)
{
  nvJitLinkResult r;

  r = LIBNVJITLINK_SYMBOL_CALL_NOCHECK (nvJitLinkVersion, major, minor);
  if (r != NVJITLINK_SUCCESS)
    {
      error_stream << "nvJitLinkVersion failed: " << (int) r;
      return false;
    }

  return true;
}

bool
interface_libnvjitlink_verify (std::ostream &error_stream, const char *filename, size_t n_link_options, const char **link_options)
{
  nvJitLinkResult r;

  nvJitLinkHandle h;
  r = LIBNVJITLINK_SYMBOL_CALL_NOCHECK (nvJitLinkCreate, &h, n_link_options, link_options);
  if (r != NVJITLINK_SUCCESS)
    {
      error_stream << "nvJitLinkCreate failed: " << (int) r;
      goto error_out;
    }

  r = LIBNVJITLINK_SYMBOL_CALL_NOCHECK (nvJitLinkAddFile, h, NVJITLINK_INPUT_PTX, filename);
  if (r != NVJITLINK_SUCCESS)
    {
      error_stream << "nvJitLinkAddFile failed: " << (int) r;
      goto error_out;
    }

  /* "Link".  With a single input, and '-r' in 'link_options' (see
     'nvptx-as.cc:is_libnvjitlink_usable'), this is effectively just
     compilation.  */
  r = LIBNVJITLINK_SYMBOL_CALL_NOCHECK (nvJitLinkComplete, h);
  if (r != NVJITLINK_SUCCESS)
    {
      error_stream << "nvJitLinkComplete failed: " << (int) r;
      goto error_out;
    }

  /* Throw away the resulting cubin.  */

  r = LIBNVJITLINK_SYMBOL_CALL_NOCHECK (nvJitLinkDestroy, &h);
  if (r != NVJITLINK_SUCCESS)
    {
      error_stream << "nvJitLinkDestroy failed: " << (int) r;
      goto error_out;
    }

  return true;

 error_out:
  error_stream << '\n';
  size_t error_log_size;
  r = LIBNVJITLINK_SYMBOL_CALL_NOCHECK (nvJitLinkGetErrorLogSize, h, &error_log_size);
  if (r != NVJITLINK_SUCCESS)
    {
      error_stream << "nvJitLinkGetErrorLogSize failed: " << (int) r;
      return false;
    }
  if (error_log_size > 0)
    {
      std::string error_log;
      error_log.resize (error_log_size);
      r = LIBNVJITLINK_SYMBOL_CALL_NOCHECK (nvJitLinkGetErrorLog, h, &error_log[0]);
      if (r != NVJITLINK_SUCCESS)
	{
	  error_stream << "nvJitLinkGetErrorLog failed: " << (int) r;
	  return false;
	}
      /* Adjust the 'std::string' instance to what the C API actually has written.  */
      error_log.resize (std::strlen (error_log.c_str ()));

      /* Strip trailing line breaks.  */
      while (!error_log.empty ()
	     && error_log.back () == '\n')
	error_log.pop_back ();

      error_stream << error_log;
    }
  else
    error_stream << "(empty error log)";

  return false;
}
