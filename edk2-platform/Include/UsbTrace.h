#ifndef BAYTRAIL_USB_TRACE_H
#define BAYTRAIL_USB_TRACE_H
#include <Uefi.h>
typedef struct {UINT32 Revision;BOOLEAN Enabled;} BAYTRAIL_USB_TRACE_CONTROL;
extern EFI_GUID gBaytrailUsbTraceControlGuid;
#endif
