#include <pspkernel.h>

PSP_MODULE_INFO ("test", 0x0, 1, 2);

extern void dumpctrl1 (void);
extern void dumpstatus1 (void);
extern int testcc (float a, float b);


int innerfunc (int a)
{
  return a * a;
}


int (*myfunc) (int a) = innerfunc;

int func (int b)
{
  if (b & 1)
    return myfunc (b + 2);
  return myfunc (b - 1);
}

int main (int argc, char **argv)
{
  func (4);
  printf ("0x%08X\n", testcc (2.0f, 3.0f));
  return 0;
}


