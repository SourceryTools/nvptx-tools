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

/* Not all <cuda.h> interfaces are encapsulated.  */

#ifndef INTERFACE_LIBCUDA_H
#define INTERFACE_LIBCUDA_H 1

#ifndef NVPTX_RUN_INCLUDE_SYSTEM_CUDA_H
# include "cuda/cuda.h"
#else
# include <cuda.h>

/* On systems where installed NVIDIA driver is newer than CUDA Toolkit,
   libcuda.so may have these functions even though <cuda.h> does not.  */

extern "C" CUresult cuGetErrorName (CUresult, const char **);
extern "C" CUresult cuGetErrorString (CUresult, const char **);
#endif

#define DO_PRAGMA(x) _Pragma (#x)

#ifndef NVPTX_RUN_LINK_LIBCUDA

extern struct libcuda_s {
# define LIBCUDA_SYMBOL(name) \
  __typeof (::name) *name;
# define LIBCUDA_SYMBOL_MAYBE_NULL(name) \
  LIBCUDA_SYMBOL (name)
# include "interface-libcuda-symbols.def"
# undef LIBCUDA_SYMBOL
# undef LIBCUDA_SYMBOL_MAYBE_NULL
} libcuda_s;
# define LIBCUDA_SYMBOL_PREFIX libcuda_s.

#else /* NVPTX_RUN_LINK_LIBCUDA */

# define LIBCUDA_SYMBOL_PREFIX

# define LIBCUDA_SYMBOL(name)
# define LIBCUDA_SYMBOL_MAYBE_NULL(name) DO_PRAGMA (weak name)
# include "interface-libcuda-symbols.def"
# undef LIBCUDA_SYMBOL_MAYBE_NULL
# undef LIBCUDA_SYMBOL

#endif /* NVPTX_RUN_LINK_LIBCUDA */

#define LIBCUDA_SYMBOL_CALL_NOCHECK(FN, ...) \
  LIBCUDA_SYMBOL_PREFIX FN (__VA_ARGS__)
#define LIBCUDA_SYMBOL_EXISTS(FN) \
  LIBCUDA_SYMBOL_PREFIX FN


#include <sstream>

extern bool interface_libcuda_init (std::ostream &error_stream);

#endif /* INTERFACE_LIBCUDA_H */
