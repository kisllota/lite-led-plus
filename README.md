<div align="center">

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

# LITE-LED+ // [CYBERWARE PROTOCOL]

**Autonomous Smart Notification LED Sysmodule & Tesla HUD for Nintendo Switch Lite (Hoag)**

[![Build Status](https://github.com/kisllota/lite-led-plus/actions/workflows/build.yml/badge.svg)](https://github.com/kisllota/lite-led-plus/actions/workflows/build.yml)
[![Platform](https://img.shields.io/badge/Target-Nintendo%20Switch%20Lite%20(Hoag)-e60012.svg?logo=nintendo-switch&logoColor=white)](https://github.com/kisllota/lite-led-plus)
[![Atmosphere](https://img.shields.io/badge/Atmosph%C3%A8re-1.0%2B-blue.svg)](https://github.com/Atmosphere-NX/Atmosphere)
[![Tesla Compatible](https://img.shields.io/badge/Tesla%20Overlay-v1.1.0-brightgreen.svg)](https://github.com/WerWolv/Tesla-Menu)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

<br/>

<img src="led.jpeg" alt="Lite-LED+ in action on Nintendo Switch Lite" width="540" style="border-radius: 8px; box-shadow: 0 4px 16px rgba(0,0,0,0.4);" />

<br/>

**[English Documentation](#-overview)** | **[Русская документация](#-русская-версия-russian-documentation)**

</div>

---

## ⚡ Overview

`lite-led-plus` is an advanced **Atmosphère sysmodule** and companion **Tesla / Ultrahand overlay** built specifically for the **Nintendo Switch Lite (Hoag motherboard)**. 

Every Nintendo Switch Lite motherboard features a physical blue notification LED beneath the translucent ring of the **HOME button**. However, Nintendo's official firmware never illuminates it. Previous homebrew implementations relied on naive software sleep loops that caused high CPU consumption, visible flickering, or complete driver lockouts whenever a USB-C charger was plugged or unplugged.

`lite-led-plus` solves this with an industrial-grade **Smart Daemon**: an autonomous finite-state machine running on core 3 that monitors hardware interrupts, display backlight power states, and battery telemetry to deliver a seamless, OEM-like lighting experience with zero impact on gameplay performance.

---

## 🎯 Key Features

* **0x01 SMART DAEMON (FSM)**: Autonomous state machine that handles button clicks, console wake-up greetings, and battery charging states without manual intervention.
* **Flicker-Free Delta-Sigma DAC Engine**: High-frequency software PWM powered by a first-order Delta-Sigma accumulator running at a rock-solid 500 Hz (2 ms tick). Completely overcomes Horizon OS thread scheduling limits to provide silky-smooth analog brightness transitions.
* **Self-Healing Session Recovery**: Automatically detects USB-C power rail transitions and re-arms the Tegra X1 Pinmux to prevent the LED driver from locking up or bricking after disconnect.
* **Sub-Millisecond Home Intercept**: Direct kernel event hooking for instant visual feedback on button presses.
* **Tesla & Ultrahand Cyberdeck HUD**: On-the-fly mode switching, brightness overrides, and live telemetry in any game or system menu.
* **Minimal Footprint**: Low memory footprint (~256 KB heap), pinned to CPU Core 3, 0% game frame drops.

---

## 📊 Protocol Matrix [Daemon Logic]

In default **Smart Daemon** mode (`custom`), the sysmodule autonomously prioritizes events:

| Priority | Trigger / Event | Visual Feedback | Lighting Curve | Behavior |
| :---: | :--- | :--- | :--- | :--- |
| **P1 (Highest)** | **Home Button Press** | Tactile Glow | 33% Brightness Solid | Lights up for **1.0s** on press in any game or menu |
| **P2** | **Console Wake-Up** | Double Greeting Pulse | 33% Brightness Pulse | **1.0s ON -> 0.4s OFF -> 1.0s ON** welcome signal |
| **P3** | **Charging (< 100%)** | Smooth Breathing | 0% to 33% Sine Wave | Gentle **2.0s period** breathing glow |
| **P3** | **Charging (100% Full)** | Solid Indicator | 33% Brightness Solid | Constant dim light signaling battery is full |
| **Base** | **Active Gaming / Idle** | Stealth Blackout | LED Off (0%) | Completely dark so nothing distracts from the display |

---

## 🎮 Cyberdeck HUD (Tesla Overlay)

Activate the Tesla overlay at any time by pressing your configured hotkey (default: **L + D-Pad Down + R3**):

```text
┌──────────────────────────────────────────────┐
│  LITE-LED+                 v1.1.0 // CYBERDECK│
├──────────────────────────────────────────────┤
│  [SYSTEM PROFILE]                            │
│  > Smart Daemon                       [ACTIVE]│
│                                              │
│  [MANUAL OVERRIDE]                           │
│  > Solid High                            100%│
│  > Solid Low                              33%│
│  > Breathing                            PULSE│
│  > LED Output                             OFF│
│                                              │
│  [TELEMETRY]                                 │
│  SYS: ONLINE // HW_GPIO: 0x65                │
│  DAEMON: SMART_MODE.HEX                      │
└──────────────────────────────────────────────┘
```

* **Smart Daemon**: Activates autonomous state machine logic.
* **Solid High**: Locks LED to 100% maximum output (flashlight / inspection mode).
* **Solid Low**: Locks LED to 33% brightness (soft ambient mode).
* **Breathing**: Continuous infinite sinusoidal breathing wave.
* **LED Output (Off)**: Cuts all power to the LED line.

---

## 📦 Installation

### Prerequisites
* Nintendo Switch Lite (**Hoag** motherboard only — regular Switch and OLED lack this GPIO route).
* Custom Firmware: **Atmosphère 1.0.0+**.
* System Firmware: **HOS 9.0.0 — 19.x+**.
* Optional: **Tesla-Menu** or **Ultrahand Overlay Menu** for the in-game HUD.

### Step-by-Step
1. Download the latest release archive (`lite-led-plus.zip`) from **[Releases](../../releases)** (or download the latest build artifact from **[GitHub Actions](../../actions)**).
2. Connect your Switch microSD card to your computer (or mount it via **DBI MTP Responder**).
3. Extract the contents of the archive directly to the root of your microSD card.
4. If you had the old legacy `lite-led` installed, remove `sdmc:/switch/.overlays/lite-led.ovl` to prevent duplicates.
5. Reboot your console into Atmosphère.
6. Open your Tesla / Ultrahand overlay and select **Smart Daemon**.

### MicroSD Directory Structure
```text
sdmc:/
├── atmosphere/
│   └── contents/
│       └── 0100000000000895/
│           ├── flags/
│           │   └── boot2.flag
│           ├── exefs.nsp
│           └── toolbox.json
└── switch/
    └── .overlays/
        └── lite-led-plus.ovl
```

---

## 🛠️ Technical Breakdown & Architecture

### The Hardware Line
The Switch Lite notification LED is wired directly to Tegra X1 Pinmux pad mapped to internal device code `0x35000065U` under GPIO port controller mode `3U`. It has no direct connection to the Joy-Con microcontroller (`hid:bus`), requiring low-level kernel GPIO manipulation via `libnx`.

### Why Stock PWM Failed
Previous implementations tried calling `svcSleepThread` with microsecond periods (e.g. 66 µs or 200 µs) to generate software PWM. In Horizon OS, the kernel scheduler rounds up thread sleep calls to the next scheduler tick (~1 ms). This turned any duty cycle calculation into a distorted 500 Hz square wave locked at ~50% brightness, ruining subtle breathing effects.

### The Delta-Sigma Solution
`lite-led-plus` replaces raw delay loops with a **first-order Delta-Sigma modulator (Bresenham DAC)** clocked at 500 Hz (`LITE_TICK_NS = 2000000ULL` = 2 ms). The accumulator computes density modulation on the fly:
```c
accumulator += currentBrightness;
if (accumulator >= 255) {
    accumulator -= 255;
    nextGpioLevel = true;
} else {
    nextGpioLevel = false;
}
```
Combined with dirty-state IPC filtering (only dispatching commands to `gpioPadSetValue` when the logical pin output actually toggles), CPU utilization drops to virtually zero while producing visually flawless analog brightness gradients.

### Sleep Mode Architecture (SC7 Suspend)
When Nintendo Switch enters deep sleep (SC7 suspend), the Tegra X1 CPU cores (Cortex-A57) are fully clock-gated and powered off. No userland thread or sysmodule can execute instructions during SC7. When the console resumes, `lite-led-plus` instantly catches the ARM tick leap, restores the hardware Pinmux to GPIO mode via `initLiteBackend()`, and fires the signature double wake pulse.

---

## 👥 Credits & Recon

* **[marcinsz89](https://github.com/marcinsz89/lite-led)** — Original `lite-led` architecture and concept.
* **[souldbminerr](https://github.com/souldbminerr)** — Reverse-engineering of the Nintendo Switch Lite GPIO pad addresses.
* **[WerWolv](https://github.com/WerWolv)** — Creator of libtesla and the Tesla Menu framework.
* **devkitPro Team** — Toolchain and `libnx` libraries.

---
---

<br/>

# 🇷🇺 Русская версия (Russian Documentation)

## ⚡ Описание

`lite-led-plus` — системный модуль Atmosphère и сопутствующий оверлей для **Tesla / Ultrahand**, созданные специально для портативной консоли **Nintendo Switch Lite (плата Hoag)**.

На материнской плате каждой консоли Switch Lite под прозрачным кольцом вокруг кнопки **HOME** распаян физический синий светодиод. В официальной прошивке Nintendo он программно отключен и никогда не светится. Предыдущие любительские решения использовали примитивные задержки микросекундного таймера, из-за чего диод сильно мерцал, тратил ресурсы процессора, а при подключении или отключении кабеля зарядки зависал и переставал реагировать на кнопку.

`lite-led-plus` полностью решает эти проблемы. Встроенный автономный демон (**Smart Daemon**) работает на независимом 3-м ядре процессора, отслеживает прерывания кнопки, статус подсветки экрана и телеметрию контроллера питания, превращая диод в информативный и стильный системный индикатор уровня заводской прошивки.

---

## 🎯 Ключевые возможности

* **Режим 0x01 SMART DAEMON**: Полностью автоматическое управление индикатором без необходимости ручной настройки.
* **Мягкий ШИМ без мерцания (Delta-Sigma DAC)**: Программный широтно-импульсный модулятор на базе дельта-сигма аккумулятора с частотой 500 Гц (шаг 2 мс). Обоходит ограничения планировщика Horizon OS и обеспечивает аналоговую плавность эффекта дыхания.
* **Самовосстанавливающаяся сессия GPIO**: Автоматический перехват шины Pinmux при подключении/отключении кабеля Type-C. Индикатор больше не зависает намертво после отключения зарядки.
* **Моментальный отклик на кнопку HOME**: Аппаратный перехват прерывания кнопки обеспечивает мгновенную световую отдачу.
* **Tesla / Ultrahand HUD**: Быстрое переключение профилей, принудительное включение света и статусная строка прямо во время игры.
* **Нулевое влияние на игры**: Модуль потребляет менее 256 КБ оперативной памяти и работает на служебном ядре CPU, исключая просадки FPS.

---

## 📊 Матрица работы авторежима (Smart Daemon)

В основном режиме **Smart Daemon** (`custom`) модуль самостоятельно переключает состояния по приоритетам:

| Приоритет | Событие системы | Реакция светодиода | Параметры яркости | Описание поведения |
| :---: | :--- | :--- | :--- | :--- |
| **P1 (Высший)** | **Нажатие кнопки HOME** | Световой отклик | 33% (Solid Low) | Загорается ровно на **1.0 секунду** при клике в любой игре или меню |
| **P2** | **Выход из режима сна** | Приветственный импульс | 33% (Двойная вспышка) | **1.0 с светит -> 0.4 с пауза -> 1.0 с светит** при включении экрана |
| **P3** | **Идет зарядка (< 100%)** | Плавное «дыхание» | 0% - 33% (Синусоида) | Мягкое затухание и нарастание света с периодом **2.0 секунды** |
| **P3** | **Батарея заряжена (100%)** | Постоянный индикатор | 33% (Solid Low) | Ровный неяркий свет, сигнализирующий о полном заряде АКБ |
| **Базовый** | **Обычная игра / Меню** | Скрытый режим (Стелс) | Выключен (0%) | Диод полностью погашен, чтобы не отвлекать от экрана консоли |

---

## 🎮 Меню оверлея Tesla

Меню оверлея открывается в любой момент комбинацией клавиш (по умолчанию: **L + Стрелка вниз + Нажатие правого стика R3**):

* **Smart Daemon**: Включение интеллектуального автоматического режима (рекомендуется по умолчанию).
* **Solid High**: 100% максимальная яркость (режим фонарика / проверки).
* **Solid Low**: 33% мягкая приглушенная яркость.
* **Breathing**: Непрерывное синусоидальное пульсирование («дыхание»).
* **LED Output**: Полное отключение диода.

В нижней части меню отображается блок телеметрии, подтверждающий активность системного модуля и шины `GPIO 0x65`.

---

## 📦 Инструкция по установке

### Что необходимо
* Консоль **Nintendo Switch Lite** (на фатках и OLED физически нет этого диода на данной линии).
* Прошивка **Atmosphère 1.0.0+**.
* Версия системного ПО: **HOS от 9.0.0 до 19.x+**.
* Установленный **Tesla-Menu** или **Ultrahand Overlay**.

### Порядок действий
1. Скачайте архив со сборкой (`lite-led-plus.zip`) из раздела **[Releases](../../releases)** или из артефактов **[GitHub Actions](../../actions)**.
2. Подключите microSD карту к компьютеру (или включите режим MTP Responder в приложении **DBI**).
3. Скопируйте папки `atmosphere` и `switch` в корень карты памяти с подтверждением слияния.
4. Если ранее использовался старый модуль `lite-led`, удалите файл `switch/.overlays/lite-led.ovl`, чтобы оверлей не двоился в меню.
5. Перезагрузите консоль.
6. Откройте Tesla меню и выберите пункт **Smart Daemon**.

### Структура файлов на MicroSD
```text
sdmc:/
├── atmosphere/
│   └── contents/
│       └── 0100000000000895/
│           ├── flags/
│           │   └── boot2.flag
│           ├── exefs.nsp
│           └── toolbox.json
└── switch/
    └── .overlays/
        └── lite-led-plus.ovl
```

---

## 🛠️ Инженерные детали реализации

1. **Аппаратная линия**: Светодиод подключен к выводу чипа Nvidia Tegra X1 с кодом `0x35000065U` в режиме `3U`. Прямого отношения к микроконтроллеру Joy-Con он не имеет, поэтому управление выполняется напрямую через ядро Horizon OS (`gpioPadSetValue`).
2. **Устранение мерцания (Дельта-Сигма ЦАП)**: Обычные циклы задержек заставляли шедулер Horizon OS округлять сон до 1 мс, что ломало скважность ШИМ. Алгоритм Брезенхема (Delta-Sigma) рассчитывает плотность импульсов на стабильной частоте 500 Гц без джиттера и нагрузки на систему.
3. **Защита от зависаний**: При подключении/отключении зарядного кабеля контроллер питания сбрасывает состояние шины. Модуль отслеживает статус сессии и мгновенно восстанавливает владение пином (Pinmux), предотвращая отказ кнопки HOME.
4. **Режим глубокого сна (SC7 Suspend)**: При переходе Switch Lite в сон 4 вычислительных ядра ARM Cortex-A57 полностью обесточиваются на аппаратном уровне. При пробуждении консоли таймер фиксирует скачок времени и запускает двойной приветственный импульс.

---

## 📄 Лицензия

Проект распространяется под открытой лицензией **MIT License**. Подробнее см. в файле [LICENSE](LICENSE).
