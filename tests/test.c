#include <pspkernel.h>

PSP_MODULE_INFO ("test", 0x1007, 1, 2);

int module_start (SceSize args, void *argp)
{
  void *ptr = &sceIoOpen;
  switch (args) {
  case 0: ptr++; break;
  case 1: ptr--; break;
  case 2:
  case 3: ptr += 2; break;
  case 4: ptr -= 2; break;
  case 5: ptr += 4;
  case 6: ptr += 4; break;
  case 7: args--; break;
  default: args++;
  }
  return (int) ptr;
}

int module_stop (SceSize args, void *argp)
{
  return 0;
}

