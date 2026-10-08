/* Build the same collector through EDK2 for M3, with official PeCoffLib. */
#if defined(MILESTONE_USB)
#include "../../build/usb/blobs.h"
#define BAYTRAIL_BLOBS_PREINCLUDED
#elif defined(MILESTONE_SHELL)
#include "../../build/m5/blobs.h"
#define BAYTRAIL_BLOBS_PREINCLUDED
#elif defined(MILESTONE_4)
#include "../../build/m4/blobs.h"
#define BAYTRAIL_BLOBS_PREINCLUDED
#endif
#include "../../loader-ia32/main.c"
