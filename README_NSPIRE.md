# Naive Circuit Simulator for TI-Nspire CX CAS (Ndless)

Port of **Naive Circuit Simulator** to the **TI-Nspire CX CAS** calculator running **Ndless**, featuring a full Modified Nodal Analysis (MNA) numerical engine, Backward Euler companion models for transient reactive circuits (capacitors & inductors), an on-screen cursor with touchpad & directional keypad control, and an nSDL interface.

---

## Features

- **Modified Nodal Analysis (MNA) Engine**:
  - Solves arbitrary circuit topologies ($Ax = z$), including bridges and multi-loop networks.
  - Optimized Gaussian Elimination with partial pivoting and 1D cache-contiguous matrix storage.
  - $G_{min}$ regularization to prevent floating-node matrix singularities.
- **Transient Analysis Engine**:
  - Discrete time-stepping loop ($\Delta t = 1.0\text{ms}$, 2 steps/frame).
  - Backward Euler companion models for **Capacitors** ($G_{eq} = C / \Delta t$, $I_{eq} = G_{eq} \cdot V_{prev}$) and **Inductors** ($G_{eq} = \Delta t / L$, $I_{eq} = I_{prev}$).
- **Components Supported**:
  - Resistors ($R$)
  - Capacitors ($C$)
  - Inductors ($L$)
  - Independent Voltage Sources ($V$)
  - Independent Current Sources ($I$)
  - Ground Reference (Node 0)
  - Wires with automatic Union-Find electrical net resolution
- **Controls & Cursor**:
  - **Touchpad Cursor**: Move your finger across the calculator's touchpad for smooth mouse tracking.
  - **Keypad Arrows**: Move the on-screen cursor using `UP`, `DOWN`, `LEFT`, `RIGHT` (hold `Shift` for fast cursor).
  - **Click**: Center click button on touchpad (`CLICK`), `ENTER`, or tap on pad.
  - **Component Value Editing**: Press `E` or click with Edit tool to modify component values using standard numbers or `+`/`-` keys.
- **Built-in Presets**:
  - `1`: RC Step Response (Low-pass filter charging curve)
  - `2`: RLC Resonant Tank (Damped sinusoidal transient oscillation)
  - `3`: Wheatstone Bridge
  - `0`: Clear canvas

---

## Calculator Controls

| Key / Action | Function |
|---|---|
| **Touchpad Swipe** | Move cursor smoothly across screen |
| **Arrow Keys** (`▲ ▼ ◄ ►`) | Move cursor (hold `Shift` for faster speed) |
| **Center Click** / `ENTER` | Place component / Select tool / Confirm value |
| `W` | Select **Wire** tool |
| `R` | Select **Resistor** tool |
| `C` | Select **Capacitor** tool |
| `L` | Select **Inductor** tool |
| `V` | Select **Voltage Source** tool |
| `I` | Select **Current Source** tool |
| `G` | Select **Ground** tool |
| `D` or `DEL` | Select **Delete** (Blank) tool |
| `E` | **Edit** hovered component value |
| `P` or `Space` | **Pause / Resume** simulation |
| `1` / `2` / `3` | Load **RC**, **RLC**, or **Bridge** preset |
| `0` | Clear canvas |
| `ESC` | Exit simulator |

---