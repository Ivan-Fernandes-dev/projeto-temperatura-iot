# Projeto EmbarcaTech - Monitoramento de Temperatura com Wi-Fi

## 📌 Descrição
Sistema embarcado utilizando a BitDogLab (RP2040) para monitoramento de temperatura via Wi-Fi.

O sistema realiza a leitura da temperatura interna do microcontrolador e disponibiliza os dados em uma página web acessível pelo celular.

---

## ⚙️ Funcionalidades

- Leitura de temperatura em tempo real (°C)
- Exibição do IP no monitor serial
- Servidor HTTP embarcado
- Página web com:
  - Temperatura atual em Celsius
  - Status (Seguro / Perigo)
  - Botão para desligar alerta
- Acionamento de:
  - LED vermelho
  - Buzzer (PWM)

---

## 🚨 Lógica de funcionamento

- Temperatura abaixo do limite estabelecido:
  - Status: ✅ Temperatura segura
  - LED e buzzer desligados

- Temperatura acima do limite estabelecido:
  - Status: ⚠️ Perigo! Temperatura acima do limite
  - LED e buzzer ativados
  - Botão disponível para desativar alerta

---

## 📡 Comunicação

- Wi-Fi (modo Station)
- Servidor HTTP

---

## 🛠️ Tecnologias utilizadas

- Linguagem C
- Raspberry Pi Pico W (RP2040)
- SDK Pico
- PWM
- ADC (sensor interno de temperatura)

---

## 👨‍💻 Autor

Ivan Fernandes
