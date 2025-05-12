# Chip-8 Emulator

A modular, cycle-accurate Chip-8 interpreter built in C++17, leveraging concepts from my System Hardware course such as memory mapping, register operations, and precise timers.

## Features

* **Complete opcode support:** All 35 original Chip-8 instructions via a fetch–decode–execute loop and opcode dispatch table.
* **Modular design:** Separated components for CPU, Memory, Display, Input, and Timers for maintainability and future extensions.
* **SDL3 graphics & input:** 64×32 monochrome display rendered at 60 FPS and hex keypad mapped to standard keyboard keys.
* **Precise timing:** `<chrono>`-based CPU cycle throttling and delay/sound timers for authentic gameplay.
* **Robust ROM loader:** Error-checked file loading with diagnostics for invalid or unsupported ROMs.

## Built With

* C++17
* SDL3
* Git & GitHub

## Installation

```bash
# Clone the repo
git clone https://github.com/<username>/chip8-emulator.git
cd chip8-emulator
```

## Usage

```bash
./chip8-emulator <Scale> <Delay> <ROM>
```

* **Scale**: Window scale factor (e.g., 10)
* **Delay**: CPU cycle delay in milliseconds (e.g., 2)
* **ROM**: Path to the Chip-8 ROM file

## Controls

| Chip‑8 Key | Keyboard Key |
| ---------- | ------------ |
| 1 2 3 C    | 1 2 3 4      |
| 4 5 6 D    | Q W E R      |
| 7 8 9 E    | A S D F      |
| A 0 B F    | Z X C V      |

## Architecture

* **CPU:** Fetch–decode–execute loop with function-pointer dispatch handlers
* **Memory:** 4 KB RAM including built-in font set
* **Display:** SDL3-based framebuffer and draw routines
* **Input:** Keyboard event mapping to Chip‑8 keypad
* **Timers:** Delay & sound timers decrementation
