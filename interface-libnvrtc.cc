/* Interface to the NVIDIA/CUDA NVRTC library.

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

#include "interface-libnvrtc.h"

#include <stdlib.h>

#ifndef NVPTX_AS_INCLUDE_SYSTEM_NVRTC_H
# include "cuda/nvrtc.h"
#else
# include <nvrtc.h>
#endif

#define DO_PRAGMA(x) _Pragma (#x)

#ifndef NVPTX_AS_LINK_LIBNVRTC

static struct libnvrtc_s {
# define LIBNVRTC_SYMBOL(name) \
  __typeof (::name) *name;
# define LIBNVRTC_SYMBOL_MAYBE_NULL(name) \
  LIBNVRTC_SYMBOL (name)
# include "interface-libnvrtc-symbols.def"
# undef LIBNVRTC_SYMBOL
# undef LIBNVRTC_SYMBOL_MAYBE_NULL
} libnvrtc_s;
# define LIBNVRTC_SYMBOL_PREFIX libnvrtc_s.

# ifdef HAVE_DLFCN_H

#  include <dlfcn.h>

/* -1 if libnvrtc_init has not been called yet, false
   if it has been and failed, true if it has been and succeeded.  */
static signed char libnvrtc_inited = -1;

/* Dynamically load the NVIDIA/CUDA NVRTC library and initialize function
   pointers, return false if unsuccessful, true if successful.  */
static bool
libnvrtc_init (std::ostream &error_stream)
{
  if (libnvrtc_inited != -1)
    return libnvrtc_inited;
  const char *libnvrtc = "libnvrtc.so";
  void *h = dlopen (libnvrtc, RTLD_LAZY);
  libnvrtc_inited = false;
  if (h == NULL)
    {
      error_stream << "couldn't dlopen " << libnvrtc;
      return false;
    }

#  define LIBNVRTC_SYMBOL(name) LIBNVRTC_SYMBOL_1 (name, false)
#  define LIBNVRTC_SYMBOL_MAYBE_NULL(name) LIBNVRTC_SYMBOL_1 (name, true)
#  define LIBNVRTC_SYMBOL_1(name, allow_null) \
  libnvrtc_s.name = (__typeof (name) *) dlsym (h, #name); \
  if (!allow_null && libnvrtc_s.name == NULL) \
    { \
      error_stream << "couldn't find " << #name << " in " << libnvrtc; \
      return false; \
    }
#  include "interface-libnvrtc-symbols.def"
#  undef LIBNVRTC_SYMBOL
#  undef LIBNVRTC_SYMBOL_1
#  undef LIBNVRTC_SYMBOL_MAYBE_NULL

  libnvrtc_inited = true;
  return true;
}

# else /* !HAVE_DLFCN_H */

static bool
libnvrtc_init (std::ostream &error_stream)
{
  error_stream << "Don't know how to load dynamic shared objects (NVRTC library)";
  return false;
}

# endif /* HAVE_DLFCN_H */

#else /* NVPTX_AS_LINK_LIBNVRTC */

# define LIBNVRTC_SYMBOL_PREFIX

# define LIBNVRTC_SYMBOL(name)
# define LIBNVRTC_SYMBOL_MAYBE_NULL(name) DO_PRAGMA (weak name)
# include "interface-libnvrtc-symbols.def"
# undef LIBNVRTC_SYMBOL_MAYBE_NULL
# undef LIBNVRTC_SYMBOL

static bool
libnvrtc_init (std::ostream &error_stream)
{
  (void) error_stream;
  return true;
}

#endif /* NVPTX_AS_LINK_LIBNVRTC */

#define LIBNVRTC_SYMBOL_CALL_NOCHECK(FN, ...) \
  LIBNVRTC_SYMBOL_PREFIX FN (__VA_ARGS__)

#define LIBNVRTC_SYMBOL_EXISTS(FN) \
  LIBNVRTC_SYMBOL_PREFIX FN


bool
interface_libnvrtc_init (std::ostream &error_stream)
{
  if (!libnvrtc_init (error_stream))
    return false;

  return true;
}

bool
interface_libnvrtc_version (std::ostream &error_stream, int *major, int *minor)
{
  nvrtcResult r;

  r = LIBNVRTC_SYMBOL_CALL_NOCHECK (nvrtcVersion, major, minor);
  if (r != NVRTC_SUCCESS)
    {
      error_stream << "nvrtcVersion failed: " << (int) r;
      return false;
    }

  return true;
}

bool
interface_libnvrtc_supported_archs (std::ostream &error_stream, int *n, int **archs)
{
  nvrtcResult r;

  r = LIBNVRTC_SYMBOL_CALL_NOCHECK (nvrtcGetNumSupportedArchs, n);
  if (r != NVRTC_SUCCESS)
    {
      error_stream << "nvrtcGetNumSupportedArchs failed: " << (int) r;
      return false;
    }

  *archs = new int[*n];
  r = LIBNVRTC_SYMBOL_CALL_NOCHECK (nvrtcGetSupportedArchs, *archs);
  if (r != NVRTC_SUCCESS)
    {
      error_stream << "nvrtcGetSupportedArchs failed: " << (int) r;
      delete[] *archs;
      return false;
    }

  return true;
}
