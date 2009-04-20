#include <pspkernel.h>

PSP_MODULE_INFO ("test", 0x0, 1, 2);

extern void testcfg_branches (void);
extern void testcfg_branches_likely (void);
extern void testcfg_branches_never (void);
extern void testcfg_branches_always (void);
extern void testcfg_strangerefs (void);
extern void testcfg_jumptobegin (void);
extern void testswitch (void);

int module_start (SceUInt argc, void *arg)
{
  testcfg_branches ();
  testcfg_branches_likely ();
  testcfg_branches_never ();
  testcfg_branches_always ();
  testcfg_branchlink ();
  testcfg_strangerefs ();
  testcfg_jumptobegin ();
  testswitch ();

  sceKernelSleepThread ();
  return 0;
}

int module_stop (SceUInt argc, void *arg)
{
  return 0;
}
