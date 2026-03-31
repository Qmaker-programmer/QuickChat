# QuickChat S3 🚀

QuickChat es un sistema de mensajería instantánea ligero y autónomo diseñado específicamente para el **ESP32-S3 (Freenove WROOM)**. Permite crear una red de chat local sin necesidad de internet, utilizando un punto de acceso WiFi (AP) y una interfaz web moderna.

## ✨ Características Principales
* **Infraestructura Autónoma**: Funciona como WiFi AP con WebServer (puerto 80) y WebSockets (puerto 81) integrados.
* **Visualización Dual**: Los mensajes se muestran tanto en la interfaz web como en una pantalla OLED 128x64 con soporte de emojis Unicode convertidos a glifos ASCII.
* **Persistencia de Datos**: Registro de usuarios (IP → Nombre) e historial de chat almacenados de forma persistente en el sistema de archivos **LittleFS**.
* **Rendimiento Optimizado**: Implementación mediante **FreeRTOS Dual-Core**, gestionando la pantalla OLED en el Core 1 y la red en el Core 0.
* **Seguridad y Robustez**: Sistema *thread-safe* con uso de mutex y secciones críticas para la gestión del estado global.
* **Interfaz Premium**: UI web con diseño oscuro tipo "Obsidian Dark", animada y optimizada para dispositivos móviles.

## 🛠️ Requisitos de Hardware
* **Microcontrolador**: ESP32-S3 (Probado en Freenove WROOM).
* **Pantalla**: OLED SSD1306 128x64 (I2C).
    * SDA: Pin 8
    * SCL: Pin 9

## 📚 Librerías Necesarias
Es necesario instalar las siguientes librerías desde el Arduino Library Manager:
1.  **Adafruit SSD1306** + **Adafruit GFX Library**.
2.  **WebSockets** (de Markus Sattler).
3.  **ArduinoJson** (de Benoit Blanchon).

## ⚙️ Configuración por Defecto
Al iniciar, el dispositivo creará una red con las siguientes credenciales:
* **SSID**: `QuickChat`
* **Contraseña**: `12345678`
* **Acceso Web**: `http://192.168.4.1`

## 🚀 Instalación
1.  Asegúrate de tener configurado el soporte para ESP32 en tu IDE de Arduino.
2.  Instala las librerías mencionadas anteriormente.
3.  Carga el código `QuickChat.ino` en tu placa ESP32-S3.
4.  Conéctate a la red WiFi "QuickChat" desde tu móvil o PC y abre el navegador.

## 📋 Comandos Administrativos
* Para resetear y borrar todo el historial del chat, accede a: `http://192.168.4.1/reset`.

---
*Este software se distribuye bajo los términos de la Licencia Pública General GNU v2.