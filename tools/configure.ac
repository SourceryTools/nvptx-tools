AC_INIT(nvptxtools)
AC_PREREQ(2.64)
AC_CONFIG_SRCDIR(nvptx-run.c)

AC_PROG_CC
AC_PROG_CXX

gcc_AC_PROG_LN_S
#AC_PROG_RANLIB
gcc_AC_PROG_INSTALL

AC_PATH_PROG(RANLIB, ranlib)
AC_PATH_PROG(AR, ar)

# Look for the CUDA driver package.
CUDA_DRIVER_CPPFLAGS=
CUDA_DRIVER_LDFLAGS=
AC_ARG_WITH(cuda-driver,
        [AS_HELP_STRING([--with-cuda-driver=PATH],
                [specify prefix directory for installed CUDA driver package.
                 Equivalent to --with-cuda-driver-include=PATH/include
                 plus --with-cuda-driver-lib=PATH/lib])])
AC_ARG_WITH(cuda-driver-include,
        [AS_HELP_STRING([--with-cuda-driver-include=PATH],
                [specify directory for installed CUDA driver include files])])
AC_ARG_WITH(cuda-driver-lib,
        [AS_HELP_STRING([--with-cuda-driver-lib=PATH],
                [specify directory for the installed CUDA driver library])])
if test "x$with_cuda_driver" != x; then
  CUDA_DRIVER_CPPFLAGS=-I$with_cuda_driver/include
  CUDA_DRIVER_LDFLAGS="-L$with_cuda_driver/lib"
fi
if test "x$with_cuda_driver_include" != x; then
  CUDA_DRIVER_CPPFLAGS=-I$with_cuda_driver_include
fi
if test "x$with_cuda_driver_lib" != x; then
  CUDA_DRIVER_LDFLAGS="-L$with_cuda_driver_lib"
fi

CXXFLAGS="$CXXFLAGS $CUDA_DRIVER_CPPFLAGS"
LDFLAGS="$LDFLAGS $CUDA_DRIVER_LDFLAGS"

AC_MSG_CHECKING([for extra programs to build requiring -lcudart])
NVPTX_RUN=
cudart_save_CFLAGS=$CFLAGS
CFLAGS="$CUDA_DRIVER_CPPFLAGS $CFLAGS"
cudart_save_LDFLAGS=$LDFLAGS
LDFLAGS="$CUDA_DRIVER_LDFLAGS $LDFLAGS"
cudart_save_LIBS=$LIBS
LIBS="$LIBS -lcuda -lcudart"
AC_LINK_IFELSE([AC_LANG_PROGRAM(
[#include "cuda.h"
#include <cuda_runtime.h>
],
[const char *p; CUresult r; cuGetErrorString (r, &p); cudaDeviceReset (); ])],
[NVPTX_RUN=nvptx-none-run])
AC_MSG_RESULT($NVPTX_RUN)

CFLAGS=$cudart_save_CFLAGS
LDFLAGS=$cudart_save_LDFLAGS
LIBS=$cudart_save_LIBS

AC_SUBST(NVPTX_RUN)
AC_CONFIG_FILES(Makefile)
AC_OUTPUT
