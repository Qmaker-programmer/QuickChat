/*
QuickChat es software libre: usted puede redistribuirlo y/o modificarlo
bajo los términos de la Licencia Pública General GNU v2 publicada por
la Free Software Foundation.
 * ╔══════════════════════════════════════════════════════════════════╗
 * ║        QuickChat S3                                                                    ║
 * ║        ESP32-S3 Freenove WROOM                                                         ║
 * ╠══════════════════════════════════════════════════════════════════╣
 * ║  CARACTERÍSTICAS:                                                                      ║
 * ║  ✅ WiFi AP + WebServer (puerto 80) + WebSockets (81)                                  ║
 * ║  ✅ OLED 128x64 con emojis Unicode → glifos propios                                    ║
 * ║  ✅ Registro de usuarios: IP → Nombre (LittleFS JSON)                                  ║
 * ║  ✅ Historial de chat persistente en LittleFS                                          ║
 * ║  ✅ FreeRTOS Dual-Core: OLED en Core 1, red en Core 0                                  ║
 * ║  ✅ UI Web de diseño oscuro premium, animada                                           ║
 * ║  ✅ GET /reset → borrar todo el chat                                                   ║
 * ║  ✅ Thread-safe con mutex y secciones críticas                                         ║
 * ╠══════════════════════════════════════════════════════════════════╣
 * ║  LIBRERÍAS (Instalar via Arduino Library Manager):                                     ║
 * ║  • Adafruit SSD1306  + Adafruit GFX Library                                            ║
 * ║  • WebSockets  (Markus Sattler)                                                        ║
 * ║  • ArduinoJson (Benoit Blanchon) ← para la BD de usuarios                              ║
 * ╠══════════════════════════════════════════════════════════════════╣
 * ║  • Especificaciones:                                                                   ║
 * ║  • Nombre de red: QuickChat                                                            ║
 * ║  • Contraseña: 12345678                                                                ║                                                            ║
 * ╚══════════════════════════════════════════════════════════════════╝
 */

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <LittleFS.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>

// ═══════════════════════════════════════════
//  CONFIG
// ═══════════════════════════════════════════
const char* SSID     = "QuickChat";
const char* PASSWORD = "12345678";

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_SDA      8
#define OLED_SCL      9
#define OLED_ADDR     0x3C

#define FILE_CHAT     "/chat.txt"
#define FILE_USERS    "/users.json"
#define MAX_MSGS      200      // Máximo mensajes guardados
#define MAX_MSG_LEN   280      // Largo máximo por mensaje

// ═══════════════════════════════════════════
//  HARDWARE
// ═══════════════════════════════════════════
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
WebServer        server(80);
WebSocketsServer webSocket(81);

// ═══════════════════════════════════════════
//  ESTADO GLOBAL (protegido con mutex)
// ═══════════════════════════════════════════
SemaphoreHandle_t fsMutex;
SemaphoreHandle_t stateMutex;

struct State {
    String  ultimoMsg   = "Listo...";
    String  ultimoNombre = "---";
    int     usuarios    = 0;
    int     totalMsgs   = 0;
} gState;

// ═══════════════════════════════════════════
//  EMOJI → OLED (Unicode UTF-8 → glif ASCII)
//  El ESP32 recibe UTF-8 desde el browser.
//  La SSD1306 solo tiene ASCII básico.
//  Mapeamos los emojis más comunes a
//  representaciones cortas y legibles.
// ═══════════════════════════════════════════
struct EmojiMap { const char* utf8; const char* glyph; };

