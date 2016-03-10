/* A tool to run PTX binaries compiled with -mmainkernel.
   Copyright (C) 2014, 2015 Mentor Graphics

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

#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <cuda.h>

#include "version.h"

/* On systems where installed NVIDIA driver is newer than CUDA Toolkit,
   libcuda.so may have these functions even though <cuda.h> does not.  */

#if defined HAVE_CUGETERRORNAME && !HAVE_DECL_CUGETERRORNAME
extern "C" CUresult cuGetErrorName (CUresult, const char **);
#endif
#if defined HAVE_CUGETERRORSTRING && !HAVE_DECL_CUGETERRORSTRING
extern "C" CUresult cuGetErrorString (CUresult, const char **);
#endif


static void __attribute__ ((format (printf, 1, 2)))
fatal_error (const char * cmsgid, ...)
{
  va_list ap;

  va_start (ap, cmsgid);
  fprintf (stderr, "nvptx-run: ");
  vfprintf (stderr, cmsgid, ap);
  fprintf (stderr, "\n");
  va_end (ap);

  exit (127);
}

static void
fatal_unless_success (CUresult r, const char *err)
{
  if (r == CUDA_SUCCESS)
    return;

  const char *s = "[unknown]";
  const char *n = "[unknown]";
#if defined HAVE_CUGETERRORSTRING
  cuGetErrorString (r, &s);
#endif
#if defined HAVE_CUGETERRORNAME
  cuGetErrorName (r, &n);
#endif
  fatal_error ("%s: %s (%s, %d)", err, s, n, (int) r);
}

static size_t jitopt_lineinfo, jitopt_debuginfo, jitopt_optimize = 4;

static void
compile_file (FILE *f, CUmodule *phModule, CUfunction *phKernel)
{
  CUresult r;
   
  char elog[8192];
  CUjit_option opts[] = {
    CU_JIT_ERROR_LOG_BUFFER, CU_JIT_ERROR_LOG_BUFFER_SIZE_BYTES,
    CU_JIT_GENERATE_LINE_INFO,
    CU_JIT_GENERATE_DEBUG_INFO,
    CU_JIT_OPTIMIZATION_LEVEL,
  };
  void *optvals[] = {
    elog, (void*) sizeof elog,
    (void*) jitopt_lineinfo,
    (void*) jitopt_debuginfo,
    (void*) jitopt_optimize,
  };
  CUlinkState linkstate;

  r = cuLinkCreate (sizeof opts / sizeof *opts, opts, optvals, &linkstate);
  fatal_unless_success (r, "cuLinkCreate failed");
  
  fseek (f, 0, SEEK_END);
  int len = ftell (f);
  fseek (f, 0, SEEK_SET);

  char *program = new char[len + 1];
  fread (program, 1, len, f);
  program[len] = '\0';
  int off = 0;
  int count = 0;
  
  while (off < len)
    {
      char namebuf[100];
      int l = strlen (program + off);
      sprintf (namebuf, "input file %d at offset %d", count++, off);
      r = cuLinkAddData (linkstate, CU_JIT_INPUT_PTX, program + off, l + 1,
			 strdup (namebuf), 0, 0, 0);
      if (r != CUDA_SUCCESS)
	{
#if 0
	  fputs (program + off, stderr);
#endif
	  fprintf (stderr, "%s\n", elog);
	  fatal_unless_success (r, "cuLinkAddData failed");
	}

      off += l;
      while (off < len && program[off] == '\0')
	off++;
    }

  void *linkout;
  r = cuLinkComplete (linkstate, &linkout, NULL);
  if (r != CUDA_SUCCESS)
    {
      fprintf (stderr, "%s\n", elog);
      fatal_unless_success (r, "cuLinkComplete failed");
    }

  r = cuModuleLoadData (phModule, linkout);
  fatal_unless_success (r, "cuModuleLoadData failed");

  r = cuModuleGetFunction (phKernel, *phModule, "__main");
  fatal_unless_success (r, "could not find kernel __main");
}

static const struct option long_options[] =
  {
    { "stack-size", required_argument, 0, 'S' },
    { "heap-size", required_argument, 0, 'H' },
    { "lanes", required_argument, 0, 'L' },
    { "optlevel", required_argument, 0, 'O' },
    { "lineinfo", no_argument, 0, 'g' },
    { "debuginfo", no_argument, 0, 'G' },
    { "help", no_argument, 0, 'h' },
    { "version", no_argument, 0, 'V' },
    { 0, 0, 0, 0 }
  };

