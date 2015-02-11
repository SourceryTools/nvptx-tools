AC_INIT([nvptx-tools])
ACX_PKGVERSION([nvptx-tools])
ACX_BUGURL([https://github.com/MentorEmbedded/nvptx-tools/issues])
AC_PREREQ([2.64])
AC_CONFIG_SRCDIR([nvptx-as.c])

AC_PROG_CC
AC_PROG_CXX

gcc_AC_PROG_LN_S
#AC_PROG_RANLIB
gcc_AC_PROG_INSTALL

AC_PATH_PROG(RANLIB, ranlib)
AC_PATH_PROG(AR, ar)

# Look for the CUDA driver package.
CUDA_DRIVER_CPPFLAGS=
AC_SUBST([CUDA_DRIVER_CPPFLAGS])
CUDA_DRIVER_LDFLAGS=
AC_SUBST([CUDA_DRIVER_LDFLAGS])
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

# Look for the CUDA runtime package.
CUDA_RUNTIME_CPPFLAGS=
AC_SUBST([CUDA_RUNTIME_CPPFLAGS])
CUDA_RUNTIME_LDFLAGS=
AC_SUBST([CUDA_RUNTIME_LDFLAGS])
AC_ARG_WITH(cuda-runtime,
        [AS_HELP_STRING([--with-cuda-runtime=PATH],
                [specify prefix directory for installed CUDA runtime package.
                 Equivalent to --with-cuda-runtime-include=PATH/include
                 plus --with-cuda-runtime-lib=PATH/lib])])
AC_ARG_WITH(cuda-runtime-include,
        [AS_HELP_STRING([--with-cuda-runtime-include=PATH],
                [specify directory for installed CUDA runtime include files])])
AC_ARG_WITH(cuda-runtime-lib,
        [AS_HELP_STRING([--with-cuda-runtime-lib=PATH],
                [specify directory for the installed CUDA runtime library])])
if test "x$with_cuda_runtime" != x; then
  CUDA_RUNTIME_CPPFLAGS=-I$with_cuda_runtime/include
  CUDA_RUNTIME_LDFLAGS="-L$with_cuda_runtime/lib"
fi
if test "x$with_cuda_runtime_include" != x; then
  CUDA_RUNTIME_CPPFLAGS=-I$with_cuda_runtime_include
fi
if test "x$with_cuda_runtime_lib" != x; then
  CUDA_RUNTIME_LDFLAGS="-L$with_cuda_runtime_lib"
fi

save_CPPFLAGS=$CPPFLAGS
CPPFLAGS="$CUDA_DRIVER_CPPFLAGS $CUDA_RUNTIME_CPPFLAGS $CPPFLAGS"
save_LDFLAGS=$LDFLAGS
LDFLAGS="$CUDA_DRIVER_LDFLAGS $CUDA_RUNTIME_LDFLAGS $LDFLAGS"
save_LIBS=$LIBS
LIBS="$LIBS -lcuda -lcudart"

AC_CHECK_FUNCS([[cuGetErrorName] [cuGetErrorString]])
AC_CHECK_DECLS([[cuGetErrorName], [cuGetErrorString]],
  [], [], [[#include <cuda.h>]])

AC_MSG_CHECKING([for extra programs to build requiring -lcudart])
NVPTX_RUN=
AC_LINK_IFELSE([AC_LANG_PROGRAM(
  [#include <cuda_runtime.h>],
  [cudaDeviceReset ();])],
  [NVPTX_RUN=nvptx-none-run])
AC_MSG_RESULT($NVPTX_RUN)
AC_SUBST(NVPTX_RUN)

CPPFLAGS=$save_CPPFLAGS
LDFLAGS=$save_LDFLAGS
LIBS=$save_LIBS

AC_CONFIG_SUBDIRS([libiberty])
AC_CONFIG_FILES([Makefile dejagnu.exp])
AC_OUTPUT
