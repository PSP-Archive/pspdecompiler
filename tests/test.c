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
int global_b = 2;

int func (int b)
{
  if (b & 1)
    return callback (b + global_b);
  return callback (b - 1);
}

int complexifs (int a, int b, int c, int d, int e, int f)
{
  if ((a = ((b < c) ? (d + 1) : e)) == f) {
    f = a * a;
    global_b = a;
    return f - 1;
  } else {
    a = complexifs (a, b, c - 1, d, e, f + 1);
    return e - a;
  }
}

int module_start (SceUInt argc, void *arg)
{
  func (4);
  switchtest (array[2]);
  testcall (4);
  sceKernelSleepThread ();
  return complexifs (10, 9, 8, 7, 6, 5);
}

int module_stop (SceUInt argc, void *arg)
{
  return 0;
}