static const EmojiMap EMOJIS[] = {
    // Caras
    {"\xF0\x9F\x98\x80", ":D"  },  // 😀
    {"\xF0\x9F\x98\x81", ":D"  },  // 😁
    {"\xF0\x9F\x98\x82", "XD"  },  // 😂
    {"\xF0\x9F\x98\x83", ":D"  },  // 😃
    {"\xF0\x9F\x98\x84", ":)"  },  // 😄
    {"\xF0\x9F\x98\x85", "^.^" },  // 😅
    {"\xF0\x9F\x98\x86", "lol" },  // 😆
    {"\xF0\x9F\x98\x87", "o.o" },  // 😇
    {"\xF0\x9F\x98\x88", ">:)" },  // 😈
    {"\xF0\x9F\x98\x8A", ":)"  },  // 😊
    {"\xF0\x9F\x98\x8B", ":P"  },  // 😋
    {"\xF0\x9F\x98\x8D", ":*"  },  // 😍
    {"\xF0\x9F\x98\x8E", "B)"  },  // 😎
    {"\xF0\x9F\x98\x90", "-.-" },  // 😐
    {"\xF0\x9F\x98\x91", "-_-" },  // 😑
    {"\xF0\x9F\x98\x92", ":/"  },  // 😒
    {"\xF0\x9F\x98\x93", ":s"  },  // 😓
    {"\xF0\x9F\x98\x94", ":("  },  // 😔
    {"\xF0\x9F\x98\x95", "o.o" },  // 😕
    {"\xF0\x9F\x98\x96", ":("  },  // 😖
    {"\xF0\x9F\x98\x97", ":*"  },  // 😗
    {"\xF0\x9F\x98\x98", ":3"  },  // 😘
    {"\xF0\x9F\x98\x99", ":O"  },  // 😙
    {"\xF0\x9F\x98\x9A", "T-T" },  // 😚→😚 (mezcla)
    {"\xF0\x9F\x98\x9B", ">.<" },  // 😛
    {"\xF0\x9F\x98\x9C", ";P"  },  // 😜
    {"\xF0\x9F\x98\x9D", ":P"  },  // 😝
    {"\xF0\x9F\x98\x9E", ":'(" },  // 😞
    {"\xF0\x9F\x98\x9F", "D:"  },  // 😟
    {"\xF0\x9F\x98\xA0", ">:(" },  // 😠
    {"\xF0\x9F\x98\xA1", ">:(" },  // 😡
    {"\xF0\x9F\x98\xA2", ":'(" },  // 😢
    {"\xF0\x9F\x98\xA3", "D:"  },  // 😣
    {"\xF0\x9F\x98\xA4", "x_x" },  // 😤
    {"\xF0\x9F\x98\xA5", "T_T" },  // 😥
    {"\xF0\x9F\x98\xA6", "D:"  },  // 😦
    {"\xF0\x9F\x98\xA7", "D::" },  // 😧
    {"\xF0\x9F\x98\xA8", "O_O" },  // 😨
    {"\xF0\x9F\x98\xA9", "x.x" },  // 😩
    {"\xF0\x9F\x98\xAA", "z_z" },  // 😪
    {"\xF0\x9F\x98\xAB", "z.z" },  // 😫
    {"\xF0\x9F\x98\xAD", ":'(" },  // 😭
    {"\xF0\x9F\x98\xAE", ":O"  },  // 😮
    {"\xF0\x9F\x98\xAF", "o_o" },  // 😯
    {"\xF0\x9F\x98\xB0", "O.O" },  // 😰
    {"\xF0\x9F\x98\xB1", "!_!" },  // 😱
    {"\xF0\x9F\x98\xB2", "O_O" },  // 😲
    {"\xF0\x9F\x98\xB3", ">//< "},  // 😳
    {"\xF0\x9F\x98\xB4", "zzz" },  // 😴
    {"\xF0\x9F\x98\xB5", "@_@" },  // 😵
    {"\xF0\x9F\x98\xB7", ":M"  },  // 😷 mascarilla
    {"\xF0\x9F\x98\xB8", "=^.^="},  // 😸
    {"\xF0\x9F\x98\xB9", ">;3" },  // 😹
    {"\xF0\x9F\x98\xBA", "^.^" },  // 😺
    {"\xF0\x9F\x98\xBB", ":3"  },  // 😻
    {"\xF0\x9F\x98\xBC", "o.o" },  // 😼
    {"\xF0\x9F\x98\xBD", "T.T" },  // 😽
    {"\xF0\x9F\x98\xBE", "x.x" },  // 😾
    {"\xF0\x9F\x98\xBF", ">.<" },  // 😿
    {"\xF0\x9F\x99\x80", "O.O" },  // 🙀
    {"\xF0\x9F\x99\x81", ">:(" },  // 🙁
    {"\xF0\x9F\x99\x82", ":)"  },  // 🙂
    {"\xF0\x9F\x99\x83", "(:)" },  // 🙃
    {"\xF0\x9F\x99\x84", ";)"  },  // 🙄
    {"\xF0\x9F\xA4\x94", "hmm" },  // 🤔
    {"\xF0\x9F\xA4\xA3", "XD"  },  // 🤣
    {"\xF0\x9F\xA4\xA9", "uwu" },  // 🤩
    {"\xF0\x9F\xA4\xAF", "!!"  },  // 🤯
    {"\xF0\x9F\xA4\xB7", "???" },  // 🤷
    {"\xF0\x9F\xA5\xB0", ":3"  },  // 🥰
    {"\xF0\x9F\xA5\xB2", "xD"  },  // 🥲
    {"\xF0\x9F\xA5\xB3", "\\o/"},  // 🥳
    // Gestos
    {"\xF0\x9F\x91\x8D", "+1"  },  // 👍
    {"\xF0\x9F\x91\x8E", "-1"  },  // 👎
    {"\xF0\x9F\x91\x8F", "clap"},  // 👏
    {"\xF0\x9F\x91\x90", "ok!" },  // 👐
    {"\xF0\x9F\x91\x8A", "bam" },  // 👊
    {"\xF0\x9F\x91\x8B", "hi!" },  // 👋
    {"\xF0\x9F\x91\x8C", "ok"  },  // 👌
    {"\xF0\x9F\x91\x8F", "prr" },  // 👏
    {"\xF0\x9F\x99\x8C", "pls" },  // 🙌
    {"\xF0\x9F\xA4\x9D", "deal"},  // 🤝
    {"\xF0\x9F\xA4\x9E", "\\m/"},  // 🤞
    {"\xF0\x9F\xA4\x9F", "ok"  },  // 🤟
    // Corazones
    {"\xE2\x9D\xA4\xEF\xB8\x8F","<3" },  // ❤️
    {"\xF0\x9F\x92\x94", "</3" },  // 💔
    {"\xF0\x9F\x92\x95", "<3"  },  // 💕
    {"\xF0\x9F\x92\x96", "<3"  },  // 💖
    {"\xF0\x9F\x92\x97", "<3"  },  // 💗
    {"\xF0\x9F\x92\x98", "<3"  },  // 💘
    {"\xF0\x9F\x92\x99", "<3"  },  // 💙
    {"\xF0\x9F\x92\x9A", "<3"  },  // 💚
    {"\xF0\x9F\x92\x9B", "<3"  },  // 💛
    {"\xF0\x9F\x92\x9C", "<3"  },  // 💜
    // Símbolos
    {"\xF0\x9F\x94\xA5", "wow" },  // 🔥
    {"\xF0\x9F\x8E\x89", "\\o/"},  // 🎉
    {"\xF0\x9F\x8E\x8A", "pop" },  // 🎊
    {"\xE2\x9C\x85",     "OK"  },  // ✅
    {"\xE2\x9D\x8C",     "NO"  },  // ❌
    {"\xE2\x9A\xA0\xEF\xB8\x8F","!" },   // ⚠️
    {"\xF0\x9F\x92\xAF", "100" },  // 💯
    {"\xF0\x9F\x92\xA5", "POW" },  // 💥
    {"\xF0\x9F\x92\xA4", "zzz" },  // 💤
    {"\xF0\x9F\x92\xA9", "poop"},  // 💩
    {"\xF0\x9F\x91\xBB", "boo" },  // 👻
    {"\xF0\x9F\x91\xBD", "et"  },  // 👽
    {"\xF0\x9F\x92\x80", "RIP" },  // 💀
    {"\xF0\x9F\xA4\x96", "bot" },  // 🤖
    {"\xF0\x9F\x8E\xAE", "game"},  // 🎮
    {"\xF0\x9F\x8F\x86", "win" },  // 🏆
    {"\xF0\x9F\x9A\x80", ">>>" },  // 🚀
    {"\xF0\x9F\x93\xA2", "msg" },  // 📢
    {"\xF0\x9F\x94\x94", "ring"},  // 🔔
    {"\xF0\x9F\x94\x87", "mute"},  // 🔇
    {"\xF0\x9F\x93\xB1", "tel" },  // 📱
    {"\xF0\x9F\x92\xBB", "pc"  },  // 💻
    {"\xF0\x9F\x92\xB0", "$$"  },  // 💰
    {"\xF0\x9F\x93\x8C", "pin" },  // 📌
    {"\xF0\x9F\x93\x8E", "pin" },  // 📎
    {"\xF0\x9F\x93\x9D", "note"},  // 📝
    {"\xF0\x9F\x93\xB0", "news"},  // 📰
    {"\xF0\x9F\x94\x8D", "src" },  // 🔍
    {"\xF0\x9F\x94\x8E", "src" },  // 🔎
    {"\xF0\x9F\x94\x91", "key" },  // 🔑
    {"\xF0\x9F\x94\x92", "lock"},  // 🔒
    {"\xF0\x9F\x94\x93", "open"},  // 🔓
    {"\xF0\x9F\x94\x97", "lnk" },  // 🔗
    {"\xF0\x9F\x94\xA7", "set" },  // 🔧
    {"\xF0\x9F\x94\xA8", "fix" },  // 🔨
    {"\xF0\x9F\x94\xAB", "!"   },  // 🔫
    {"\xF0\x9F\x95\x90", "12am"},  // 🕐
    {"\xF0\x9F\x8C\x99", "moon"},  // 🌙
    {"\xE2\x98\x80\xEF\xB8\x8F","sun"},  // ☀️
    {"\xE2\x9B\x85",     "cld" },  // ⛅
    {"\xF0\x9F\x8C\x88", "rbw" },  // 🌈
    {"\xE2\x9D\x84\xEF\xB8\x8F","ice"},  // ❄️
    {"\xF0\x9F\x8C\x8A", "wav" },  // 🌊
    {"\xF0\x9F\x8C\x8D", "wrld"},  // 🌍
    {"\xF0\x9F\x8C\x8E", "wrld"},  // 🌎
    {"\xF0\x9F\x8C\x8F", "wrld"},  // 🌏
    // Números y letras en emojis
    {"\x30\xEF\xB8\x8F\xE2\x83\xA3","0"},  // 0️⃣
    {"\x31\xEF\xB8\x8F\xE2\x83\xA3","1"},  // 1️⃣
};
static const int EMOJI_COUNT = sizeof(EMOJIS) / sizeof(EMOJIS[0]);

