/** @type {import('tailwindcss').Config} */
export default {
  darkMode: 'class',
  content: ['./index.html', './src/**/*.{js,ts,jsx,tsx}'],
  theme: {
    extend: {
      fontFamily: {
        sans: ['"IBM Plex Sans"', 'system-ui', 'sans-serif'],
        display: ['"Public Sans"', '"IBM Plex Sans"', 'system-ui', 'sans-serif'],
        brand: ['Karla', '"IBM Plex Sans"', 'system-ui', 'sans-serif'],
      },
      colors: {
        estado: {
          seco: '#e4b739',
          enchendo: '#30e4bc',
          transbordando: '#005b7f',
          transbordandoClaro: '#4cc7fb',
          alagado: '#141781',
        },
        canvas: {
          claro: '#ffffff',
          claroAlt: '#f8f8f8',
          escuro: '#2b2b36',
          cardEscuro: '#353443',
          acento: '#071d41',
          textoClaro: '#333333',
          textoEscuro: '#ffffff',
        },
      },
    },
  },
  plugins: [],
}
