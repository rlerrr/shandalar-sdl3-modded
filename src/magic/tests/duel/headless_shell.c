/* SDL shell entry points are presentation, never gameplay mocks. */
#include "magic/src/duel_host.h"
int shandalar_host_run_facemaker(const char *assets, const char *userdata)
{ (void)assets; (void)userdata; duel_host_fail("unexpected Facemaker request"); return 0; }
void shandalar_host_begin_deckbuilder(void) { duel_host_fail("unexpected deckbuilder request"); }
void shandalar_host_end_deckbuilder(void) { }
void shandalar_host_begin_duel(void) { duel_host_fail("unexpected duel window request"); }
void shandalar_host_end_duel(void) { }
void shandalar_host_pump(void) { duel_host_checkpoint("execution budget exhausted during host pump"); }
