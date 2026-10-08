#ifndef BAYTRAIL_MARKER_H
#define BAYTRAIL_MARKER_H
#include <Uefi.h>
#include "Handoff.h"
VOID EFIAPI BaytrailMarker(IN CONST Handoff *H, IN UINT32 Row, IN CONST CHAR8 *Text, IN UINT32 Color);
VOID EFIAPI BaytrailHalt(VOID);
#endif
