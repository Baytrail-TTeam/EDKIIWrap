[Defines]
 PLATFORM_NAME = BaytrailShell
 PLATFORM_GUID = c781241a-55f0-43bc-965e-a13c65a86b55
 PLATFORM_VERSION = 0.1
 DSC_SPECIFICATION = 0x00010005
 OUTPUT_DIRECTORY = Build/BaytrailShell
 SUPPORTED_ARCHITECTURES = X64
 BUILD_TARGETS = DEBUG|RELEASE
 SKUID_IDENTIFIER = DEFAULT
!include MdePkg/MdeLibs.dsc.inc
[LibraryClasses]
 BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
 BaseMemoryLib|MdePkg/Library/BaseMemoryLibRepStr/BaseMemoryLibRepStr.inf
 DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
 PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
 PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
 MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
 UefiApplicationEntryPoint|MdePkg/Library/UefiApplicationEntryPoint/UefiApplicationEntryPoint.inf
 UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
 UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
 UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
 DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
 DxeServicesTableLib|MdePkg/Library/DxeServicesTableLib/DxeServicesTableLib.inf
 PeCoffGetEntryPointLib|MdePkg/Library/BasePeCoffGetEntryPointLib/BasePeCoffGetEntryPointLib.inf
 HobLib|MdePkg/Library/DxeHobLib/DxeHobLib.inf
 DxeServicesLib|MdePkg/Library/DxeServicesLib/DxeServicesLib.inf
[BuildOptions]
 GCC:*_CLANGPDB_*_DLINK_FLAGS = /timestamp:0

[LibraryClasses]
 FileHandleLib|MdePkg/Library/UefiFileHandleLib/UefiFileHandleLib.inf
 ShellLib|ShellPkg/Library/UefiShellLib/UefiShellLib.inf
 ShellCommandLib|ShellPkg/Library/UefiShellCommandLib/UefiShellCommandLib.inf
 HandleParsingLib|ShellPkg/Library/UefiHandleParsingLib/UefiHandleParsingLib.inf
 SortLib|MdeModulePkg/Library/UefiSortLib/UefiSortLib.inf
 OrderedCollectionLib|MdePkg/Library/BaseOrderedCollectionRedBlackTreeLib/BaseOrderedCollectionRedBlackTreeLib.inf
 HiiLib|MdeModulePkg/Library/UefiHiiLib/UefiHiiLib.inf
 UefiHiiServicesLib|MdeModulePkg/Library/UefiHiiServicesLib/UefiHiiServicesLib.inf
[Components.X64]
 ShellPkg/Application/Shell/Shell.inf {
  <PcdsFixedAtBuild>
   gEfiShellPkgTokenSpaceGuid.PcdShellLibAutoInitialize|FALSE
   gEfiShellPkgTokenSpaceGuid.PcdShellDefaultDelay|0
  <LibraryClasses>
   NULL|ShellPkg/Library/UefiShellLevel1CommandsLib/UefiShellLevel1CommandsLib.inf
   NULL|ShellPkg/Library/UefiShellLevel2CommandsLib/UefiShellLevel2CommandsLib.inf
   NULL|ShellPkg/Library/UefiShellLevel3CommandsLib/UefiShellLevel3CommandsLib.inf
 }
