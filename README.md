# Jardim Sentinela

O **Jardim Sentinela** é um projeto acadêmico voltado à adaptação climática urbana, com foco inicial em comunidades vulneráveis do Recife.

A proposta combina **jardins de chuva**, **sensoriamento local** e **comunicação territorial** para observar o comportamento da água durante eventos de chuva, apoiar o monitoramento dos jardins e, futuramente, complementar sistemas de informação e alerta já existentes.

> **Reduzir, observar e comunicar.**

---

## Sobre o projeto

O Jardim Sentinela parte de três frentes principais:

### 🌧️ Reduzir

Os jardins de chuva ajudam a reter temporariamente parte da água do escoamento superficial, favorecendo infiltração e liberação gradual.

O objetivo não é eliminar alagamentos, mas contribuir para reduzir ou atrasar o acúmulo de água em pontos vulneráveis.

### 📡 Observar

Sensores associados aos jardins podem gerar informações como:

- nível da água;
- comportamento de enchimento;
- comportamento de drenagem;
- possível saturação;
- indícios de obstrução ou necessidade de manutenção.

### 🚨 Comunicar

Os dados produzidos pelo sistema poderão futuramente complementar dashboards, totens comunitários e outros mecanismos de comunicação territorial.

O Jardim Sentinela **não substitui alertas oficiais nem órgãos como Defesa Civil, APAC ou CEMADEN**.

O estado físico de um jardim também não deve ser confundido com o estado de emergência de uma comunidade.

---

# Arquitetura conceitual

A arquitetura geral estudada para o projeto é:

```text
Sensor
   ↓
Microcontrolador / unidade de campo
   ↓
LoRa / Wi-Fi / rede móvel
   ↓
Gateway
   ↓
API REST
   ↓
Processamento
   ↓
Armazenamento
   ↓
Dashboard / análise de dados
