/* This is a variant of '1-1.c', altered only such that they may be linked
   together.

   In particular, this file contains the same 'static' parts, so that the same
   symbols get emitted for those.

   Compile with '-fno-builtin-memset'.  */

static int static_var;
static int static_var_init = 25;

extern int global_var;
extern int global_var_init;

/* An 'extern' variable with a definition in libc; <unistd.h>.  */
extern char **environ;

static int static_function()
{
  static int local_static_var;
  static int local_static_var_init = 5;

  if (!environ)
    local_static_var = static_var_init;

  return local_static_var_init + local_static_var;
}

int GLOBAL_FUNCTION(int p)
{
  static int local_static_var;
  static int local_static_var_init = 5;

  if (!p)
    local_static_var = static_var_init;

  return local_static_var_init + local_static_var;
}

int GLOBAL_FUNCTION2()
{
  int x = 1;
  return x;
}

#ifdef __cplusplus
extern "C"
#endif
void NON_MANGLED_FUNCTION()
{
  /* An 'extern' function with a definition in libc; <string.h>.  */
  extern void *memset(void *, int, __SIZE_TYPE__);
  memset(&static_var, 0, sizeof static_var);
}

int MAIN(void)
{
  global_var = 1;
  static_var = 2;

  return 0;
}