String filtrarUnicodeOLED(String s) {
    for (int i = 0; i < EMOJI_COUNT; i++) {
        s.replace(EMOJIS[i].utf8, EMOJIS[i].glyph);
    }
    // Eliminar cualquier byte > 0x7E que quede (caracteres multi-byte no mapeados)
    String resultado = "";
    resultado.reserve(s.length());
    for (int i = 0; i < (int)s.length(); ) {
        uint8_t c = (uint8_t)s[i];
        if (c < 0x80) {
            // ASCII normal
            resultado += (char)c;
            i++;
        } else if (c >= 0xF0) {
            // Emoji 4 bytes → ignorar
            i += 4;
        } else if (c >= 0xE0) {
            // 3 bytes (muchos emojis ya mapeados) → ignorar resto
            i += 3;
        } else if (c >= 0xC0) {
            // 2 bytes (acentos etc) → reemplazar por vocal base
            uint8_t c2 = (i+1 < (int)s.length()) ? (uint8_t)s[i+1] : 0;
            // Mapeo básico de latin extendido
            if (c == 0xC3) {
                switch(c2) {
                    case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4: case 0xA5: resultado+='a'; break;
                    case 0xA8: case 0xA9: case 0xAA: case 0xAB: resultado+='e'; break;
                    case 0xAC: case 0xAD: case 0xAE: case 0xAF: resultado+='i'; break;
                    case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6: resultado+='o'; break;
                    case 0xB9: case 0xBA: case 0xBB: case 0xBC: resultado+='u'; break;
                    case 0xB1: resultado+='n'; break;
                    case 0x80: resultado+='A'; break;
                    case 0x81: resultado+='A'; break;
                    case 0x89: resultado+='E'; break;
                    case 0x8D: resultado+='I'; break;
                    case 0x93: resultado+='O'; break;
                    case 0x9A: resultado+='U'; break;
                    case 0x91: resultado+='N'; break;
                    default:   resultado+='?'; break;
                }
            } else {
                resultado += '?';
            }
            i += 2;
        } else {
            i++;
        }
    }
    return resultado;
}

// ═══════════════════════════════════════════
//  BASE DE DATOS USUARIOS: IP → Nombre
//  Formato: JSON array en /users.json
//  [{"ip":"192.168.4.2","name":"Pepito"}, ...]
// ═══════════════════════════════════════════

String obtenerNombre(const String& ip) {
    if (xSemaphoreTake(fsMutex, pdMS_TO_TICKS(500)) != pdTRUE) return ip;
    String nombre = "";
    if (LittleFS.exists(FILE_USERS)) {
        File f = LittleFS.open(FILE_USERS, "r");
        if (f) {
            DynamicJsonDocument doc(4096);
            DeserializationError err = deserializeJson(doc, f);
            f.close();
            if (!err) {
                JsonArray arr = doc.as<JsonArray>();
                for (JsonObject obj : arr) {
                    if (obj["ip"].as<String>() == ip) {
                        nombre = obj["name"].as<String>();
                        break;
                    }
                }
            }
        }
    }
    xSemaphoreGive(fsMutex);
    return nombre;
}

void guardarNombre(const String& ip, const String& nombre) {
    if (xSemaphoreTake(fsMutex, pdMS_TO_TICKS(1000)) != pdTRUE) return;
    DynamicJsonDocument doc(4096);
    if (LittleFS.exists(FILE_USERS)) {
        File f = LittleFS.open(FILE_USERS, "r");
        if (f) { deserializeJson(doc, f); f.close(); }
    }
    if (!doc.is<JsonArray>()) doc.to<JsonArray>();
    JsonArray arr = doc.as<JsonArray>();
    // Actualizar si ya existe
    bool found = false;
    for (JsonObject obj : arr) {
        if (obj["ip"].as<String>() == ip) {
            obj["name"] = nombre;
            found = true;
            break;
        }
    }
    if (!found) {
        JsonObject nuevo = arr.createNestedObject();
        nuevo["ip"]   = ip;
        nuevo["name"] = nombre;
    }
    File fw = LittleFS.open(FILE_USERS, "w");
    if (fw) { serializeJson(doc, fw); fw.close(); }
    xSemaphoreGive(fsMutex);
}

// Devuelve JSON con todos los usuarios para la UI
String getAllUsersJson() {
    if (xSemaphoreTake(fsMutex, pdMS_TO_TICKS(500)) != pdTRUE) return "[]";
    String out = "[]";
    if (LittleFS.exists(FILE_USERS)) {
        File f = LittleFS.open(FILE_USERS, "r");
        if (f) { out = f.readString(); f.close(); }
    }
    xSemaphoreGive(fsMutex);
    return out;
}

// ═══════════════════════════════════════════
//  CHAT — LITTLEFS
// ═══════════════════════════════════════════

// Un mensaje guardado es una línea JSON:
// {"t":1234,"ip":"192.168.4.2","name":"Pepito","msg":"Hola"}
void guardarMsg(const String& ip, const String& nombre, const String& msg) {
    if (xSemaphoreTake(fsMutex, pdMS_TO_TICKS(500)) != pdTRUE) return;
    File f = LittleFS.open(FILE_CHAT, "a");
    if (f) {
        DynamicJsonDocument doc(512);
        doc["t"]    = millis() / 1000;
        doc["ip"]   = ip;
        doc["name"] = nombre;
        doc["msg"]  = msg.substring(0, MAX_MSG_LEN);
        serializeJson(doc, f);
        f.println();
        f.close();
    }
    xSemaphoreGive(fsMutex);
}

// Lee historial como JSON array (para enviar al cliente al conectar)
String leerHistorialJson() {
    if (xSemaphoreTake(fsMutex, pdMS_TO_TICKS(500)) != pdTRUE) return "[]";
    String out = "[";
    if (LittleFS.exists(FILE_CHAT)) {
        File f = LittleFS.open(FILE_CHAT, "r");
        bool first = true;
        while (f.available()) {
            String linea = f.readStringUntil('\n');
            linea.trim();
            if (linea.length() > 2) {
                if (!first) out += ",";
                out += linea;
                first = false;
            }
        }
        f.close();
    }
    out += "]";
    xSemaphoreGive(fsMutex);
    return out;
}

