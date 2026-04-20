/// <reference types="vite/client" />

interface ImportMetaEnv {
  readonly VITE_CELIA_RUNTIME_TARGET?: 'web' | 'native-desktop' | 'native-mobile' | 'automotive'
  readonly VITE_CELIA_API_BASE_URL?: string
}

interface ImportMeta {
  readonly env: ImportMetaEnv
}
