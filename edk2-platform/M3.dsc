[Defines]
 PLATFORM_NAME = BaytrailM3
 PLATFORM_GUID = bf9a6f70-cff9-45ac-968a-38488bd21baf
 PLATFORM_VERSION = 0.1
 DSC_SPECIFICATION = 0x00010005
 OUTPUT_DIRECTORY = Build/BaytrailM3
 SUPPORTED_ARCHITECTURES = X64
 BUILD_TARGETS = DEBUG|RELEASE
 SKUID_IDENTIFIER = DEFAULT
 FLASH_DEFINITION = edk2-platform/M3.fdf
!include edk2-platform/M3Libraries.dsc.inc
[Components.X64]
 edk2-platform/Entry/M3Entry.inf
 MdeModulePkg/Core/Dxe/DxeMain.inf
 MdeModulePkg/Core/RuntimeDxe/RuntimeDxe.inf
 edk2-platform/DxeProbe/DxeProbe.inf
