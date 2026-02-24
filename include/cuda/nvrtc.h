/* NVIDIA/CUDA NVRTC library API

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

   This header provides parts of the NVIDIA/CUDA NVRTC library API, without
   having to rely on the proprietary CUDA Toolkit.  */

#ifndef GCC_NVRTC_H
#define GCC_NVRTC_H

#ifdef __cplusplus
extern "C" {
#endif

/* The following API description corresponds to information available on
   <https://docs.nvidia.com/cuda/archive/11.2.0/nvrtc/>.  */

typedef enum {
  NVRTC_SUCCESS = 0,
} nvrtcResult;

extern nvrtcResult nvrtcGetNumSupportedArchs (int *);
extern nvrtcResult nvrtcGetSupportedArchs (int *);
extern nvrtcResult nvrtcVersion (int *, int *);

#ifdef __cplusplus
}
#endif

#endif /* GCC_NVRTC_H */