int
main (int argc, char **argv)
{
  int o;
  long stack_size = 0, heap_size = 256 * 1024 * 1024, num_lanes = 1;
  while ((o = getopt_long (argc, argv, "S:H:L:O:gGhV", long_options, 0)) != -1)
    {
      switch (o)
	{
	case 'S':
	  stack_size = strtol (optarg, NULL, 0);
	  if (stack_size <= 0)
	    fatal_error ("invalid stack size");
	  break;
	case 'H':
	  heap_size = strtol (optarg, NULL, 0);
	  if (heap_size <= 0)
	    fatal_error ("invalid heap size");
	  break;
	case 'L':
	  num_lanes = strtol (optarg, NULL, 0);
	  if (num_lanes < 1 || num_lanes > 32)
	    fatal_error ("invalid lane count");
	  break;
	case 'O':
	  jitopt_optimize = (size_t) optarg[0] - '0';
	  if (jitopt_optimize > 4 || optarg[1] != 0)
	    fatal_error ("invalid optimization level");
	  break;
	case 'g':
	  jitopt_lineinfo = 1;
	  break;
	case 'G':
	  jitopt_debuginfo = 1;
	  break;
	case 'h':
	  printf ("\
Usage: nvptx-none-run [option...] program [argument...]\n\
Options:\n\
  -S, --stack-size N    Set per-lane GPU stack size to N (default: auto)\n\
  -H, --heap-size N     Set GPU heap size to N (default: 256 MiB)\n\
  -L, --lanes N         Launch N lanes (for testing gcc -muniform-simt)\n\
  -O, --optlevel N      Pass PTX JIT option to set optimization level N\n\
  -g, --lineinfo        Pass PTX JIT option to generate line information\n\
  -G, --debuginfo       Pass PTX JIT option to generate debug information\n\
  --help                Print this help and exit\n\
  --version             Print version number and exit\n\
\n\
Report bugs to %s.\n",
		  REPORT_BUGS_TO);
	  exit (0);
	case 'V':
	  printf ("\
nvtpx-none-run %s%s\n\
Copyright %s Mentor Graphics\n\
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>\n\
This program is free software; you may redistribute it under the terms of\n\
the GNU General Public License version 3 or later.\n\
This program has absolutely no warranty.\n",
		  PKGVERSION, NVPTX_TOOLS_VERSION, "2015");
	  exit (0);
	default:
	  break;
	}
    }

  if (argc < optind + 1)
    fatal_error ("no program file specified");

  const char *progname = argv[optind];
  FILE *f = fopen (progname, "r");
  if (f == NULL)
    fatal_error ("program file not found");

  CUresult r;
  r = cuInit (0);
  fatal_unless_success (r, "cuInit failed");

  CUdevice dev;
  CUcontext ctx;
  r = cuDeviceGet (&dev, 0);
  fatal_unless_success (r, "cuDeviceGet failed");
  r = cuCtxCreate (&ctx, 0, dev);
  fatal_unless_success (r, "cuCtxCreate failed");

  CUdeviceptr d_retval;
  r = cuMemAlloc(&d_retval, sizeof (int));
  fatal_unless_success (r, "cuMemAlloc failed");
  int d_argc = argc - optind;
  /* The argv pointers, followed by the actual argv strings.  */
  CUdeviceptr  d_argv;
  {
    size_t s_d_argv = d_argc * sizeof (char *);
    for (int arg = optind; arg < argc; ++arg)
      s_d_argv += strlen (argv[arg]) + 1;
    r = cuMemAlloc(&d_argv, s_d_argv);
    fatal_unless_success (r, "cuMemAlloc failed");
    /* Skip the argv pointers.  */
    size_t pos = d_argc * sizeof (char *);
    for (int arg = optind; arg < argc; ++arg)
    {
      CUdeviceptr d_arg = (CUdeviceptr) ((char *) d_argv + pos);
      size_t len = strlen (argv[arg]) + 1;
      r = cuMemcpyHtoD(d_arg, argv[arg], len);
      fatal_unless_success (r, "cuMemcpyHtoD (d_arg) failed");
      r = cuMemcpyHtoD((CUdeviceptr) ((char *) d_argv
				      + (arg - optind) * sizeof (char *)),
		       &d_arg, sizeof (char *));
      fatal_unless_success (r, "cuMemcpyHtoD (d_argv) failed");
      pos += len;
    }
  }

#if 0
  /* Default seems to be 1 KiB stack, 8 MiB heap.  */
  size_t stack, heap;
  cuCtxGetLimit (&stack, CU_LIMIT_STACK_SIZE);
  cuCtxGetLimit (&heap, CU_LIMIT_MALLOC_HEAP_SIZE);
  printf ("stack %ld heap %ld\n", stack, heap);
#endif

  if (!stack_size)
    {
      /* It appears that CUDA driver sometimes accounts memory as if stacks
         were reserved for the maximum number of threads the device can host,
	 even if only a few are launched.  Compute the default accordingly.  */
      int sm_count, thread_max;
      r = cuDeviceGetAttribute (&sm_count,
				CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT, dev);
      fatal_unless_success (r, "could not get SM count");
      r = cuDeviceGetAttribute
	(&thread_max, CU_DEVICE_ATTRIBUTE_MAX_THREADS_PER_MULTIPROCESSOR, dev);
      fatal_unless_success (r, "could not get max threads per SM count");
      size_t mem;
      r = cuDeviceTotalMem (&mem, dev);
      fatal_unless_success (r, "could not get available memory");
      /* Subtract heap size and a 128 MiB extra.  */
      mem -= heap_size + 128 * 1024 * 1024;
      mem /= sm_count * thread_max;
      /* Always limit default size to 128 KiB maximum.  */
      if (mem > 128 * 1024)
	mem = 128 * 1024;
      /* Round down to 8-byte boundary.  */
      stack_size = mem & -8u;
    }
  r = cuCtxSetLimit(CU_LIMIT_STACK_SIZE, stack_size);
  fatal_unless_success (r, "could not set stack limit");
  r = cuCtxSetLimit(CU_LIMIT_MALLOC_HEAP_SIZE, heap_size);
  fatal_unless_success (r, "could not set heap limit");

  CUmodule hModule = 0;
  CUfunction hKernel = 0;
  compile_file (f, &hModule, &hKernel);

  void *args[] = { &d_retval, &d_argc, &d_argv };
    
  r = cuLaunchKernel (hKernel, 1, 1, 1, num_lanes, 1, 1, 1024, NULL, args, NULL);
  fatal_unless_success (r, "error launching kernel");

  int result;
  r = cuMemcpyDtoH(&result, d_retval, sizeof (int));
  fatal_unless_success (r, "error getting kernel result");

  cuMemFree (d_retval);
  cuMemFree (d_argv);

  if (hModule)
    cuModuleUnload (hModule);

  cuCtxDestroy (ctx);

  return result;
}
