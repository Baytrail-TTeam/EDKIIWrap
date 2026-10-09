/* CPU services needed by DXE's ExitBootServices. This remains an IRQ-off
 * bootstrap: do not enable interrupts or pretend to configure MTRRs/APs. */
#include <PiDxe.h>
#include <Protocol/Cpu.h>
#include <Library/BaseLib.h>
#include <Library/UefiBootServicesTableLib.h>
STATIC EFI_STATUS EFIAPI Flush(EFI_CPU_ARCH_PROTOCOL *This,EFI_PHYSICAL_ADDRESS Start,UINT64 Length,EFI_CPU_FLUSH_TYPE Type) {
 (VOID)This;(VOID)Start;(VOID)Length;
 if(Type!=EfiCpuFlushTypeWriteBackInvalidate) return EFI_UNSUPPORTED;
 AsmWbinvd();return EFI_SUCCESS;
}
STATIC EFI_STATUS EFIAPI Enable(EFI_CPU_ARCH_PROTOCOL *This) {(VOID)This;return EFI_DEVICE_ERROR;}
STATIC EFI_STATUS EFIAPI Disable(EFI_CPU_ARCH_PROTOCOL *This) {(VOID)This;DisableInterrupts();return GetInterruptState()?EFI_DEVICE_ERROR:EFI_SUCCESS;}
STATIC EFI_STATUS EFIAPI State(EFI_CPU_ARCH_PROTOCOL *This,BOOLEAN *Enabled) {
 (VOID)This;if(!Enabled) return EFI_INVALID_PARAMETER;*Enabled=GetInterruptState();return EFI_SUCCESS;
}
STATIC EFI_STATUS EFIAPI Init(EFI_CPU_ARCH_PROTOCOL *This,EFI_CPU_INIT_TYPE Type) {(VOID)This;(VOID)Type;return EFI_UNSUPPORTED;}
STATIC EFI_STATUS EFIAPI Register(EFI_CPU_ARCH_PROTOCOL *This,EFI_EXCEPTION_TYPE Type,EFI_CPU_INTERRUPT_HANDLER Handler) {
 (VOID)This;(VOID)Type;(VOID)Handler;return EFI_UNSUPPORTED;
}
STATIC EFI_STATUS EFIAPI Timer(EFI_CPU_ARCH_PROTOCOL *This,UINT32 Index,UINT64 *Value,UINT64 *Period) {
 (VOID)This;(VOID)Index;(VOID)Period;if(!Value) return EFI_INVALID_PARAMETER;return EFI_UNSUPPORTED;
}
STATIC EFI_STATUS EFIAPI Attributes(EFI_CPU_ARCH_PROTOCOL *This,EFI_PHYSICAL_ADDRESS Base,UINT64 Length,UINT64 Value) {
 (VOID)This;(VOID)Base;(VOID)Length;(VOID)Value;return EFI_UNSUPPORTED;
}
STATIC EFI_CPU_ARCH_PROTOCOL Cpu={Flush,Enable,Disable,State,Init,Register,Timer,Attributes,0,64};
EFI_STATUS InstallBootCpu(VOID) {
 EFI_HANDLE Handle=NULL;return gBS->InstallProtocolInterface(&Handle,&gEfiCpuArchProtocolGuid,EFI_NATIVE_INTERFACE,&Cpu);
}
