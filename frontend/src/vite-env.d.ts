/// <reference types="vite/client" />

interface ImportMetaEnv {
  /** URL base da API Java (ex.: http://localhost:8080). */
  readonly VITE_API_BASE_URL?: string
}

interface ImportMeta {
  readonly env: ImportMetaEnv
}
