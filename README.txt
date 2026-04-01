# 🚀 QuickChat S3: Mini Servidor de Mensajería Offline

QuickChat es un sistema de mensajería instantánea ligero y **100% autónomo** diseñado para el **ESP32-S3**. Permite crear una red de chat privada sin necesidad de internet, ideal para comunicaciones locales, privacidad total o situaciones de emergencia.

## 📸 Vista Previa
| 📱 Interfaz "Obsidian Dark" | ⚡ Hardware en Acción |
| :---: | :---: |
| <img src="IMG_DE_TU_SCREENSHOT.jpg" width="300"> | <img src="IMG_DE_TU_ESP32.jpg" width="300"> |
*El sistema combina una web moderna con respuesta en tiempo real en la pantalla OLED.*

## ✨ Características Principales
* **Infraestructura Totalmente Autónoma**: Funciona como WiFi AP con WebServer (puerto 80) y WebSockets (puerto 81).
* **Visualización Dual**: Mensajes en tiempo real en la web y en pantalla OLED 128x64 (con soporte de emojis).
* **Persistencia con LittleFS**: El historial de chat y el registro de usuarios (IP → Nombre) no se borran al apagar el dispositivo.
* **Arquitectura Dual-Core**: Optimizado con **FreeRTOS**; el Core 0 gestiona la red y el Core 1 la interfaz OLED.
* **Diseño Premium**: Interfaz web oscura tipo "Obsidian", con animaciones suaves y optimizada para móviles.

## 🛠️ Especificaciones Técnicas
* **Microcontrolador**: ESP32-S3 (Freenove WROOM).
* **Pantalla**: OLED SSD1306 128x64 (I2C).
    * `SDA: Pin 8` | `SCL: Pin 9`
* **Alimentación**: 
    * *Actual*: 4 pilas AA en serie (6V) al pin 5V.
    * *Soportado*: Batería LiPo 3.7V con módulo de carga TP4056.

## 📚 Requisitos de Software
Instalar desde el *Library Manager* de Arduino:
1. **Adafruit SSD1306** + **Adafruit GFX**.
2. **WebSockets** (de Markus Sattler).
3. **ArduinoJson** (de Benoit Blanchon).

## ⚙️ Inicio Rápido
1. Carga el código `QuickChat.ino` en tu ESP32-S3.
2. Conéctate a la red WiFi generada:
   - **SSID**: `QuickChat`
   - **Password**: `12345678`
3. Abre tu navegador en: `http://192.168.4.1`

> ⚠️ **Nota de Admin**: Para resetear todo el historial y usuarios, visita `http://192.168.4.1/reset`.

---
🛡️ *Software distribuido bajo la Licencia Pública General **GNU v2**.* 
