# ESPHome TCL Climate — Johnson JN52N / Tuya AC

ESPHome custom component + YAML para controlar aires acondicionados **Tuya** que en realidad usan el **protocolo TCL propietario** por UART (no el Tuya MCU).

> Probado en un **Johnson JN52N** con módulo **TYWE1S** (ESP8266EX, 2 MB flash).

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

## Requisitos de hardware

| Placa | Flash | UART al AC |
|---|---|---|
| `esp01_1m` (TYWE1S) | 2 MB | GPIO15 = TX, GPIO13 = RX |

> Otros módulos ESP8266/ESP32 con el mismo protocolo deberían funcionar cambiando los pines UART.

## Estructura del repositorio

```
.
├── air-conditioner-salon.yaml   # YAML principal (ajusta secrets)
└── components/
    └── tcl_climate/               # Componente custom ESPHome
        ├── __init__.py
        ├── climate.py
        ├── tcl_climate.h
        └── tcl_climate.cpp
```

## Uso

1. Copia la carpeta `components/tcl_climate/` junto a tu YAML.
2. Copia `air-conditioner-salon.yaml` a tu directorio de ESPHome.
3. Edita las claves WiFi (`!secret ...`) y ajusta `name` / `friendly_name` si lo deseas.
4. Compila y flashea desde ESPHome Builder o CLI:
   ```bash
   esphome run air-conditioner-salon.yaml
   ```

## Notas importantes

- **Protocolo TCL**: usa tramas de 55/61 bytes a 9600 baud con paridad EVEN. No es compatible con `tuya:` ni `midea:`.
- **Eco**: byte 7 bit 6 del mensaje de estado.
- **Sleep**: byte 19 bits 6:7 (`0x01` = Default).
- **Turbo/Boost**: bit separado en byte 8 bit 6.
- **Mute/Silencio**: bit separado en el protocolo; se expone como switch template porque ESPHome no permite `custom_preset` desde componentes custom externos.
- **Swing**: byte 10 del estado: bit 6 = vertical on/off, bit 5 = horizontal on/off (descubierto esnifando el UART con el mando físico). En el frame SET: vertical = byte 10 bits 3:5 (`0x07` = on), horizontal = byte 11 bit 3. La unidad no reporta posiciones de lamas (bytes 51/52 siempre `0xFF`).

## Licencia

MIT — úsalo bajo tu propia responsabilidad.
