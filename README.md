# ESP32 Elevator FSM

An ESP32-based elevator simulation implemented with a finite state machine. The project models elevator behavior using states, events, queued travel requests, LED indicators, and button input.

## Features

- Finite state machine with clear elevator states
- Handles idle, moving, loading, and unloading behavior
- Queue for pending travel requests
- LED indicators for current elevator level
- Button input with debounce handling
- Timer-based state transitions
- Designed for ESP32 using ESP-IDF / PlatformIO

## State Machine

The system is organized around four main states:

- `idle`
- `moving`
- `loading`
- `unloading`

Events such as button presses, arrival, timer ticks, and completed loading/unloading trigger transitions between states.

## Tech Stack

- C
- ESP32
- ESP-IDF
- PlatformIO
- GPIO
- Timers

## Key Concepts

- Finite state machines
- Event-driven programming
- Queue handling
- GPIO input/output
- Timer-based logic
- Embedded C structure

## Project Structure

```text
src/
  main.c

platformio.ini
CMakeLists.txt
```
## How It Works
The elevator keeps track of its current position and current state. Travel requests are stored in a queue and processed one at a time. LEDs indicate the current level, and the state machine controls movement, loading, and unloading behavior based on events and elapsed time.

Status
Educational embedded systems project focused on state machines, event handling, and ESP32 GPIO control.
