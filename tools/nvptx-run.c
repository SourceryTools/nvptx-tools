/* A tool to run nvptx binaries compiled with -mmainkernel.
   Copyright 2014 Mentor Graphics.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3, or (at your option)
any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */

#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <cuda.h>
#include <cuda_runtime.h>

#include "version.h"

static void __attribute__ ((format (printf, 1, 2)))
fatal_error (const char * cmsgid, ...)
{
  va_list ap;

  va_start (ap, cmsgid);
  fprintf (stderr, "nvptx-run: ");
  vfprintf (stderr, cmsgid, ap);
  fprintf (stderr, "\n");
  va_end (ap);

  exit (1);
}

static void
fatal_unless_success (CUresult r, const char *err)
{
  if (r == CUDA_SUCCESS)
    return;

  const char *p;
  cuGetErrorString (r, &p);
  fatal_error ("%s: %s", err, p);
}

static void
compile_file (FILE *f, CUmodule *phModule, CUfunction *phKernel)
{
  CUresult r;
   
  CUlinkState linkstate;
  CUjit_option opts[5];
  void *optvals[5];
#define LOGSIZE 8192
  char elog[LOGSIZE];

  opts[0] = CU_JIT_TARGET;
  optvals[0] = (void *) CU_TARGET_COMPUTE_30;

  opts[1] = CU_JIT_ERROR_LOG_BUFFER;
  optvals[1] = &elog[0];

  opts[2] = CU_JIT_ERROR_LOG_BUFFER_SIZE_BYTES;
  optvals[2] = (void *) LOGSIZE;

  r = cuLinkCreate (3, opts, optvals, &linkstate);
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
    { "help", no_argument, 0, 'h' },
    { "version", no_argument, 0, 'V' },
    { 0, 0, 0, 0 }
  };

int
main (int argc, char **argv)
{
  int o;
  int option_index = 0;
  while ((o = getopt_long (argc, argv, "o:I:v", long_options, &option_index)) != -1)
    {
      switch (o)
	{
	case 'h':
	  printf ("\
Usage: nvptx-none-run [option...] FILE\n\
Options:\n\
  --help                Print this help and exit\n\
  --version             Print version number and exit\n\
\n\
Report bugs to %s.\n",
		  REPORT_BUGS_TO);
	  exit (0);
	case 'V':
	  printf ("\
nvtpx-none-run %s%s\n\
Copyright %s Free Software Foundation, Inc.\n\
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

  if (argc > optind + 1)
    fatal_error ("more than one argument");
  else if (argc < optind + 1)
    fatal_error ("no input file specified");

  const char *progname = argv[optind];
  FILE *f = fopen (progname, "r");
  if (f == NULL)
    fatal_error ("input file not found");

  CUresult r;
  r = cuInit (0);
  fatal_unless_success (r, "cuInit failed");

  CUdevice dev;
  CUcontext ctx;
  r = cuDeviceGet (&dev, 0);
  fatal_unless_success (r, "cuDeviceGet failed");
  r = cuCtxCreate (&ctx, 0, dev);
  fatal_unless_success (r, "cuCtxCreate failed");
    
  int d_argc = 0;
  CUdeviceptr d_retval, d_progname, d_argv;
  size_t nameln = strlen (progname);
  r = cuMemAlloc(&d_retval, 4);
  fatal_unless_success (r, "cuMemAlloc failed");
  r = cuMemAlloc(&d_argv, 8);
  fatal_unless_success (r, "cuMemAlloc failed");
  r = cuMemAlloc(&d_progname, nameln + 1);
  fatal_unless_success (r, "cuMemAlloc failed");

  r = cuMemcpyHtoD(d_progname, progname, nameln + 1);
  fatal_unless_success (r, "cuMemcpy failed");
  r = cuMemcpyHtoD(d_argv, &d_progname, 8);
  fatal_unless_success (r, "cuMemcpy failed");

#if 0
  /* Default seems to be 8k stack, 8M heap.  */
  size_t stack, heap;
  cuCtxGetLimit (&stack, CU_LIMIT_STACK_SIZE);
  cuCtxGetLimit (&heap, CU_LIMIT_MALLOC_HEAP_SIZE);
  printf ("stack %ld heap %ld\n", stack, heap);
#endif

  r = cuCtxSetLimit(CU_LIMIT_STACK_SIZE, 256 * 1024);
  fatal_unless_success (r, "could not set stack limit");
  r = cuCtxSetLimit(CU_LIMIT_MALLOC_HEAP_SIZE, 256 * 1024 * 1024);
  fatal_unless_success (r, "could not set heap limit");

  CUmodule hModule = 0;
  CUfunction hKernel = 0;
  compile_file (f, &hModule, &hKernel);

  void *args[] = { &d_retval, &d_argc, &d_argv };
    
  r = cuLaunchKernel (hKernel, 1, 1, 1, 1, 1, 1, 1024, NULL, args, NULL);
  fatal_unless_success (r, "error launching kernel");

  int result;
  r = cuMemcpyDtoH(&result, d_retval, sizeof (int));
  fatal_unless_success (r, "error getting kernel result");

  cuMemFree (d_retval);
  cuMemFree (d_argv);
  cuMemFree (d_progname);

  if (hModule)
    cuModuleUnload (hModule);

  cuCtxDestroy (ctx);
  cudaDeviceReset ();

  if (result != 0)
    {
      printf ("Program result: %d\n", result);
      abort ();
    }

  return 0;
}
