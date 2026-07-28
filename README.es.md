# ESPHome TCL Climate — Johnson JN52N / Tuya AC

[🇬🇧 English version](README.md)

ESPHome custom component + YAML para controlar aires acondicionados **Tuya** que en realidad usan el **protocolo TCL propietario** por UART (no el Tuya MCU).

> Probado en un **Johnson JN52N** con módulo **TYWE1S** (ESP8266EX, 2 MB flash).

![Ficha técnica del dispositivo](images/device.jpg)

*Ficha técnica del Johnson JN52N.*

## Origen del componente

Este paquete es un **fork adaptado** del componente de la comunidad **TCL AC para ESPHome**.

- **Repo original**: [`Kannix2005/esphome-tcl-ac`](https://github.com/Kannix2005/esphome-tcl-ac)
  Componente custom de ESPHome basado en reverse engineering del protocolo UART de aires TCL/Realtek RTL8710C.
- **Referencia previa**: [`I-am-nightingale/tclac`](https://github.com/I-am-nightingale/tclac) — variante inicial del protocolo TCL.

Las modificaciones principales respecto al original:
- Adaptación para módulo **TYWE1S** (`esp01_1m`, 2 MB flash) con pines UART **GPIO15/GPIO13**.
- Presets mapeados a los estándar de ESPHome: `ECO`, `SLEEP`, `BOOST` (Turbo).
- Ventilador expuesto con modos estándar (`Auto / Low / Medium / High`) para compatibilidad nativa con iconos de Home Assistant.
- **Silencio (Mute)** expuesto como switch template porque ESPHome no expone setter público para presets custom en componentes externos.
- Swing vertical **y horizontal** motorizados (on/off por eje), mapeados esnifando el bus UART mientras se pulsaba el mando físico.

## Características

- **Modos**: Frío, Calor, Seco, Ventilador, Auto
- **Presets**: Eco, Sleep, Boost (Turbo)
- **Ventilador**: Auto / Bajo / Medio / Alto
- **Silencio (Mute)**: switch independiente (baja velocidad + mute)
- **Swing vertical y horizontal**: on/off por eje desde la tarjeta climate; selects con posiciones Fix/Move (experimentales — la unidad no reporta posiciones)
- **Temperatura**: 16–30 °C
- **Paridad UART EVEN** (requerido por el protocolo TCL)

## Hardware

| Placa | Flash | UART al AC |
|---|---|---|
| `esp01_1m` (TYWE1S) | 2 MB | GPIO15 = TX, GPIO13 = RX |

> Otros módulos ESP8266/ESP32 con el mismo protocolo deberían funcionar cambiando los pines UART.

| ![Trasera de la placa](images/image1.jpg) | ![Módulo TYWE1S](images/image2.jpg) |
|---|---|
| *Trasera de la placa WiFi: los pines de flasheo están serigrafiados. **No hace falta soldar** — con cables dupont asegurando el contacto funciona.* | *Frontal de la placa con el módulo TYWE1S visible, conectada a la máquina.* |

## Estructura del repositorio

```text
.
├── ac-jn52n.yaml                   # YAML principal (ajusta secrets)
├── components/
│   └── tcl_climate/                # Componente custom ESPHome
│       ├── __init__.py
│       ├── climate.py
│       ├── tcl_climate.h
│       └── tcl_climate.cpp
├── firmware/
│   ├── JohnsonJN52N-original_firmware.bin  # Volcado del firmware Tuya original (2 MiB)
│   └── ac-jn52n-esphome.bin        # Firmware ESPHome precompilado (sin credenciales)
└── images/                         # Fotos del hardware
```

## Respaldo del firmware original (muy recomendado)

Antes de flashear nada, vuelca el firmware Tuya original. Solo lleva unos minutos y es tu billete de vuelta al estado de fábrica en cualquier momento.

1. Con el aire **desenchufado de la red**, conecta tu adaptador USB-UART a los pines de flasheo de la placa: **3V3, GND, TX, RX** (están serigrafiados en la trasera — ver foto). Los cables dupont funcionan; solo asegúrate de que hacen buen contacto.
2. Puentea **GPIO0 a GND** y alimenta la placa para entrar en modo download.
3. Vuelca los 2 MiB completos de flash:
   ```bash
   esptool.py --port /dev/ttyUSB0 read_flash 0x0 0x200000 backup.bin
   ```
4. Verifica que el volcado ocupa exactamente **2097152 bytes** y guárdalo a buen recaudo.

Este repo ya incluye mi propio volcado de fábrica: `firmware/JohnsonJN52N-original_firmware.bin`. Debería funcionar en cualquier Johnson JN52N / TCL con TYWE1S, pero **hacer tu propio volcado sigue siendo recomendable**.

### Restaurar el firmware original

Mismo cableado, mismo modo download (GPIO0→GND al alimentar):

```bash
esptool.py --port /dev/ttyUSB0 write_flash 0x0 firmware/JohnsonJN52N-original_firmware.bin
```

## Flasheo de ESPHome

### Opción A: firmware precompilado

`firmware/ac-jn52n-esphome.bin` está listo para flashear (mismo cableado y modo download que arriba):

```bash
esptool.py --port /dev/ttyUSB0 write_flash 0x0 firmware/ac-jn52n-esphome.bin
```

Está compilado **sin credenciales WiFi**, así que en el primer arranque levanta directamente su AP de respaldo con portal cautivo: conéctate a él y configura ahí tu propia WiFi.

### Opción B: compílalo tú mismo

1. Copia la carpeta `components/tcl_climate/` junto a tu YAML.
2. Copia `ac-jn52n.yaml` a tu directorio de ESPHome.
3. Define `wifi_ssid` y `wifi_password` en tu `secrets.yaml` y ajusta `name` / `friendly_name` si lo deseas.
4. Compila y flashea:
   ```bash
   esphome run ac-jn52n.yaml
   ```

## Actualizaciones OTA

El flasheo por cable solo hace falta una vez. A partir de ahí, actualiza por WiFi desde el dashboard de ESPHome o con `esphome run ac-jn52n.yaml`.

## Notas importantes

- **Protocolo TCL**: usa tramas de 55/61 bytes a 9600 baud con paridad EVEN. No es compatible con `tuya:` ni `midea:`.
- **Eco**: byte 7 bit 6 del mensaje de estado.
- **Sleep**: byte 19 bits 6:7 (`0x01` = Default).
- **Turbo/Boost**: bit separado en byte 8 bit 6.
- **Mute/Silencio**: bit separado en el protocolo; se expone como switch template porque ESPHome no permite `custom_preset` desde componentes custom externos.
- **Swing**: byte 10 del estado: bit 6 = vertical on/off, bit 5 = horizontal on/off (descubierto esnifando el UART con el mando físico). En el frame SET: vertical = byte 10 bits 3:5 (`0x07` = on), horizontal = byte 11 bit 3. La unidad no reporta posiciones de lamas (bytes 51/52 siempre `0xFF`).

## Licencia

AGPL-3.0 — úsalo bajo tu propia responsabilidad, especialmente cuando trabajes con tensión de red.