void borrarChat() {
    if (xSemaphoreTake(fsMutex, pdMS_TO_TICKS(1000)) != pdTRUE) return;
    LittleFS.remove(FILE_CHAT);
    xSemaphoreGive(fsMutex);
}

// ═══════════════════════════════════════════
//  TAREA OLED — Core 1
// ═══════════════════════════════════════════
void TaskOLED(void* pv) {
    uint8_t tick = 0;
    const char* spinner[] = {"|","/","-","\\"};

    for (;;) {
        display.clearDisplay();
        display.setTextColor(SSD1306_WHITE);

        // — Título + spinner —
        display.setTextSize(1);
        display.setCursor(0, 0);
        display.print("QuickChat ");
        display.print(spinner[tick % 4]);

        // — Usuarios online —
        display.setCursor(88, 0);
        display.print("u:");
        if (xSemaphoreTake(stateMutex, 0) == pdTRUE) {
            display.print(gState.usuarios);
            xSemaphoreGive(stateMutex);
        }

        display.drawFastHLine(0, 9, 128, SSD1306_WHITE);

        // — Último mensaje —
        String nombre = "";
        String msg    = "";
        if (xSemaphoreTake(stateMutex, 0) == pdTRUE) {
            nombre = gState.ultimoNombre;
            msg    = gState.ultimoMsg;
            xSemaphoreGive(stateMutex);
        }

        display.setTextSize(1);
        display.setCursor(0, 12);
        display.print(nombre + ":");

        // Filtrar Unicode → ASCII legible
        String oledMsg = filtrarUnicodeOLED(msg);

        display.setCursor(0, 23);
        if (oledMsg.length() <= 10) {
            display.setTextSize(2);
            display.println(oledMsg.substring(0, 10));
        } else {
            display.setTextSize(1);
            // Wrap manual: 21 chars por línea, max 2 líneas
            display.println(oledMsg.substring(0, 21));
            if (oledMsg.length() > 21)
                display.println(oledMsg.substring(21, 42));
        }

        // — Info inferior —
        display.drawFastHLine(0, 54, 128, SSD1306_WHITE);
        display.setTextSize(1);
        display.setCursor(0, 56);
        display.print(WiFi.softAPIP().toString());
        display.setCursor(90, 56);
        String ts = String(millis()/60000) + "min";
        display.print(ts);

        display.display();
        tick++;
        vTaskDelay(pdMS_TO_TICKS(400));
    }
}

// ═══════════════════════════════════════════
//  WEBSOCKET EVENTS
// ═══════════════════════════════════════════
void wsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t len) {
    switch (type) {
        case WStype_CONNECTED: {
            String ip = webSocket.remoteIP(num).toString();
            if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                gState.usuarios++;
                xSemaphoreGive(stateMutex);
            }

            // Verificar si ya tiene nombre registrado
            String nombre = obtenerNombre(ip);

            // Construir payload de bienvenida
            DynamicJsonDocument welcome(8192);
            welcome["type"]    = "init";
            welcome["ip"]      = ip;
            welcome["name"]    = nombre;          // vacío si no registrado
            welcome["history"] = serialized(leerHistorialJson());
            welcome["users"]   = serialized(getAllUsersJson());

            String welStr;
            serializeJson(welcome, welStr);
            webSocket.sendTXT(num, welStr);

            // Broadcast contador
            DynamicJsonDocument cnt(64);
            cnt["type"]  = "users";
            cnt["count"] = gState.usuarios;
            String cntStr; serializeJson(cnt, cntStr);
            webSocket.broadcastTXT(cntStr);
            break;
        }
        case WStype_DISCONNECTED: {
            if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                if (gState.usuarios > 0) gState.usuarios--;
                xSemaphoreGive(stateMutex);
            }
            DynamicJsonDocument cnt(64);
            cnt["type"]  = "users";
            cnt["count"] = gState.usuarios;
            String cntStr; serializeJson(cnt, cntStr);
            webSocket.broadcastTXT(cntStr);
            break;
        }
        case WStype_TEXT: {
            String ip  = webSocket.remoteIP(num).toString();
            String raw = String((char*)payload);
            raw.trim();
            if (raw.length() == 0) break;

            // Parsear JSON del cliente
            DynamicJsonDocument inDoc(512);
            DeserializationError err = deserializeJson(inDoc, raw);

            if (!err && inDoc.containsKey("type")) {
                String mtype = inDoc["type"].as<String>();

                // — Registro de nombre —
                if (mtype == "register") {
                    String nombre = inDoc["name"].as<String>();
                    nombre.trim();
                    if (nombre.length() < 1 || nombre.length() > 32) break;
                    guardarNombre(ip, nombre);

                    // Confirmar al cliente
                    DynamicJsonDocument ack(128);
                    ack["type"] = "registered";
                    ack["name"] = nombre;
                    String ackStr; serializeJson(ack, ackStr);
                    webSocket.sendTXT(num, ackStr);

                    // Broadcast lista de usuarios actualizada
                    DynamicJsonDocument upd(8192);
                    upd["type"]  = "users_update";
                    upd["users"] = serialized(getAllUsersJson());
                    String updStr; serializeJson(upd, updStr);
                    webSocket.broadcastTXT(updStr);
                    break;
                }

                // — Mensaje de chat —
                if (mtype == "msg") {
                    String texto = inDoc["msg"].as<String>();
                    texto.trim();
                    if (texto.length() == 0 || texto.length() > MAX_MSG_LEN) break;

                    String nombre = obtenerNombre(ip);
                    if (nombre.length() == 0) nombre = ip;  // Fallback

                    guardarMsg(ip, nombre, texto);

                    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                        gState.ultimoMsg    = texto;
                        gState.ultimoNombre = nombre;
                        gState.totalMsgs++;
                        xSemaphoreGive(stateMutex);
                    }

                    // Broadcast el mensaje a todos
                    DynamicJsonDocument out(768);
                    out["type"]  = "msg";
                    out["ip"]    = ip;
                    out["name"]  = nombre;
                    out["msg"]   = texto;
                    out["t"]     = millis() / 1000;
                    String outStr; serializeJson(out, outStr);
                    webSocket.broadcastTXT(outStr);
                    break;
                }
            }
            break;
        }
        default: break;
    }
}

// ═══════════════════════════════════════════
//  INTERFAZ WEB — HTML/CSS/JS
//  Estética: Obsidian Dark — Elegante, moderna
//  Tipografía: JetBrains Mono + DM Sans
// ═══════════════════════════════════════════
const char HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>QuickChat</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=DM+Sans:ital,wght@0,300;0,400;0,500;0,600;1,400&family=JetBrains+Mono:wght@400;600&display=swap" rel="stylesheet">
<style>
:root {
  --bg:       #0a0a0f;
  --surface:  #12121a;
  --surface2: #1a1a26;
  --border:   #ffffff0f;
  --border2:  #ffffff1a;
  --accent:   #7c6af7;
  --accent2:  #a594f9;
  --accent3:  #c4b5fd;
  --text:     #e8e8f0;
  --text2:    #9090a8;
  --text3:    #606078;
  --green:    #4ade80;
  --red:      #f87171;
  --gold:     #fbbf24;
  --r:        12px;
  --r2:       20px;
}
*, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
html, body { height: 100%; overflow: hidden; }

