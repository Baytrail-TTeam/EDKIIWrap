# M4 — gated integration

Reuse/adapt official UefiPayloadPkg/GraphicsOutputDxe plus FrameBufferBltLib,
with graphics info/device HOB and preserved PCI BAR resources.
No GOP driver is built or loaded in M1/M2. The minimal X64 payload writes
inherited pixels directly. Native ValleyView X64 GOP donor: docs/GOP-DONOR.json.
