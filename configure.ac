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

save_CPPFLAGS=$CPPFLAGS
CPPFLAGS="$CUDA_DRIVER_CPPFLAGS $CPPFLAGS"
save_LDFLAGS=$LDFLAGS
LDFLAGS="$CUDA_DRIVER_LDFLAGS $LDFLAGS"
save_LIBS=$LIBS

# With certain (distribution?) variants of CUDA installations in combination
# with certain linker configurations, "gcc -lcuda" might fail:
#
#     [...]/ld: warning: libnvidia-fatbinaryloader.so.375.66, needed by [...]/libcuda.so, not found (try using -rpath or -rpath-link)
#     [...]/libcuda.so: undefined reference to `elfLink_Start'
#     [...]/libcuda.so: undefined reference to `fatBinaryCtl_Compile'
#     [...]
#     [...]/libcuda.so: undefined reference to `gpucompRestoreLogLine'
#     collect2: error: ld returned 1 exit status
#
# This is because of "libnvidia-fatbinaryloader.so*" residing in some
# non-standard directory that the run-time "ld.so" might look into, but the
# compile-time "ld" doesn't.  In this case, try linking with
# "--allow-shlib-undefined".
CUDA_DRIVER_LDFLAGS_ALLOW_SHLIB_UNDEFINED=
AC_SUBST([CUDA_DRIVER_LDFLAGS_ALLOW_SHLIB_UNDEFINED])
AC_CHECK_LIB([cuda], [cuInit],
  [:],
  [CUDA_DRIVER_LDFLAGS_ALLOW_SHLIB_UNDEFINED=-Wl,--allow-shlib-undefined])
LIBS="$LIBS -lcuda $CUDA_DRIVER_LDFLAGS_ALLOW_SHLIB_UNDEFINED"

AC_CHECK_FUNCS([[cuGetErrorName] [cuGetErrorString]])
AC_CHECK_DECLS([[cuGetErrorName], [cuGetErrorString]],
  [], [], [[#include <cuda.h>]])
AC_CHECK_HEADERS(unistd.h sys/stat.h)

AC_MSG_CHECKING([for extra programs to build requiring -lcuda])
NVPTX_RUN=
AC_LINK_IFELSE([AC_LANG_PROGRAM(
  [#include <cuda.h>],
  [cuInit (0);])],
  [NVPTX_RUN=nvptx-none-run])
AC_MSG_RESULT($NVPTX_RUN)
AC_SUBST(NVPTX_RUN)

CPPFLAGS=$save_CPPFLAGS
LDFLAGS=$save_LDFLAGS
LIBS=$save_LIBS

AC_CHECK_DECLS(getopt)

AC_CONFIG_SUBDIRS([libiberty])
AC_CONFIG_FILES([Makefile dejagnu.exp])
AC_OUTPUT
