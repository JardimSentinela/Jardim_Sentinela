import { useState } from 'react'
import { Droplets, Moon, Sun } from 'lucide-react'

function App() {
  const [temaEscuro, setTemaEscuro] = useState(false)

  function alternarTema() {
    const proximo = !temaEscuro
    setTemaEscuro(proximo)
    document.documentElement.classList.toggle('dark', proximo)
  }

  return (
    <main className="mx-auto flex min-h-screen max-w-3xl flex-col justify-center gap-8 px-6 py-16">
      <header className="flex items-start justify-between gap-4">
        <div className="flex items-center gap-3">
          <span className="flex h-12 w-12 items-center justify-center rounded-xl bg-jardin-100 text-jardin-700 dark:bg-jardin-900 dark:text-jardin-200">
            <Droplets className="h-6 w-6" aria-hidden="true" />
          </span>
          <div>
            <h1 className="text-2xl">Jardim Sentinela</h1>
            <p className="text-suave text-sm">Reduzir, observar e comunicar.</p>
          </div>
        </div>

        <button
          type="button"
          onClick={alternarTema}
          aria-label={temaEscuro ? 'Ativar tema claro' : 'Ativar tema escuro'}
          className="flex h-10 w-10 items-center justify-center rounded-lg border border-[var(--cor-borda)] bg-[var(--cor-superficie)] text-[var(--cor-texto)] transition-colors hover:bg-jardin-50 dark:hover:bg-slate-800"
        >
          {temaEscuro ? (
            <Sun className="h-5 w-5" aria-hidden="true" />
          ) : (
            <Moon className="h-5 w-5" aria-hidden="true" />
          )}
        </button>
      </header>

      <section className="card">
        <h2 className="text-lg">Frontend inicializado</h2>
        <p className="text-suave mt-2 text-sm">
          Base React + Vite + TypeScript com Tailwind CSS, lucide-react e recharts.
          O dashboard será construído nas próximas etapas.
        </p>
      </section>
    </main>
  )
}

export default App
