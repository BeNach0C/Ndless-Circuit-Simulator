# Naive Circuit Simulator for TI-Nspire CX CAS (Ndless)

Port of **Naive Circuit Simulator** to the **TI-Nspire CX CAS** calculator running **Ndless**, featuring a full Modified Nodal Analysis (MNA) numerical engine, Backward Euler companion models for transient reactive circuits, 3-Phase AC power support, phasor impedance analysis, an interactive mini-oscilloscope, an on-screen cursor with touchpad & directional keypad control, and an nSDL interface.

---

## Features

- **Modified Nodal Analysis (MNA) Engine**:
  - Solves arbitrary circuit topologies ($Ax = z$), including multi-loop, bridged, and 3-phase polyphase networks.
  - Optimized Gaussian Elimination with partial pivoting and 1D cache-contiguous matrix storage.
  - $G_{min}$ regularization to prevent floating-node matrix singularities.
- **Transient Analysis Engine**:
  - Discrete time-stepping loop ($\Delta t = 0.5\text{ms}$ for smooth 60 Hz / 50 Hz AC sinusoidal tracking).
  - Backward Euler companion models for **Capacitors** ($G_{eq} = C / \Delta t$, $I_{eq} = G_{eq} \cdot V_{prev}$) and **Inductors** ($G_{eq} = \Delta t / L$, $I_{eq} = I_{prev}$).
- **Three-Phase (Triphasic) Power Systems**:
  - **Wye ($Y$) 3-Phase Macro (`Y` key)**: Instantly generates a complete balanced 3-phase 4-wire Wye generator (Phase A at $0^\circ$, Phase B at $-120^\circ$, Phase C at $+120^\circ$) connected to a neutral bus and balanced load!
  - **Delta ($\Delta$) 3-Phase Macro (`D` key)**: Instantly generates a 3-phase closed-loop Delta generator with $120^\circ$ phase shifts.
  - **Sinusoidal AC Voltage Sources (`A` key)**: Independent AC sources with configurable amplitude ($V_{\text{RMS}}$), frequency ($f$), and phase offset ($\phi \in \{0^\circ, -120^\circ, +120^\circ\}$).
- **Phasor Domain & Frequency Conversion**:
  - **Phasor Toggle (`F` key or `PH` button)**: Instantly toggles between Time-Domain view and Phasor view.
  - **Frequency Definition ($\omega$) (`O` key)**: Configure angular frequency $\omega$ in rad/s (default $377\text{ rad/s}$ for 60 Hz). If $\omega$ is set to 0 (undefined), components convert symbolically (e.g. $1/8\text{F} \to -j8/\omega$).
  - **Generic Phasor Impedance $Z$ (`Z` key)**:
    - In **Phasor Mode**: Drawn as an international standard **rectangle** ($\boxed{\ Z\ }$) showing $R \pm jX\ \Omega$.
    - In **Time Mode**: Drawn as an **inductor** (if $X > 0$, $L = X/\omega$), **capacitor** (if $X < 0$, $C = -1/(\omega X)$), or **resistor** (if $X = 0$).
- **Mini-Oscilloscope (`S` key)**:
  - Toggle a non-cluttering mini-scope window on/off anytime with `S`.
  - Live plots Phase A (Yellow), Phase B (Green), and Phase C (Cyan) sinusoidal waveforms moving in real time with their distinct $120^\circ$ phase angles!
- **RMS Measurement & Anti-Flicker Tooltip**:
  - Automatically computes running true RMS voltage ($V_{\text{RMS}}$) and current ($I_{\text{RMS}}$) for AC components to prevent fluctuating digits on screen.
- **Controls & Cursor**:
  - **Touchpad Cursor**: Move your finger across the calculator's touchpad for smooth mouse tracking.
  - **Keypad Arrows**: Move the on-screen cursor using `UP`, `DOWN`, `LEFT`, `RIGHT` (hold `Shift` for fast cursor).
  - **Click**: Center click button on touchpad (`CLICK`), `ENTER`, or tap on pad.
  - **Component Value Editing**: Press `E` or click with Edit tool to modify component values using standard numbers or `+`/`-` keys.

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
| `V` | Select **DC Voltage Source** tool |
| `A` | Select **AC Voltage Source** tool |
| `I` | Select **Current Source** tool |
| `Z` | Select **Generic Phasor Impedance ($Z$)** tool |
| `G` | Select **Ground** tool |
| `Y` | Macro: Add **Wye ($Y$) 3-Phase Generator**|
| `D` | Macro: Add **Delta ($\Delta$) 3-Phase Generator** |
| `S` | Toggle **Mini-Oscilloscope** (3-phase live waveforms) |
| `F` or `PH` | Toggle **Phasor Mode** $\longleftrightarrow$ **Time-Domain Mode** |
| `O` | Set **Global Frequency $\omega$** (in rad/s, 0 for symbolic) |
| `DEL` (Keypad / Toolbar) | Delete hovered component |
| `E` | **Edit** component value / properties |
| `P` or `Space` | **Pause / Resume** simulation |
| `1` / `2` / `3` | Load **RC**, **RLC**, or **Bridge** preset |
| `0` | Clear canvas |
| `ESC` | Cancel current action / Close dialog / Exit |

---

## How to Build

From within the Ndless SDK environment:

```bash
export PATH="/path/to/Ndless/ndless-sdk/bin:/path/to/Ndless/ndless-sdk/toolchain/install/bin:$PATH"
cd NaiveCircuitSimulator
make clean
make
```

Outputs `NaiveCircuitSimulator.tns`.

---

## How to Install on Calculator

1. Connect your TI-Nspire CX CAS calculator via USB to your computer using **TI-Nspire Computer Link Software** or **TiLink**.
2. Transfer `NaiveCircuitSimulator.tns` into your calculator's `ndless` folder or documents folder.
3. On the calculator, navigate to `NaiveCircuitSimulator` in My Documents and press `ENTER` to launch.
