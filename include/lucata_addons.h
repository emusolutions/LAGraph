#ifndef LUCATA_ADDONS_H
#define LUCATA_ADDONS_H


#ifdef __EMU_CC__
#include "emu_c_utils/emu_c_utils.h"
#include "memoryweb/memoryweb.h"
/*
The LC_TIMING_WRAPPER_* utilities are meant to be used as:
```c++
    LC_TIMING_WRAPPER_START("LAGr_BFS");
    LAGRAPH_TRY (LAGr_BreadthFirstSearch (NULL, &parent, G, src, msg)) ;
    LC_TIMING_WRAPPER_END("LAGr_BFS");
```

The call to hooks_region_end will dump the timing info to stdout as a json object.
*/
#define LC_TIMING_WRAPPER_START(NAME)                                             \
  hooks_set_active_region(NAME, TIMING_ONLY);                                  \
  hooks_region_begin(NAME);

// Be aware if you capture this time, it will be in MILLISECONDS
#define LC_TIMING_WRAPPER_END(NAME) hooks_region_end(NAME)

#else
#define LC_TIMING_WRAPPER_START(NAME)
#define LC_TIMING_WRAPPER_END(NAME)
#endif

#endif
