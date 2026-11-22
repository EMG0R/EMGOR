// sdlayout_fatfs.h -- device StoreCtx (FatFS-backed). Background loop only.
#pragma once
#include "sdlayout.h"
StoreCtx& storeDevice();   // lazily initialised; storeQueue* then storeJobTick(storeDevice()) once per background pass
