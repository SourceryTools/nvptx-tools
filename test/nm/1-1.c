/* Loosely based on
   <https://en.wikipedia.org/w/index.php?title=Nm_(Unix)&oldid=1096095574#nm_output_sample>,
   2022-11-16.

   Compile with '-fno-builtin-memset'.  */

static int static_var;
static int static_var_init = 25;

int global_var;
int global_var_init = 26;

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

int global_function(int p)
{
  static int local_static_var;
  static int local_static_var_init = 5;

  if (!p)
    local_static_var = static_var_init;

  return local_static_var_init + local_static_var;
}

int global_function2()
{
  int x = 1;
  return x;
}

#ifdef __cplusplus
extern "C"
#endif
void non_mangled_function()
{
  /* An 'extern' function with a definition in libc; <string.h>.  */
  extern void *memset(void *, int, __SIZE_TYPE__);
  memset(&static_var, 0, sizeof static_var);
}

int main(void)
{
  global_var = 1;
  static_var = 2;

  return 0;
}
