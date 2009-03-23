#include <pspkernel.h>

PSP_MODULE_INFO ("test", 0x0, 1, 2);

extern void dumpctrl1 (void);
extern void dumpstatus1 (void);
extern int testcc (float a, float b);

int main (int argc, char **argv)
{
  printf ("%d\n", testcc (2.0f, 3.0f));
  return 0;
}


