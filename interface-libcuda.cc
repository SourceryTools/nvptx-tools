/* Interface to the NVIDIA/CUDA Driver library.

   Copyright (C) 2014, 2015 Mentor Graphics
   Copyright (C) 2016 Ivannikov Institute for System Programming of the Russian Academy of Sciences
   Copyright (C) 2022, 2023 Siemens
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

#include "interface-libcuda.h"

#ifndef NVPTX_RUN_LINK_LIBCUDA

/*static*/ struct libcuda_s
libcuda_s;

# ifdef HAVE_DLFCN_H

#  include <dlfcn.h>

/* -1 if libcuda_init has not been called yet, false
   if it has been and failed, true if it has been and succeeded.  */
static signed char libcuda_inited = -1;

/* Dynamically load the CUDA runtime library and initialize function
   pointers, return false if unsuccessful, true if successful.  */
static bool
libcuda_init (std::ostream &error_stream)
{
  if (libcuda_inited != -1)
    return libcuda_inited;
  const char *libcuda = "libcuda.so.1";
  void *h = dlopen (libcuda, RTLD_LAZY);
  libcuda_inited = false;
  if (h == NULL)
    {
      error_stream << "couldn't dlopen " << libcuda;
      return false;
    }

#  define LIBCUDA_SYMBOL(name) LIBCUDA_SYMBOL_1 (name, false)
#  define LIBCUDA_SYMBOL_MAYBE_NULL(name) LIBCUDA_SYMBOL_1 (name, true)
#  define LIBCUDA_SYMBOL_1(name, allow_null) \
  libcuda_s.name = (__typeof (name) *) dlsym (h, #name); \
  if (!allow_null && libcuda_s.name == NULL) \
    { \
      error_stream << "couldn't find " << #name << " in " << libcuda; \
      return false; \
    }
#  include "interface-libcuda-symbols.def"
#  undef LIBCUDA_SYMBOL
#  undef LIBCUDA_SYMBOL_1
#  undef LIBCUDA_SYMBOL_MAYBE_NULL

  libcuda_inited = true;
  return true;
}

# else /* !HAVE_DLFCN_H */

#  error "Don't know how to load dynamic shared objects."

# endif /* HAVE_DLFCN_H */

#else /* NVPTX_RUN_LINK_LIBCUDA */

static bool
libcuda_init (std::ostream &error_stream)
{
  (void) error_stream;
  return true;
}

#endif /* NVPTX_RUN_LINK_LIBCUDA */


bool
interface_libcuda_init (std::ostream &error_stream)
{
  if (!libcuda_init (error_stream))
    return false;

  return true;
}