body {
  font-family: 'DM Sans', system-ui, sans-serif;
  background: var(--bg);
  color: var(--text);
  display: flex;
  flex-direction: column;
}

/* Fondo animado de partículas */
body::before {
  content: '';
  position: fixed; inset: 0;
  background:
    radial-gradient(ellipse 80% 60% at 20% 10%, #7c6af71a 0%, transparent 60%),
    radial-gradient(ellipse 60% 40% at 80% 90%, #4ade801a 0%, transparent 50%);
  pointer-events: none; z-index: 0;
}

/* ── MODAL REGISTRO ── */
#modal-overlay {
  position: fixed; inset: 0;
  background: rgba(0,0,0,0.85);
  backdrop-filter: blur(12px);
  z-index: 999;
  display: flex; align-items: center; justify-content: center;
  padding: 20px;
}
#modal-overlay.hidden { display: none; }

#modal-box {
  background: var(--surface);
  border: 1px solid var(--border2);
  border-radius: var(--r2);
  padding: 40px 36px;
  max-width: 420px; width: 100%;
  text-align: center;
  box-shadow: 0 32px 80px rgba(0,0,0,0.7), 0 0 0 1px var(--border);
  animation: slideUp 0.4s cubic-bezier(.16,1,.3,1) both;
}
@keyframes slideUp {
  from { transform: translateY(30px); opacity: 0; }
  to   { transform: translateY(0);    opacity: 1; }
}
#modal-box .logo {
  font-family: 'JetBrains Mono', monospace;
  font-size: 2.2rem;
  background: linear-gradient(135deg, var(--accent), var(--accent3));
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
  margin-bottom: 8px;
}
#modal-box h2 {
  font-size: 1.25rem; font-weight: 600;
  color: var(--text); margin-bottom: 6px;
}
#modal-box p {
  color: var(--text2); font-size: 0.88rem;
  margin-bottom: 28px; line-height: 1.5;
}
#modal-box .input-wrap {
  position: relative; margin-bottom: 16px;
}
#modal-box .input-wrap input {
  width: 100%;
  background: var(--surface2);
  border: 1.5px solid var(--border2);
  border-radius: var(--r);
  padding: 14px 18px;
  color: var(--text);
  font-family: inherit; font-size: 1rem;
  outline: none;
  transition: border-color 0.2s, box-shadow 0.2s;
}
#modal-box .input-wrap input:focus {
  border-color: var(--accent);
  box-shadow: 0 0 0 3px #7c6af722;
}
#modal-box .input-wrap input::placeholder { color: var(--text3); }

.btn-primary {
  width: 100%;
  background: linear-gradient(135deg, var(--accent), #9b8bfa);
  color: white; border: none;
  border-radius: var(--r);
  padding: 14px;
  font-family: inherit; font-size: 1rem; font-weight: 600;
  cursor: pointer;
  transition: opacity 0.2s, transform 0.15s;
  box-shadow: 0 8px 24px #7c6af733;
}
.btn-primary:hover  { opacity: 0.9; transform: translateY(-1px); }
.btn-primary:active { transform: translateY(0); }
.btn-primary:disabled { opacity: 0.4; cursor: not-allowed; }

/* ── LAYOUT PRINCIPAL ── */
#app {
  flex: 1; display: flex; flex-direction: column;
  height: 100%; overflow: hidden; position: relative; z-index: 1;
}

/* Header */
#header {
  background: var(--surface);
  border-bottom: 1px solid var(--border);
  padding: 0 16px;
  display: flex; align-items: center; gap: 12px;
  height: 62px; flex-shrink: 0;
}
#header .logo-sm {
  font-family: 'JetBrains Mono', monospace;
  font-weight: 600; font-size: 1rem;
  background: linear-gradient(135deg, var(--accent), var(--accent3));
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
}
#header .sep { flex: 1; }
#status-dot {
  width: 9px; height: 9px; border-radius: 50%;
  background: var(--red);
  box-shadow: 0 0 8px var(--red);
  transition: all 0.3s;
}
#status-dot.on { background: var(--green); box-shadow: 0 0 8px var(--green); }
#user-count-badge {
  background: var(--surface2);
  border: 1px solid var(--border2);
  border-radius: 20px;
  padding: 3px 10px; font-size: 0.78rem; color: var(--text2);
  font-family: 'JetBrains Mono', monospace;
}
#my-name-badge {
  background: linear-gradient(135deg, var(--accent)22, var(--accent3)22);
  border: 1px solid var(--accent)44;
  border-radius: 20px;
  padding: 3px 12px; font-size: 0.82rem; color: var(--accent3);
  font-weight: 500; cursor: pointer;
  transition: background 0.2s;
}
#my-name-badge:hover {
  background: linear-gradient(135deg, var(--accent)33, var(--accent3)33);
}

/* Chat area */
#chat-wrap {
  flex: 1; overflow-y: auto;
  padding: 20px 16px;
  display: flex; flex-direction: column;
  gap: 4px;
  scroll-behavior: smooth;
}
#chat-wrap::-webkit-scrollbar { width: 4px; }
#chat-wrap::-webkit-scrollbar-track { background: transparent; }
#chat-wrap::-webkit-scrollbar-thumb { background: var(--border2); border-radius: 2px; }

/* Burbujas */
.msg-group { display: flex; flex-direction: column; gap: 2px; margin-bottom: 14px; }
.msg-group.mine { align-items: flex-end; }
.msg-group .sender {
  font-size: 0.75rem; font-weight: 600;
  color: var(--text3); padding: 0 6px; margin-bottom: 3px;
}
.msg-group.mine .sender { color: var(--accent2); }

