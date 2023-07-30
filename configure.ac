AC_INIT([nvptx-tools])
ACX_PKGVERSION([nvptx-tools])
ACX_BUGURL([https://github.com/MentorEmbedded/nvptx-tools/issues])
AC_CONFIG_SRCDIR([nvptx-as.cc])
AC_CONFIG_MACRO_DIRS([config])

AC_PROG_CC
AC_PROG_CXX

#AC_PROG_RANLIB
AC_PROG_INSTALL

AC_PATH_PROG(RANLIB, ranlib)
AC_PATH_PROG(AR, ar)

AC_CHECK_HEADERS(unistd.h sys/stat.h)
AC_CHECK_DECLS(getopt)

DL_LIBS=
AC_SUBST(DL_LIBS)
AC_CHECK_LIB(dl, dlsym, [DL_LIBS=-ldl])

NVPTX_RUN=nvptx-none-run
AC_SUBST(NVPTX_RUN)
if test x"$DL_LIBS" = x; then
  AC_MSG_NOTICE([not able to build $NVPTX_RUN])
  NVPTX_RUN=
fi

AC_CONFIG_SUBDIRS([libiberty])
AC_CONFIG_FILES([Makefile dejagnu.exp])


# Testsuite

# As of 2020-12-18, there is no standard way to invoke lit; see 'test/README'.
AC_CHECK_PROGS([LIT], [lit] [lit.py] [llvm-lit])

AC_CONFIG_FILES([test/lit.site.cfg.py])


AC_OUTPUT
