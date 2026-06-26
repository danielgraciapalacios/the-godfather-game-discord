# Discord Rich Presence para The Godfather (PC, 2006) — Diseño

**Fecha:** 2026-06-27
**Estado:** Aprobado, listo para plan de implementación

## Resumen

Plugin nativo `.asi` (DLL x86) que el juego *The Godfather: The Game* (EA, 2006 — edición
española "El Padrino") carga automáticamente a través del Ultimate ASI Loader ya presente
en la carpeta del juego como `dinput8.dll`. El plugin corre **dentro** del proceso
`godfather.exe`, lee periódicamente el estado del juego desde memoria y publica una
actividad de Rich Presence en Discord mediante el protocolo IPC nativo (named pipe), sin
SDK ni dependencias externas.

**Nivel de detalle objetivo (Intermedio):** mostrar si el jugador está en menú, cargando o
en partida, y en qué distrito/barrio se encuentra.

## Contexto del juego (de `estructura.txt`)

- Juego x86 de 2006, sin código fuente disponible.
- Ya carga plugins `.asi` vía `dinput8.dll` (Ultimate ASI Loader): conviven
  `scripts/SilentPatchGF.asi` y `scripts/TheGodfather.WidescreenFix.asi`. Inyectar nuestro
  código por la misma vía es el patrón nativo y probado.
- Distritos/mapas detectados en `godfather_v4/*_global.str`:
  Brooklyn Grid, Hell's Kitchen, Holland Tunnel, Lincoln Tunnel, Midtown, New Jersey, SoHo,
  Westside Parkway, Woltz Compound (9 zonas).

## Decisiones de diseño (acordadas)

| Decisión | Elección |
|---|---|
| Nivel de detalle | Intermedio: estado (menú/cargando/partida) + distrito |
| Técnica de detección | Plugin ASI en C++ que lee memoria del proceso |
| Conexión con Discord | IPC nativo propio sobre named pipe (sin librería discord-rpc) |
| Direcciones de memoria | Offsets en `.ini` editable, **no** hardcodeados; modo degradado si faltan |
| Contenido del RPC | `state` = "Estado + distrito"; timestamp de sesión (gratis). Sin imágenes ni `details` |
| Toolchain | MSVC + CMake, target x86 (32-bit) |
| Discord Client ID | `1520195420398289017` (valor por defecto en `.ini`, editable) |

## Arquitectura

```
godfather.exe (proceso x86)
│
├── dinput8.dll  (Ultimate ASI Loader — ya existe)
│
└── scripts/GodfatherDiscordRPC.asi   ◄── este proyecto
        │
        ├── [hilo de fondo, loop ~2s]
        │     1. Lee memoria del proceso (offsets desde .ini, relativos a base de módulo)
        │     2. Traduce a estado lógico: Menu / Loading / InGame + districtId
        │     3. Si cambió y han pasado ≥15s → envía SET_ACTIVITY por IPC
        │
        └── Discord IPC (named pipe \\.\pipe\discord-ipc-0..-9)
              handshake + SET_ACTIVITY (JSON), sin dependencias externas
```

### Componentes (responsabilidad única, testables por separado)

| Unidad | Responsabilidad | Depende de |
|---|---|---|
| `Config` | Parsea `GodfatherDiscordRPC.ini` (client id, offsets, tabla de distritos, intervalo) | fichero .ini |
| `GameState` | Lee memoria del juego y la traduce a estado lógico (escena + distrito) | `Config`, memoria del proceso |
| `DiscordIPC` | Protocolo IPC sobre named pipe (handshake + set activity + reconexión) | named pipe de Discord |
| `dllmain` | Entry point `.asi`; arranca/para el hilo de fondo; orquesta el loop | las tres anteriores |

`DiscordIPC` no conoce el juego; `GameState` no conoce Discord; el `.ini` desacopla los
offsets del binario.

## GameState — modelo de estados y lectura de memoria

```cpp
enum class Scene { Unknown, Menu, Loading, InGame };

struct GameState {
    Scene scene;
    int   districtId;   // -1 si no aplica
};
```

**Lectura:**
1. Base del módulo en runtime: `GetModuleHandleA("godfather.exe")`. Todos los offsets del
   `.ini` son **relativos a esa base** (`base + offset`), no absolutos.
2. Como corremos dentro del proceso, leemos directamente por puntero, protegido con
   `__try/__except` (SEH) para no crashear el juego ante un offset inválido.
3. Traducción `districtId` → nombre legible mediante la tabla `[Districts]` del `.ini`.

**Modo degradado (robustez clave):** si los offsets están vacíos o la lectura falla,
`GameState` devuelve `Scene::InGame` genérico sin distrito. El plugin **nunca** depende de
la ingeniería inversa para arrancar: sin offsets muestra "Jugando a El Padrino"; con offsets
añade el distrito y el menú/cargando. Si solo se conoce uno de los dos datos (escena o
distrito), se muestra lo que haya.

## DiscordIPC — protocolo nativo

1. **Conexión:** abrir `\\.\pipe\discord-ipc-0`; si falla, probar `-1`..`-9`. Si ninguno
   abre, Discord no corre → reintentar más tarde.
2. **Handshake** (opcode `0`): `{"v":1,"client_id":"1520195420398289017"}`. Discord responde `READY`.
3. **Set Activity** (opcode `1`, `SET_ACTIVITY`).

**Wire format:** header binario de 8 bytes `[opcode int32 LE][length int32 LE]` + payload JSON.

