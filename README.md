# lite-led (Custom Smart Edition)

A custom fork of [marcinsz89/lite-led](https://github.com/marcinsz89/lite-led) for the **Nintendo Switch Lite**, introducing an automated **Smart Profile** for notification LED control.

This project consists of:
- An Atmosphère sysmodule running at Title ID `0100000000000895`
- A Tesla / Ultrahand overlay at `switch/.overlays/lite-led.ovl`

---

## What makes this fork different?

The original utility offered only static predefined modes. This fork implements an OEM-like **Smart Mode (Auto)** with dynamic event tracking:

| Event | Behavior |
| --- | --- |
| **Charging (< 100%)** | Soft and gentle breathing/pulsing effect at dim brightness |
| **Fully Charged (100%)** | Solid dim light while plugged in |
| **HOME Button Press** | Lights up for 1.0 second on press during gameplay or in OS |
| **Wake from Sleep** | Greets with a distinct double 1-second pulse |
| **Gameplay / Idle** | LED remains completely off to avoid distraction |

Manual modes from the original project (`Solid`, `Dim`, `Fade`, `Off`) are fully preserved and accessible via the overlay.

---

## Installation

1. Download the latest `lite-led-custom.zip` from the [Releases](../../releases) tab.
2. Extract the archive onto your computer.
3. Copy the `atmosphere` and `switch` folders to the root of your Switch microSD card.
4. If you had an older version installed, delete `sdmc:/config/lite-led/type` to apply Smart Mode immediately.
5. Reboot the console.

---

## Controls

Open the overlay inside Tesla or Ultrahand and select **Smart Mode (Auto)**.

---

## Automated Builds

The repository includes a GitHub Actions workflow (`.github/workflows/build.yml`) that compiles both the sysmodule and overlay using the official `devkitpro/devkita64` container on every push.

---

## Credits

- [marcinsz89](https://github.com/marcinsz89/lite-led) — original `lite-led` implementation and overlay.
- [souldbminerr](https://github.com/souldbminerr) — Switch Lite GPIO notification discovery.
