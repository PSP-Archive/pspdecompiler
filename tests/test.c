#include <pspkernel.h>

PSP_MODULE_INFO ("test", 0x0, 1, 2);

extern int asmfunc (int n);

int module_start (SceUInt argc, void *arg)
{
  return asmfunc (4);
}

int module_stop (SceUInt argc, void *arg)
{
  return asmfunc (2);
}

