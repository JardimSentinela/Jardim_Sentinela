/** @type {import('tailwindcss').Config} */
export default {
  darkMode: 'class',
  content: ['./index.html', './src/**/*.{js,ts,jsx,tsx}'],
  theme: {
    extend: {
      fontFamily: {
        sans: ['Inter', 'system-ui', 'Avenir', 'Helvetica', 'Arial', 'sans-serif'],
      },
      colors: {
        jardin: {
          50: '#effaf3',
          100: '#d8f3e2',
          200: '#b4e6c9',
          300: '#83d2a8',
          400: '#4fb783',
          500: '#2b9b68',
          600: '#1d7d53',
          700: '#186344',
          800: '#164f38',
          900: '#134130',
          950: '#08251b',
        },
        agua: {
          50: '#eff8ff',
          100: '#dbeefe',
          200: '#bfe1fe',
          300: '#93cdfd',
          400: '#60b0fa',
          500: '#3b91f6',
          600: '#2573eb',
          700: '#1d5cd8',
          800: '#1e4caf',
          900: '#1e438a',
          950: '#172b54',
        },
      },
    },
  },
  plugins: [],
}
