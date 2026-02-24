/* NVIDIA/CUDA nvJitLink library API

   Copyright (C) 2026 Free Software Foundation, Inc.

   This file is part of GCC.

   GCC is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   GCC is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   Under Section 7 of GPL version 3, you are granted additional
   permissions described in the GCC Runtime Library Exception, version
   3.1, as published by the Free Software Foundation.

   You should have received a copy of the GNU General Public License and
   a copy of the GCC Runtime Library Exception along with this program;
   see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
   <http://www.gnu.org/licenses/>.

   This header provides parts of the NVIDIA/CUDA nvJitLink library API, without
   having to rely on the proprietary CUDA Toolkit.  */

#ifndef GCC_NVJITLINK_H
#define GCC_NVJITLINK_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The following API description corresponds to information available on
   <https://docs.nvidia.com/cuda/archive/12.3.0/nvjitlink/>.  */

typedef enum {
  NVJITLINK_SUCCESS = 0,
} nvJitLinkResult;

typedef enum {
  NVJITLINK_INPUT_PTX = 2,
} nvJitLinkInputType;

typedef struct nvJitLink *nvJitLinkHandle;

extern nvJitLinkResult nvJitLinkAddFile (nvJitLinkHandle, nvJitLinkInputType, const char *);
extern nvJitLinkResult nvJitLinkComplete (nvJitLinkHandle);
extern nvJitLinkResult nvJitLinkCreate (nvJitLinkHandle *, uint32_t, const char **);
extern nvJitLinkResult nvJitLinkDestroy (nvJitLinkHandle *);
extern nvJitLinkResult nvJitLinkGetErrorLog (nvJitLinkHandle, char *);
extern nvJitLinkResult nvJitLinkGetErrorLogSize (nvJitLinkHandle, size_t *);
extern nvJitLinkResult nvJitLinkVersion (unsigned int *, unsigned int *);

#ifdef __cplusplus
}
#endif

#endif /* GCC_NVJITLINK_H */
