#include <pspkernel.h>

PSP_MODULE_INFO ("testprx", 0x0, 1, 2);

extern void cfg_branches (void);
extern void cfg_branches_likely (void);
extern void cfg_branches_noswap (void);
extern void cfg_branches_never (void);
extern void cfg_branches_always (void);
extern void cfg_branchlink (void);
extern void cfg_strangerefs (void);
extern void cfg_jumptobegin (void);
extern void cfg_callnoswap (void);
extern void cfg_switch (void);
extern void graph_double_break (void);
extern void graph_nested_ifs (void);
extern void graph_for_inside_if (void);
extern void graph_nested_loops (void);
extern void var_k1 (void);
extern void hlide_constant (void);


int module_start (SceUInt argc, void *arg)
{
  cfg_branches ();
  cfg_branches_likely ();
  cfg_branches_noswap ();
  cfg_branches_never ();
  cfg_branches_always ();
  cfg_branchlink ();
  cfg_strangerefs ();
  cfg_jumptobegin ();
  cfg_callnoswap ();
  cfg_switch ();
  graph_double_break ();
  graph_nested_ifs ();
  graph_for_inside_if ();
  graph_nested_loops ();
  var_k1 ();
  hlide_constant ();

  sceKernelSleepThread ();
  return 0;
}

int module_stop (SceUInt argc, void *arg)
{
  return 0;
}
