# LITE-LED+ // [CYBERWARE PROTOCOL REV.2077]

```text
======================================================================
  _     ___ _____ _____       _     _____ ____        _ 
 | |   |_ _|_   _| ____|     | |   | ____|  _ \   _  | |
 | |    | |  | | |  _|  _____| |   |  _| | | | | (_) | |
 | |___ | |  | | | |___|_____| |___| |___| |_| |  _  |_|
 |_____|___| |_| |_____|     |_____|_____|____/  (_) (_)
======================================================================
              [ HARDWARE GPIO INJECTION // SWITCH LITE ]
```

> **ICE_STATUS:** BYPASSED  
> **TARGET_HARDWARE:** NINTENDO SWITCH LITE (HOAG)  
> **SYSMODULE_TID:** `0100000000000895`  
> **INTERFACE:** TESLA / ULTRAHAND OVERLAY (`lite-led-plus.ovl`)  

---

## // 01. OVERVIEW

`lite-led-plus` is a custom Atmosphère sysmodule and Tesla/Ultrahand overlay designed to intercept and command the hidden HOME button physical LED on **Nintendo Switch Lite (Hoag)** motherboards via the direct `GPIO 0x35000065U` line.

Unlike stock profiles, this fork introduces **0x01 SMART_DAEMON**: an autonomous finite-state machine that hooks hardware interrupts, system sleep transitions, and power state monitors into a cohesive, OEM-like lighting experience.

---

## // 02. PROTOCOL MATRIX [DAEMON LOGIC]

| System Event | LED Behavior | PWM Parameters | Priority |
| :--- | :--- | :--- | :--- |
| **KEY_HOME_INTERCEPT** | Immediate light feedback on button press | `OVERRIDE_HOLD` (1.0s @ 33%) | **High (P1)** |
| **SYS_WAKE_UP** | Distinct double greeting pulse on wake | `PULSE_x2` (1.0s ON / 0.4s OFF / 1.0s ON) | **Medium (P2)** |
| **BATT_CHARGE (<100%)** | Soft sinusoidal breathing glow while charging | `PWM_BREATHE` (85/255 @ 2.0s period) | **Low (P3)** |
| **BATT_FULL (100%)** | Constant dim solid light when fully charged | `PWM_SOLID` (85/255) | **Low (P3)** |
| **IDLE / NET_GAMING** | Complete blackout (LED remains off) | `PWM_KILL` (0/255) | **Base** |

---

## // 03. CYBERDECK HUD (OVERLAY MENU)

* `0x01 // SMART_DAEMON (AUTO)` — Engage the autonomous adaptive state machine.
* `0x02 // OVERRIDE_HIGH [100%]` — Force maximum hardware brightness output.
* `0x03 // OVERRIDE_LOW  [33%]` — Force stealth low-light mode.
* `0x04 // OSCILLATE_BREATHE` — Infinite standalone breathing pulse.
* `0x00 // KILLSWITCH_OFF` — Cut all power to the notification LED.

---

## // 04. DEPLOYMENT (INSTALLATION)

1. Grab the latest compiled package from **[Releases](../../releases)**.
2. Connect your Switch microSD card to your PC (or mount via DBI MTP).
3. Copy the `atmosphere` and `switch` folders to the root of your microSD card.
4. If you have an older version, delete `sdmc:/switch/.overlays/lite-led.ovl` to prevent duplicates.
5. Reboot the console into Atmosphère.

---

## // 05. CREDITS & RECON

* **[marcinsz89](https://github.com/marcinsz89/lite-led)** — original `lite-led` architecture and software PWM implementation.
* **[souldbminerr](https://github.com/souldbminerr)** — reverse engineering of the Lite GPIO address space.

---

<br>

# // РУССКАЯ ВЕРСИЯ (RUSSIAN SECTION)

### // ОПИСАНИЕ

`lite-led-plus` — модифицированный системный модуль Atmosphère и оверлей для низкоуровневого управления скрытым светодиодом кнопки HOME на консолях **Nintendo Switch Lite (Hoag)** через шину `GPIO 0x35000065U`.

Главное нововведение форка — режим **0x01 SMART_DAEMON**, превращающий диод в информативный системный индикатор:
* **Зарядка батареи:** мягкое плавное мерцание («дыхание») в процессе заряда.
* **100% заряд:** переход в статичный тусклый свет, сигнализирующий об окончании зарядки.
* **Кнопка HOME:** вспышка на 1 секунду при нажатии в любой игре или меню.
* **Выход из сна:** двойной секундный импульс при включении экрана.
* **Во время игры:** диод полностью выключен и не отвлекает от экрана.

### // УСТАНОВКА

1. Скачайте архив из раздела **[Releases](../../releases)**.
2. Распакуйте папки `atmosphere` и `switch` в корень карты памяти microSD.
3. В папке `switch/.overlays/` удалите старый файл `lite-led.ovl` (если он был установлен ранее).
4. Перезагрузите Switch Lite и выберите в оверлее пункт `0x01 // SMART_DAEMON (AUTO)`.
