[Defines]
 PLATFORM_NAME = BaytrailUsbPlatform
 PLATFORM_GUID = 2e3c2ccf-a90b-4af6-9e5d-5c6fe2184624
 PLATFORM_VERSION = 0.1
 DSC_SPECIFICATION = 0x00010005
 OUTPUT_DIRECTORY = Build/BaytrailUsbPlatform
 SUPPORTED_ARCHITECTURES = X64
 BUILD_TARGETS = DEBUG|RELEASE
 SKUID_IDENTIFIER = DEFAULT
 FLASH_DEFINITION = edk2-platform/UsbPlatform.fdf
!include edk2-platform/M3Libraries.dsc.inc
[LibraryClasses]
 FrameBufferBltLib|MdeModulePkg/Library/FrameBufferBltLib/FrameBufferBltLib.inf
 HiiLib|MdeModulePkg/Library/UefiHiiLib/UefiHiiLib.inf
 UefiHiiServicesLib|MdeModulePkg/Library/UefiHiiServicesLib/UefiHiiServicesLib.inf
 UefiRuntimeLib|MdePkg/Library/UefiRuntimeLib/UefiRuntimeLib.inf
 AuthVariableLib|MdeModulePkg/Library/AuthVariableLibNull/AuthVariableLibNull.inf
 TpmMeasurementLib|MdeModulePkg/Library/TpmMeasurementLibNull/TpmMeasurementLibNull.inf
 VarCheckLib|MdeModulePkg/Library/VarCheckLib/VarCheckLib.inf
 VariablePolicyLib|MdeModulePkg/Library/VariablePolicyLib/VariablePolicyLib.inf
 VariablePolicyHelperLib|MdeModulePkg/Library/VariablePolicyHelperLib/VariablePolicyHelperLib.inf
 VariableFlashInfoLib|MdeModulePkg/Library/BaseVariableFlashInfoLib/BaseVariableFlashInfoLib.inf
 UefiUsbLib|MdePkg/Library/UefiUsbLib/UefiUsbLib.inf
 GptLib|MdeModulePkg/Library/GptLib/GptLib.inf
[LibraryClasses.common.DXE_CORE]
 SerialPortLib|edk2-platform/Library/ExceptionDisplayLib/ExceptionDisplayLib.inf
[LibraryClasses.common.DXE_DRIVER]
 TimerLib|edk2-platform/Library/PmTimerLib/PmTimerLib.inf
[LibraryClasses.common.DXE_RUNTIME_DRIVER]
 VariablePolicyLib|MdeModulePkg/Library/VariablePolicyLib/VariablePolicyLibRuntimeDxe.inf
[LibraryClasses.common.UEFI_DRIVER]
 DebugLib|edk2-platform/Library/UsbDebugLib/UsbDebugLib.inf
 TimerLib|edk2-platform/Library/PmTimerLib/PmTimerLib.inf
[PcdsFixedAtBuild]
 gEfiMdeModulePkgTokenSpaceGuid.PcdEmuVariableNvModeEnable|TRUE
 gEfiMdeModulePkgTokenSpaceGuid.PcdVariableStoreSize|0x10000
 gEfiMdeModulePkgTokenSpaceGuid.PcdFlashNvStorageVariableSize|0x40000
[Components.X64]
 edk2-platform/Entry/M3Entry.inf
 MdeModulePkg/Core/Dxe/DxeMain.inf
 MdeModulePkg/Core/RuntimeDxe/RuntimeDxe.inf
 edk2-platform/ReadOnlyRtc/ReadOnlyRtc.inf
 edk2-platform/FramebufferGop/FramebufferGop.inf
 MdeModulePkg/Universal/Variable/RuntimeDxe/VariableRuntimeDxe.inf
 MdeModulePkg/Universal/HiiDatabaseDxe/HiiDatabaseDxe.inf
 MdeModulePkg/Universal/Console/GraphicsConsoleDxe/GraphicsConsoleDxe.inf
 MdeModulePkg/Universal/Disk/UnicodeCollation/EnglishDxe/EnglishDxe.inf
 MdeModulePkg/Universal/Disk/DiskIoDxe/DiskIoDxe.inf
 FatPkg/EnhancedFatDxe/Fat.inf
 edk2-platform/UsbPlatform/UsbPlatform.inf
 MdeModulePkg/Bus/Pci/XhciDxe/XhciDxe.inf
 MdeModulePkg/Bus/Usb/UsbBusDxe/UsbBusDxe.inf
 MdeModulePkg/Bus/Usb/UsbKbDxe/UsbKbDxe.inf
 MdeModulePkg/Bus/Usb/UsbMassStorageDxe/UsbMassStorageDxe.inf
 MdeModulePkg/Universal/Disk/PartitionDxe/PartitionDxe.inf
 edk2-platform/UsbBoot/UsbBoot.inf
 edk2-platform/UsbTest/UsbTest.inf

[BuildOptions]
 GCC:*_CLANGPDB_X64_CC_FLAGS = -DM3_ARENA_SIZE=268435456u
