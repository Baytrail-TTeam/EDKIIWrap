[Defines]
 PLATFORM_NAME = BaytrailM4
 PLATFORM_GUID = 7c36a4c8-e6b3-41a9-84d5-c8b570253c4a
 PLATFORM_VERSION = 0.1
 DSC_SPECIFICATION = 0x00010005
 OUTPUT_DIRECTORY = Build/BaytrailM4
 SUPPORTED_ARCHITECTURES = X64
 BUILD_TARGETS = DEBUG|RELEASE
 SKUID_IDENTIFIER = DEFAULT
 FLASH_DEFINITION = edk2-platform/M4.fdf
!include edk2-platform/M3Libraries.dsc.inc
[LibraryClasses]
 FrameBufferBltLib|MdeModulePkg/Library/FrameBufferBltLib/FrameBufferBltLib.inf
[Components.X64]
 edk2-platform/Entry/M3Entry.inf
 MdeModulePkg/Core/Dxe/DxeMain.inf
 MdeModulePkg/Core/RuntimeDxe/RuntimeDxe.inf
 edk2-platform/FramebufferGop/FramebufferGop.inf
 edk2-platform/M4Probe/M4Probe.inf
