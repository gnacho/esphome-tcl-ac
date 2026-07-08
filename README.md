# ESPHome TCL Climate — Johnson JN52N / Tuya AC

ESPHome custom component + YAML para controlar aires acondicionados **Tuya** que en realidad usan el **protocolo TCL propietario** por UART (no el Tuya MCU).

> Probado en un **Johnson JN52N** con módulo **TYWE1S** (ESP8266EX, 2 MB flash).

---

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
- Swing horizontal comentado en YAML (el modelo JN52N no tiene motorizado horizontal).

## Características

- **Modos**: Frío, Calor, Seco, Ventilador, Auto
- **Presets**: Eco, Sleep, Boost (Turbo)
- **Ventilador**: Auto / Bajo / Medio / Alto
- **Silencio (Mute)**: switch independiente (baja velocidad + mute)
- **Swing vertical**: 7 posiciones (incluyendo oscilación completa)
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
- **Swing horizontal**: en el modelo JN52N es ajuste manual/físico, no motorizado. No se expone en el YAML.

---

# English

ESPHome custom component + YAML to control **Tuya** air conditioners that actually use the **proprietary TCL protocol** over UART (not the Tuya MCU).

> Tested on a **Johnson JN52N** with **TYWE1S** module (ESP8266EX, 2 MB flash).

## Component Origin

This package is an **adapted fork** of the community **TCL AC for ESPHome** component.

- **Original repo**: [`Kannix2005/esphome-tcl-ac`](https://github.com/Kannix2005/esphome-tcl-ac)  
  Custom ESPHome component based on reverse engineering of the TCL/Realtek RTL8710C AC UART protocol.
- **Earlier reference**: [`I-am-nightingale/tclac`](https://github.com/I-am-nightingale/tclac) — initial TCL protocol variant.

Main modifications from the original:
- Adapted for **TYWE1S** module (`esp01_1m`, 2 MB flash) with UART pins **GPIO15/GPIO13**.
- Presets mapped to ESPHome standards: `ECO`, `SLEEP`, `BOOST` (Turbo).
- Fan exposed with standard modes (`Auto / Low / Medium / High`) for native Home Assistant icon compatibility.
- **Mute** exposed as a template switch because ESPHome does not expose a public setter for custom presets from external custom components.
- Horizontal swing commented out in YAML (JN52N model does not have motorized horizontal swing).

## Features

- **Modes**: Cool, Heat, Dry, Fan, Auto
- **Presets**: Eco, Sleep, Boost (Turbo)
- **Fan**: Auto / Low / Medium / High
- **Mute**: independent switch (low speed + mute)
- **Vertical swing**: 7 positions (including full sweep)
- **Temperature**: 16–30 °C
- **UART EVEN parity** (required by TCL protocol)

## Hardware Requirements

| Board | Flash | UART to AC |
|---|---|---|
| `esp01_1m` (TYWE1S) | 2 MB | GPIO15 = TX, GPIO13 = RX |

> Other ESP8266/ESP32 modules with the same protocol should work by changing the UART pins.

## Repository Structure

```
.
├── air-conditioner-salon.yaml   # Main YAML (adjust secrets)
└── components/
    └── tcl_climate/               # Custom ESPHome component
        ├── __init__.py
        ├── climate.py
        ├── tcl_climate.h
        └── tcl_climate.cpp
```

## Usage

1. Copy the `components/tcl_climate/` folder next to your YAML.
2. Copy `air-conditioner-salon.yaml` to your ESPHome directory.
3. Edit WiFi secrets (`!secret ...`) and adjust `name` / `friendly_name` as desired.
4. Compile and flash from ESPHome Builder or CLI:
   ```bash
   esphome run air-conditioner-salon.yaml
   ```

## Important Notes

- **TCL Protocol**: uses 55/61 byte frames at 9600 baud with EVEN parity. Not compatible with `tuya:` or `midea:`.
- **Eco**: byte 7 bit 6 of the status message.
- **Sleep**: byte 19 bits 6:7 (`0x01` = Default).
- **Turbo/Boost**: separate bit at byte 8 bit 6.
- **Mute/Silencio**: separate bit in the protocol; exposed as a template switch because ESPHome does not allow `custom_preset` from external custom components.
- **Horizontal swing**: on the JN52N model this is manual/physical adjustment, not motorized. Not exposed in the YAML.

## License

MIT — use at your own risk.
