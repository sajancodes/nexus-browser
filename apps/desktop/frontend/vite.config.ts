import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

// The production build is served from nexus://app/. Absolute asset paths
// resolve to that origin, and the native asset store rejects traversal.
export default defineConfig({
  plugins: [react()],
  base: '/',
  build: {
    outDir: 'dist',
    emptyOutDir: true,
    target: 'es2022',
    sourcemap: false,
    assetsDir: 'assets',
    modulePreload: { polyfill: false },
  },
  server: {
    port: 5173,
    strictPort: true,
  },
});
