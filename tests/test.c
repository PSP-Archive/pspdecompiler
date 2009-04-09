#include <pspkernel.h>

PSP_MODULE_INFO ("test", 0x0, 1, 2);

extern int testcc (float a, float b);


int innerfunc (int a)
{
  return a * a;
}

int switchtest (int c)
{
  switch (c) {
    case -2:
    case -1:
    case 0: return innerfunc (c);
    case 1:
    case 2:
    case 3: return innerfunc (c + 1);
    case 4:
    case 5:
    case 6: return innerfunc (c + 2);
  }
  return 2;
}

int (*myfunc) (int a) = innerfunc;

int func (int b)
{
  if (b & 1)
    return myfunc (b + 2);
  return myfunc (b - 1);
}

int module_start (SceUInt argc, void *arg)
{
  func (4);
  switchtest (-2);
  sceKernelSleepThread ();
  return 0;
}


