#ifndef SILPH_HUNTING_H
#define SILPH_HUNTING_H
#include "visual-tools.h"
#include "game-read.h"
#include "session-link.h"
int silph_hunting_start(const SilphManagedApi *,SilphManagedResolver,void *,uint32_t,SilphVisualAddress);
int silph_hunting_tick(void *,void *,void *,void *,const SilphScheduleIdentity *,const SilphSessionConfig *,const SilphSchedulePermit *,const SilphGameSnapshot *,uint64_t);
void silph_hunting_snapshot(SilphSessionSnapshot *);
int silph_hunting_publish(void *);
int silph_hunting_close(void);
#endif
