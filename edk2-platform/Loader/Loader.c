/* Build the same collector through EDK2 for M3, with official PeCoffLib. */
#ifdef MILESTONE_4
#include "../../build/m4/blobs.h"
#define BAYTRAIL_BLOBS_PREINCLUDED
#endif
#include "../../loader-ia32/main.c"
