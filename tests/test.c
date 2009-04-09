#include <pspkernel.h>

PSP_MODULE_INFO ("test", 0x0, 1, 2);

static int array[1024];

extern int testcall (int a);

static
int innerfunc (int a)
{
  array[a + 10] = a - 1;
  return a * a;
}

static
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

int (*callback) (int a) = innerfunc;

int func (int b)
{
  if (b & 1)
    return callback (b + 2);
  return callback (b - 1);
}

int module_start (SceUInt argc, void *arg)
{
  func (4);
  switchtest (array[2]);
  testcall (4);
  sceKernelSleepThread ();
  return 0;
}


