__attribute__((kernel))
void __main(int *exitval_ptr, int argc, char *argv[])
{
  for (int i = 0; i < argc; ++i)
    __builtin_printf("%d: %s\n", i, argv[i]);
  *exitval_ptr = !(argc > 0);
}
