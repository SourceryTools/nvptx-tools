__attribute__((kernel))
void __main(int *exitval_ptr, int argc, char *argv[])
{
  *exitval_ptr = argv[1][0];
}
