// Each -fhardened constituent, exercised directly on aarch64-apple-darwin24.
extern void sink(char *);

void protector(void)   // -fstack-protector-strong: expect __stack_chk_guard
{
  char buf[64];
  sink(buf);
}

void clash(void)       // -fstack-clash-protection: expect probed 128 KiB frame
{
  char big[128 * 1024];
  sink(big);
}

int autoinit(void)     // -ftrivial-auto-var-init=zero: expect zeroed local
{
  int x;
  sink(reinterpret_cast<char *>(&x));
  return x;
}