.bubble {
  max-width: min(76%, 500px);
  padding: 10px 14px;
  border-radius: 18px;
  font-size: 0.95rem;
  line-height: 1.5;
  word-break: break-word;
  animation: bubbleIn 0.22s cubic-bezier(.16,1,.3,1) both;
  position: relative;
}
@keyframes bubbleIn {
  from { transform: scale(0.9) translateY(8px); opacity: 0; }
  to   { transform: scale(1)   translateY(0);   opacity: 1; }
}
.msg-group:not(.mine) .bubble {
  background: var(--surface2);
  border: 1px solid var(--border);
  border-bottom-left-radius: 6px;
  color: var(--text);
}
.msg-group.mine .bubble {
  background: linear-gradient(135deg, var(--accent), #9b8bfa);
  border-bottom-right-radius: 6px;
  color: white;
  box-shadow: 0 4px 16px #7c6af733;
}
.bubble .ts {
  font-size: 0.65rem;
  margin-top: 4px; display: block;
  opacity: 0.55;
  text-align: right;
  font-family: 'JetBrains Mono', monospace;
}

/* Mensajes del sistema */
.sys-msg {
  text-align: center;
  font-size: 0.75rem;
  color: var(--text3);
  padding: 6px 12px;
  background: var(--surface2);
  border: 1px solid var(--border);
  border-radius: 20px;
  align-self: center;
  margin: 4px 0;
  animation: fadeIn 0.3s ease both;
}
@keyframes fadeIn { from { opacity:0; } to { opacity:1; } }

/* Día divider */
.day-divider {
  display: flex; align-items: center; gap: 12px;
  color: var(--text3); font-size: 0.72rem;
  font-family: 'JetBrains Mono', monospace;
  margin: 10px 0;
}
.day-divider::before, .day-divider::after {
  content: ''; flex:1; height:1px; background: var(--border);
}

/* Input area */
#input-area {
  padding: 12px 16px;
  padding-bottom: max(12px, env(safe-area-inset-bottom));
  background: var(--surface);
  border-top: 1px solid var(--border);
  display: flex; align-items: flex-end; gap: 10px;
}
#msg-input {
  flex: 1;
  background: var(--surface2);
  border: 1.5px solid var(--border2);
  border-radius: 22px;
  padding: 11px 18px;
  color: var(--text);
  font-family: inherit; font-size: 0.97rem;
  resize: none; outline: none;
  max-height: 120px; min-height: 44px;
  transition: border-color 0.2s, box-shadow 0.2s;
  line-height: 1.4;
}
#msg-input:focus {
  border-color: var(--accent);
  box-shadow: 0 0 0 3px #7c6af722;
}
#msg-input::placeholder { color: var(--text3); }
#send-btn {
  width: 44px; height: 44px;
  background: linear-gradient(135deg, var(--accent), #9b8bfa);
  border: none; border-radius: 50%;
  color: white; cursor: pointer;
  font-size: 1.1rem;
  display: flex; align-items: center; justify-content: center;
  flex-shrink: 0;
  transition: transform 0.15s, opacity 0.2s;
  box-shadow: 0 4px 14px #7c6af744;
}
#send-btn:hover  { transform: scale(1.08); }
#send-btn:active { transform: scale(0.95); }
#send-btn:disabled { opacity: 0.35; cursor: not-allowed; }

/* Emoji picker simple */
#emoji-btn {
  width: 36px; height: 36px;
  background: none; border: none;
  color: var(--text2); font-size: 1.3rem;
  cursor: pointer; border-radius: 50%;
  display: flex; align-items:center; justify-content:center;
  transition: background 0.15s;
  flex-shrink: 0;
}
#emoji-btn:hover { background: var(--surface2); }

#emoji-panel {
  position: absolute;
  bottom: 75px; left: 16px; right: 16px;
  background: var(--surface);
  border: 1px solid var(--border2);
  border-radius: var(--r2);
  padding: 12px;
  display: none;
  flex-wrap: wrap; gap: 6px;
  z-index: 50;
  box-shadow: 0 16px 48px rgba(0,0,0,0.6);
  animation: slideUp 0.2s ease both;
  max-height: 180px; overflow-y: auto;
}
#emoji-panel.open { display: flex; }
#emoji-panel button {
  background: none; border: none;
  font-size: 1.35rem; cursor: pointer;
  padding: 4px; border-radius: 8px;
  transition: background 0.1s;
  line-height: 1;
}
#emoji-panel button:hover { background: var(--surface2); }

/* Toast */
#toast {
  position: fixed; top: 80px; left: 50%; transform: translateX(-50%);
  background: var(--surface2);
  border: 1px solid var(--border2);
  padding: 9px 20px;
  border-radius: 20px;
  font-size: 0.82rem; color: var(--text);
  z-index: 200;
  opacity: 0; pointer-events: none;
  transition: opacity 0.25s;
  box-shadow: 0 8px 24px rgba(0,0,0,0.5);
  white-space: nowrap;
}
#toast.show { opacity: 1; }

/* Reconexión overlay */
#reconnect-overlay {
  position: fixed; inset: 0;
  background: rgba(0,0,0,0.8);
  backdrop-filter: blur(8px);
  z-index: 500;
  display: flex; flex-direction:column;
  align-items: center; justify-content: center;
  gap: 16px;
}
#reconnect-overlay.hidden { display: none; }
#reconnect-overlay p {
  color: var(--text2); font-size: 0.9rem;
}
.spinner {
  width: 36px; height: 36px;
  border: 3px solid var(--border2);
  border-top-color: var(--accent);
  border-radius: 50%;
  animation: spin 0.8s linear infinite;
}
@keyframes spin { to { transform: rotate(360deg); } }

/* Sidebar usuarios (slide) */
#sidebar {
  position: fixed; right: 0; top: 0; bottom: 0;
  width: 260px;
  background: var(--surface);
  border-left: 1px solid var(--border);
  transform: translateX(100%);
  transition: transform 0.3s cubic-bezier(.16,1,.3,1);
  z-index: 100;
  display: flex; flex-direction: column;
}
#sidebar.open { transform: translateX(0); }
#sidebar-header {
  padding: 20px 16px 12px;
  border-bottom: 1px solid var(--border);
  font-weight: 600; font-size: 0.95rem;
  display: flex; align-items:center; justify-content:space-between;
}
#sidebar-close {
  background: none; border: none;
  color: var(--text2); font-size: 1.2rem;
  cursor: pointer; padding: 4px;
  border-radius: 6px;
}
#sidebar-close:hover { background: var(--surface2); }
#users-list {
  flex:1; overflow-y:auto; padding: 12px;
  display: flex; flex-direction:column; gap:8px;
}
.user-item {
  display: flex; align-items: center; gap: 10px;
  padding: 10px 12px;
  background: var(--surface2);
  border: 1px solid var(--border);
  border-radius: var(--r);
}
.user-avatar {
  width: 34px; height: 34px;
  border-radius: 50%;
  background: linear-gradient(135deg, var(--accent), var(--accent3));
  display: flex; align-items:center; justify-content:center;
  font-weight: 600; font-size: 0.85rem; color: white;
  flex-shrink: 0;
}
.user-info { min-width:0; }
.user-name { font-size:0.88rem; font-weight:500; white-space:nowrap; overflow:hidden; text-overflow:ellipsis; }
.user-ip   { font-size:0.72rem; color:var(--text3); font-family:'JetBrains Mono',monospace; }

/* Sidebar trigger */
#users-btn {
  background: var(--surface2);
  border: 1px solid var(--border2);
  border-radius: 20px;
  padding: 3px 10px; font-size: 0.78rem; color: var(--text2);
  cursor: pointer; transition: background 0.15s;
  font-family: inherit;
}
#users-btn:hover { background: var(--border2); color: var(--text); }
</style>
</head>
<body>

