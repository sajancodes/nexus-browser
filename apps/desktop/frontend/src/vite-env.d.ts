import type { NativeBridge } from '@nexus/ipc-contracts';

declare global {
  interface Window {
    __nexusNative?: NativeBridge;
  }
}

export {};