**Payload de actividad (mínimo — "Estado + distrito"):**
```json
{
  "cmd": "SET_ACTIVITY",
  "args": {
    "pid": 0,
    "activity": {
      "state": "En Hell's Kitchen",
      "timestamps": { "start": 1719446400 }
    }
  },
  "nonce": "<uuid>"
}
```
(`pid` se rellena con `GetCurrentProcessId()` del juego; `start` con el timestamp Unix de inicio de la sesión del plugin.)

**Decisiones:**
- Sin `details` ni imágenes → no hace falta subir assets al portal. El nombre de la
  Application en el portal es lo que Discord muestra como "Jugando a …".
- JSON construido a mano (estructura fija); se escapan comillas/backslash/control chars del
  campo `state` (lleva apóstrofes como "Hell's Kitchen" y tildes). Encapsulado en una
  mini-función de escape; no se añade una librería JSON entera.
- Reconexión: si el pipe cae, se marca desconectado y el loop reintenta el handshake sin spam.
- Anti-spam: solo se envía `SET_ACTIVITY` cuando el estado **cambia**, respetando el
  rate-limit de Discord (~1 update / 15 s).

## dllmain — ciclo de vida, loop y errores

- `DllMain` / `DLL_PROCESS_ATTACH`: lanzar hilo de fondo (nunca bloquear `DllMain`).
- `DLL_PROCESS_DETACH`: señalar parada y `join` limpio del hilo.
- **Loop** (cada `intervalo_ms`, por defecto 2000):
  1. Asegurar conexión IPC (handshake si hace falta).
  2. Leer `GameState`.
  3. Si cambió y han pasado ≥15 s desde el último envío → `SET_ACTIVITY`.
  4. Dormir.

**Manejo de errores (el plugin JAMÁS debe crashear ni colgar el juego):**

| Fallo | Comportamiento |
|---|---|
| `.ini` ausente/corrupto | Defaults internos (Client ID + 9 distritos como fallback en código); log y seguir |
| Offsets vacíos / lectura falla | Modo degradado: "Jugando a El Padrino" sin distrito |
| Discord cerrado | Reintentar conexión periódicamente, sin spam, sin error visible |
| Pipe cae a mitad | Marcar desconectado, reintentar handshake |
| Excepción al leer memoria | `__try/__except` (SEH) alrededor de los accesos |

- **Logging:** fichero propio `GodfatherDiscordRPC.log` junto al `.asi` (no se toca el
  `log.log` del juego), con niveles. Imprescindible para depurar offsets sin debugger.

## Plan de pruebas

- **`DiscordIPC` (aislado, sin juego):** un `.exe` host que abre el pipe y envía una
  actividad fija. Valida handshake + frame + aparición en Discord. Es lo primero a validar.
- **`Config` (unitario):** parsear un `.ini` de ejemplo; comprobar offsets, distritos,
  intervalo, y casos corruptos.
- **`GameState` (memoria simulada):** inyectar un buffer falso; comprobar traducción
  id→distrito y lógica de escenas, sin necesitar el juego.
- **Integración manual:** cargar el `.asi`, arrancar el juego, recorrer distritos y verificar
  que el Rich Presence cambia. Aquí se afinan los offsets reales con Cheat Engine.

## Estructura del proyecto

```
the_godfather-dev/
├── CMakeLists.txt
├── src/
│   ├── dllmain.cpp
│   ├── Config.h / Config.cpp
│   ├── GameState.h / GameState.cpp
│   └── DiscordIPC.h / DiscordIPC.cpp
├── test/
│   ├── ipc_host_test.cpp      (host manual de IPC, sin juego)
│   └── unit_tests.cpp         (Config + GameState)
├── dist/
│   └── GodfatherDiscordRPC.ini  (Client ID + 9 distritos pre-rellenados)
└── docs/
    └── OFFSETS.md             (guía Cheat Engine para encontrar los offsets)
```

El `.asi` final se copia a la carpeta `scripts/` del juego (junto a `SilentPatchGF.asi`),
que es donde el ASI Loader busca los plugins.

### `GodfatherDiscordRPC.ini` (pre-rellenado)

```ini
[Discord]
client_id = 1520195420398289017

[Settings]
poll_interval_ms = 2000

[Memory]
; Offsets relativos a la base del módulo godfather.exe.
; Vacíos = modo degradado. Rellenar tras encontrarlos con Cheat Engine (ver docs/OFFSETS.md).
scene_offset    =
district_offset =

[Districts]
; districtId -> nombre mostrado. IDs provisionales hasta confirmar el orden real con Cheat Engine.
0 = Hell's Kitchen
1 = Midtown
2 = SoHo
3 = Brooklyn
4 = New Jersey
5 = Holland Tunnel
6 = Lincoln Tunnel
7 = Westside Parkway
8 = Woltz Compound
```

## Requisitos previos (una vez)

- **MSVC + CMake** (Visual Studio Build Tools con toolset x86).
- **Discord Application** ya creada; Client ID `1520195420398289017`. El nombre de la
  aplicación en el portal debe ser legible (p. ej. "El Padrino" / "The Godfather"), pues es
  lo que Discord muestra como "Jugando a …".
- **Cheat Engine** para localizar `scene_offset` y `district_offset` durante la fase de
  integración.

## Fuera de alcance (YAGNI)

- Imágenes grandes/pequeñas y `details` en el RPC.
- Estado avanzado (misión, dinero, respeto, salud).
- Soporte para otras versiones del exe distintas de la presente (los offsets son específicos
  del binario parcheado de esta instalación).