<!-- MODAL REGISTRO -->
<div id="modal-overlay">
  <div id="modal-box">
    <div class="logo">QC</div>
    <h2>Bienvenido a QuickChat</h2>
    <p>Primera vez que te conectas. Elige un nombre — lo verán todos en el chat.</p>
    <div class="input-wrap">
      <input type="text" id="reg-name" placeholder="Tu nombre..." maxlength="32" autocomplete="off" autocorrect="off" spellcheck="false">
    </div>
    <button class="btn-primary" id="reg-btn" onclick="register()">Entrar al chat →</button>
  </div>
</div>

<!-- RECONEXIÓN -->
<div id="reconnect-overlay" class="hidden">
  <div class="spinner"></div>
  <p>Reconectando...</p>
</div>

<!-- TOAST -->
<div id="toast"></div>

<!-- SIDEBAR USUARIOS -->
<div id="sidebar">
  <div id="sidebar-header">
    <span>👥 Usuarios registrados</span>
    <button id="sidebar-close" onclick="closeSidebar()">✕</button>
  </div>
  <div id="users-list"></div>
</div>

<!-- APP -->
<div id="app">
  <div id="header">
    <span class="logo-sm">◈ QuickChat</span>
    <span id="my-name-badge" onclick="openRename()" title="Cambiar nombre">⠀</span>
    <span class="sep"></span>
    <button id="users-btn" onclick="openSidebar()">👥 <span id="user-count">0</span></button>
    <span id="status-dot"></span>
  </div>

  <div id="chat-wrap"></div>

  <div id="emoji-panel" id="emoji-panel"></div>

  <div id="input-area" style="position:relative">
    <button id="emoji-btn" onclick="toggleEmoji()" title="Emojis">😊</button>
    <textarea id="msg-input" rows="1" placeholder="Escribe un mensaje..." maxlength="280"></textarea>
    <button id="send-btn" onclick="sendMsg()">
      <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><line x1="22" y1="2" x2="11" y2="13"/><polygon points="22 2 15 22 11 13 2 9 22 2"/></svg>
    </button>
  </div>
</div>

<script>
// ═══════════════════
//  ESTADO
// ═══════════════════
var ws;
var myIP   = '';
var myName = '';
var reconnTimer;
var emojiOpen = false;
var allUsers  = {};  // ip → name

var EMOJIS_COMMON = [
  '😀','😂','🤣','😊','😍','🥰','😎','😭','😅','🤔',
  '👍','👎','👏','🙌','🤝','💪','🙏','🔥','💯','❤️',
  '💔','🎉','🎊','✅','❌','⚠️','🚀','💻','📱','🔑',
  '😢','😡','🤯','😴','🤖','👻','💀','💩','🌈','⭐',
  '🏆','🎮','💰','📢','🔔','🔍','🎵','🍕','🍺','☀️'
];

// ═══════════════════
//  WEBSOCKET
// ═══════════════════
function connect() {
  ws = new WebSocket('ws://' + location.hostname + ':81/');

  ws.onopen = function() {
    clearTimeout(reconnTimer);
    document.getElementById('status-dot').classList.add('on');
    document.getElementById('reconnect-overlay').classList.add('hidden');
  };

  ws.onclose = function() {
    document.getElementById('status-dot').classList.remove('on');
    document.getElementById('reconnect-overlay').classList.remove('hidden');
    reconnTimer = setTimeout(connect, 3000);
  };

  ws.onmessage = function(e) {
    var data;
    try { data = JSON.parse(e.data); } catch(err) { return; }
    handleMessage(data);
  };
}

function handleMessage(d) {
  switch(d.type) {
    case 'init':
      myIP   = d.ip   || '';
      myName = d.name || '';
      // Cargar usuarios
      if (d.users) parseUsers(d.users);
      // Mostrar historial
      if (d.history && d.history.length > 0) {
        d.history.forEach(function(m) { renderMsg(m, true); });
        scrollBottom();
      }
      // ¿Necesita registrarse?
      if (!myName) {
        document.getElementById('modal-overlay').classList.remove('hidden');
      } else {
        document.getElementById('modal-overlay').classList.add('hidden');
        updateNameBadge();
      }
      break;

    case 'registered':
      myName = d.name;
      document.getElementById('modal-overlay').classList.add('hidden');
      updateNameBadge();
      showToast('¡Bienvenido, ' + myName + '! 👋');
      break;

    case 'msg':
      renderMsg(d);
      scrollBottom();
      break;

    case 'users':
      document.getElementById('user-count').textContent = d.count;
      break;

    case 'users_update':
      if (d.users) parseUsers(d.users);
      renderSidebar();
      break;
  }
}

// ═══════════════════
//  USUARIOS
// ═══════════════════
function parseUsers(usersRaw) {
  var arr = (typeof usersRaw === 'string') ? JSON.parse(usersRaw) : usersRaw;
  allUsers = {};
  arr.forEach(function(u) { allUsers[u.ip] = u.name; });
  renderSidebar();
}

function renderSidebar() {
  var list = document.getElementById('users-list');
  list.innerHTML = '';
  Object.keys(allUsers).forEach(function(ip) {
    var name = allUsers[ip];
    var initials = name.substring(0,2).toUpperCase();
    var item = document.createElement('div');
    item.className = 'user-item';
    item.innerHTML =
      '<div class="user-avatar">' + initials + '</div>' +
      '<div class="user-info">' +
        '<div class="user-name">' + esc(name) + '</div>' +
        '<div class="user-ip">' + ip + '</div>' +
      '</div>';
    list.appendChild(item);
  });
  if (Object.keys(allUsers).length === 0) {
    list.innerHTML = '<p style="color:var(--text3);font-size:0.82rem;text-align:center;padding:20px">Sin usuarios registrados aún</p>';
  }
}

function openSidebar()  { document.getElementById('sidebar').classList.add('open'); }
function closeSidebar() { document.getElementById('sidebar').classList.remove('open'); }

// ═══════════════════
//  REGISTRO
// ═══════════════════
function register() {
  var input = document.getElementById('reg-name');
  var name  = input.value.trim();
  if (name.length < 1) { input.focus(); return; }
  if (name.length > 32) { showToast('Máx 32 caracteres'); return; }
  if (!ws || ws.readyState !== 1) { showToast('Sin conexión'); return; }
  document.getElementById('reg-btn').disabled = true;
  ws.send(JSON.stringify({ type: 'register', name: name }));
}

function openRename() {
  document.getElementById('modal-overlay').classList.remove('hidden');
  document.getElementById('reg-name').value = myName;
  document.getElementById('reg-btn').disabled = false;
  setTimeout(function(){ document.getElementById('reg-name').focus(); }, 100);
}

function updateNameBadge() {
  var badge = document.getElementById('my-name-badge');
  badge.textContent = '✏️ ' + myName;
}

// ═══════════════════
//  MENSAJES
// ═══════════════════
function sendMsg() {
  var ta  = document.getElementById('msg-input');
  var txt = ta.value.trim();
  if (!txt || !ws || ws.readyState !== 1) return;
  ws.send(JSON.stringify({ type: 'msg', msg: txt }));
  ta.value = '';
  ta.style.height = '';
  if (emojiOpen) toggleEmoji();
}

