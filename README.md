# Godfather Discord Rich Presence

Discord Rich Presence para **The Godfather: The Game** (PC, EA, 2006) — incluida la edición
española *El Padrino*.

Es un plugin `.asi` nativo (DLL de 32 bits) que se carga automáticamente al arrancar el
juego y muestra en tu perfil de Discord que estás jugando, junto con un texto de estado.

![estado en Discord](https://img.shields.io/badge/Discord-Rich%20Presence-5865F2?logo=discord&logoColor=white)
![plataforma](https://img.shields.io/badge/plataforma-Windows%20x86-blue)
![licencia](https://img.shields.io/badge/licencia-MIT-green)

---

## ¿Qué muestra?

En tu perfil de Discord aparece **"Jugando a El Padrino"** con:

- Una línea de estado (actualmente fija: **"Nueva York"**).
- El tiempo de sesión transcurrido (lo cuenta Discord automáticamente).

> **Estado del proyecto:** versión básica funcional. La detección del **barrio/distrito
> concreto** (Hell's Kitchen, Brooklyn, SoHo, …) leyendo la memoria del juego está
> planificada como fase posterior. Ver [docs/](docs/).

---

## Requisitos

- **Windows** (el juego es x86 de 2006).
- **Discord de escritorio** abierto y con sesión iniciada.
- El juego con el **[Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader)**
  instalado (normalmente presente como `dinput8.dll` en la raíz del juego). Es lo que carga
  los plugins `.asi`. Muchas instalaciones modernas (con SilentPatch, WidescreenFix, etc.)
  ya lo tienen.

---

## Instalación (para usuarios)

1. Descarga `GodfatherDiscordRPC.asi` (de la sección *Releases*, o compílalo — ver abajo).
2. Cópialo a la carpeta `scripts\` de tu instalación del juego, junto a los otros `.asi`
   (p. ej. `C:\The Godfather\scripts\`).
3. *(Opcional)* copia también `GodfatherDiscordRPC.ini` a esa misma carpeta si quieres
   personalizar el texto o usar tu propio Client ID de Discord.
4. Abre Discord y arranca el juego. ¡Listo!

**Ficheros necesarios en `scripts\`:**

| Fichero | Imprescindible | Para qué |
|---|---|---|
| `GodfatherDiscordRPC.asi` | Sí | El plugin |
| `GodfatherDiscordRPC.ini` | Opcional | Personalización; sin él se usan los valores por defecto |

---

## Configuración (`GodfatherDiscordRPC.ini`)

```ini
[Discord]
client_id = 1520195420398289017

[Settings]
poll_interval_ms = 2000
state = Nueva York
```

- `client_id`: ID de la aplicación de Discord. Crea la tuya en el
  [Discord Developer Portal](https://discord.com/developers/applications) si quieres que
  aparezca con tu propio nombre tras "Jugando a…".
- `poll_interval_ms`: cada cuánto comprueba el estado / reintenta conexión.
- `state`: el texto de estado mostrado.

Si el `.ini` falta o una clave está vacía, se usan los valores por defecto compilados.

---

## Compilar desde el código fuente

Necesitas **Visual Studio Build Tools 2022** (workload C++ con target x86) y **CMake**.

```powershell
# Forma rápida (usa el script incluido)
.\build.cmd

# Ejecutar los tests
.\build.cmd test
```

O manualmente, desde un *Developer Command Prompt for VS 2022* en arquitectura x86:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

El resultado es `build\GodfatherDiscordRPC.asi` (DLL de 32 bits).

---

## Cómo funciona

- El plugin se inyecta en `godfather.exe` a través del Ultimate ASI Loader.
- Lanza un hilo en segundo plano que se conecta a Discord mediante su **IPC local**
  (named pipe `\\.\pipe\discord-ipc-N`), hace el handshake y envía la actividad
  (`SET_ACTIVITY`). Sin SDK ni dependencias externas.
- Si Discord no está abierto, reintenta la conexión periódicamente sin molestar al juego.
- Todas las operaciones están aisladas para no afectar nunca a la estabilidad del juego.

Genera un log de diagnóstico en `GodfatherDiscordRPC.log`, junto al `.asi`.

---

## Estructura del proyecto

```
src/          Código fuente C++ (Logger, Config, DiscordIPC, dllmain)
test/         Tests: unit_tests (lógica) + ipc_host_test (Discord sin el juego)
dist/         GodfatherDiscordRPC.ini de ejemplo
docs/         Diseño y plan de implementación
build.cmd     Script de compilación x86
```

---

## Aviso legal

Proyecto **no oficial**, sin relación con Electronic Arts ni con los titulares de la marca
*The Godfather* / *El Padrino*. "The Godfather" es una marca de sus respectivos
propietarios. Este software solo añade integración con Discord y no incluye ni distribuye
ningún recurso del juego.

## Licencia

[MIT](LICENSE).