function renderMsg(m, fromHistory) {
  var chat = document.getElementById('chat-wrap');
  var isMe = (m.ip === myIP);

  // Buscar grupo existente del mismo sender (últimos 2 elementos)
  var lastGroup = chat.lastElementChild;
  var senderAttr = lastGroup ? lastGroup.getAttribute('data-sender') : null;
  var group;

  if (lastGroup && lastGroup.classList.contains('msg-group') && senderAttr === m.ip) {
    group = lastGroup;
  } else {
    group = document.createElement('div');
    group.className = 'msg-group' + (isMe ? ' mine' : '');
    group.setAttribute('data-sender', m.ip);

    var sender = document.createElement('div');
    sender.className = 'sender';
    var displayName = allUsers[m.ip] || m.name || m.ip;
    sender.textContent = isMe ? 'Tú' : displayName;
    group.appendChild(sender);

    chat.appendChild(group);
  }

  var bubble = document.createElement('div');
  bubble.className = 'bubble';

  var timeStr = formatTime(m.t);
  bubble.innerHTML = esc(m.msg).replace(/\n/g,'<br>') +
    '<span class="ts">' + timeStr + '</span>';

  group.appendChild(bubble);
}

function formatTime(secs) {
  if (!secs) {
    var now = new Date();
    return now.getHours().toString().padStart(2,'0') + ':' +
           now.getMinutes().toString().padStart(2,'0');
  }
  var d = new Date(secs * 1000);
  return d.getHours().toString().padStart(2,'0') + ':' +
         d.getMinutes().toString().padStart(2,'0');
}

function scrollBottom() {
  var c = document.getElementById('chat-wrap');
  setTimeout(function(){ c.scrollTop = c.scrollHeight; }, 50);
}

// ═══════════════════
//  EMOJI PANEL
// ═══════════════════
function buildEmojiPanel() {
  var panel = document.getElementById('emoji-panel');
  EMOJIS_COMMON.forEach(function(em) {
    var btn = document.createElement('button');
    btn.textContent = em;
    btn.onclick = function() {
      var ta = document.getElementById('msg-input');
      ta.value += em;
      ta.focus();
    };
    panel.appendChild(btn);
  });
}

function toggleEmoji() {
  emojiOpen = !emojiOpen;
  document.getElementById('emoji-panel').classList.toggle('open', emojiOpen);
}

// ═══════════════════
//  UTILIDADES
// ═══════════════════
function esc(s) {
  return String(s)
    .replace(/&/g,'&amp;')
    .replace(/</g,'&lt;')
    .replace(/>/g,'&gt;')
    .replace(/"/g,'&quot;');
}

function showToast(msg) {
  var t = document.getElementById('toast');
  t.textContent = msg;
  t.classList.add('show');
  setTimeout(function(){ t.classList.remove('show'); }, 2800);
}

// Auto-resize textarea
document.getElementById('msg-input').addEventListener('input', function() {
  this.style.height = '';
  this.style.height = Math.min(this.scrollHeight, 120) + 'px';
});

document.getElementById('msg-input').addEventListener('keydown', function(e) {
  if (e.key === 'Enter' && !e.shiftKey) {
    e.preventDefault();
    sendMsg();
  }
});

document.getElementById('reg-name').addEventListener('keydown', function(e) {
  if (e.key === 'Enter') register();
});

// Cerrar emoji al click fuera
document.addEventListener('click', function(e) {
  if (emojiOpen &&
      !e.target.closest('#emoji-panel') &&
      e.target.id !== 'emoji-btn') {
    toggleEmoji();
  }
});

// Init
buildEmojiPanel();
connect();
</script>
</body>
</html>
)rawhtml";

// ═══════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════
void setup() {
    Serial.begin(115200);
    Serial.println("\n🖤 QuickChat S3 V5 — Obsidian Edition");

    // Mutexes
    fsMutex    = xSemaphoreCreateMutex();
    stateMutex = xSemaphoreCreateMutex();

    // LittleFS
    if (!LittleFS.begin(true)) {
        Serial.println("❌ LittleFS error");
    } else {
        Serial.println("✅ LittleFS OK");
        // Mostrar espacio
        Serial.printf("   Total: %u KB | Usado: %u KB\n",
            LittleFS.totalBytes()/1024, LittleFS.usedBytes()/1024);
    }

    // WiFi AP
    WiFi.softAP(SSID, PASSWORD);
    Serial.printf("📶 AP: %s | IP: %s\n", SSID, WiFi.softAPIP().toString().c_str());

    // OLED
    Wire.begin(OLED_SDA, OLED_SCL);
    if (display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        display.clearDisplay();
        display.setTextColor(SSD1306_WHITE);
        display.setTextSize(1);
        display.setCursor(8,10); display.println("QuickChat V5");
        display.setCursor(8,24); display.println("Obsidian Edition");
        display.setCursor(8,38); display.print(WiFi.softAPIP().toString());
        display.display();
        Serial.println("✅ OLED OK");

        xTaskCreatePinnedToCore(TaskOLED, "OLED", 8192, NULL, 2, NULL, 1);
    }

    // HTTP routes
    server.on("/", HTTP_GET, []() {
        server.send_P(200, "text/html; charset=utf-8", HTML);
    });

    // /reset → borrar chat
    server.on("/reset", HTTP_GET, []() {
        borrarChat();
        // Notificar a todos los clientes
        DynamicJsonDocument doc(64);
        doc["type"] = "sys";
        doc["msg"]  = "🗑️ Chat reseteado";
        String s; serializeJson(doc, s);
        webSocket.broadcastTXT(s);
        server.send(200, "text/html; charset=utf-8",
            "<html><body style='font-family:monospace;background:#0a0a0f;color:#e8e8f0;"
            "display:flex;align-items:center;justify-content:center;height:100vh;margin:0'>"
            "<div style='text-align:center'>"
            "<div style='font-size:3rem;margin-bottom:16px'>🗑️</div>"
            "<h2>Chat borrado correctamente</h2>"
            "<p style='color:#606078;margin-top:8px'>Volver al <a href='/' style='color:#7c6af7'>chat</a></p>"
            "</div></body></html>");
        Serial.println("🗑️ Chat reseteado via /reset");
    });

    server.on("/users", HTTP_GET, []() {
        server.send(200, "application/json; charset=utf-8", getAllUsersJson());
    });

    server.begin();

    // WebSocket
    webSocket.begin();
    webSocket.onEvent(wsEvent);
    webSocket.enableHeartbeat(15000, 3000, 2);

    Serial.println("✅ Todo listo! Conecta a la red '" + String(SSID) + "'");
    Serial.println("   Web: http://" + WiFi.softAPIP().toString());
    Serial.println("   Reset: http://" + WiFi.softAPIP().toString() + "/reset");
}

// ═══════════════════════════════════════════
//  LOOP — Core 0
// ═══════════════════════════════════════════
void loop() {
    webSocket.loop();
    server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(1));
}
