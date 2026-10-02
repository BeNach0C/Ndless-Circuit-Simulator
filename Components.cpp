#include "Components.h"
#include <cmath>
#include <cstdio>
#include <cstring>

void formatSIValue(double val, const char* unit, char* buf, size_t sz) {
    if (std::abs(val) < 1e-15) {
        snprintf(buf, sz, "0 %s", unit);
        return;
    }

    double absVal = std::abs(val);
    const char* prefix = "";
    double scaled = val;

    if (absVal >= 1e9) {
        prefix = "G";
        scaled = val / 1e9;
    } else if (absVal >= 1e6) {
        prefix = "M";
        scaled = val / 1e6;
    } else if (absVal >= 1e3) {
        prefix = "k";
        scaled = val / 1e3;
    } else if (absVal >= 1.0) {
        prefix = "";
        scaled = val;
    } else if (absVal >= 1e-3) {
        prefix = "m";
        scaled = val * 1e3;
    } else if (absVal >= 1e-6) {
        prefix = "u";
        scaled = val * 1e6;
    } else if (absVal >= 1e-9) {
        prefix = "n";
        scaled = val * 1e9;
    } else if (absVal >= 1e-12) {
        prefix = "p";
        scaled = val * 1e12;
    } else {
        prefix = "f";
        scaled = val * 1e15;
    }

    // Print with up to 2 decimal places, trimming trailing zeros if clean
    if (std::abs(scaled - std::round(scaled)) < 0.005) {
        snprintf(buf, sz, "%.0f %s%s", scaled, prefix, unit);
    } else if (std::abs(scaled * 10 - std::round(scaled * 10)) < 0.05) {
        snprintf(buf, sz, "%.1f %s%s", scaled, prefix, unit);
    } else {
        snprintf(buf, sz, "%.2f %s%s", scaled, prefix, unit);
    }
}

// ---------------- Base Component ----------------

Component::Component(Type t, int d1x, int d1y, int d2x, int d2y, double val)
    : type(t), dot1_x(d1x), dot1_y(d1y), dot2_x(d2x), dot2_y(d2y),
      node1(0), node2(0), aux_index(-1), value(val),
      voltage(0.0), current(0.0), state_v(0.0), state_i(0.0) {}

void Component::resetState() {
    voltage = 0.0;
    current = 0.0;
    state_v = 0.0;
    state_i = 0.0;
}

void Component::formatValueString(char* buf, size_t sz) const {
    formatSIValue(value, getUnit(), buf, sz);
}

void Component::formatInfoString(char* buf, size_t sz) const {
    char vBuf[24], iBuf[24], valBuf[24];
    formatSIValue(voltage, "V", vBuf, sizeof(vBuf));
    formatSIValue(current, "A", iBuf, sizeof(iBuf));
    formatValueString(valBuf, sizeof(valBuf));

    switch (type) {
    case COMP_RESISTOR: {
        double power = std::abs(voltage * current);
        char pBuf[24];
        formatSIValue(power, "W", pBuf, sizeof(pBuf));
        snprintf(buf, sz, "Resistor (%s): V=%s, I=%s, P=%s", valBuf, vBuf, iBuf, pBuf);
        break;
    }
    case COMP_CAPACITOR:
        snprintf(buf, sz, "Capacitor (%s): V=%s, I=%s", valBuf, vBuf, iBuf);
        break;
    case COMP_INDUCTOR:
        snprintf(buf, sz, "Inductor (%s): V=%s, I=%s", valBuf, vBuf, iBuf);
        break;
    case COMP_VOLTAGE_SRC: {
        double power = std::abs(voltage * current);
        char pBuf[24];
        formatSIValue(power, "W", pBuf, sizeof(pBuf));
        snprintf(buf, sz, "Vsrc (%s): I=%s, P=%s", valBuf, iBuf, pBuf);
        break;
    }
    case COMP_CURRENT_SRC:
        snprintf(buf, sz, "Isrc (%s): V=%s", valBuf, vBuf);
        break;
    case COMP_WIRE:
        snprintf(buf, sz, "Wire: I=%s", iBuf);
        break;
    case COMP_GROUND:
        snprintf(buf, sz, "Ground: 0 V (Ref Node 0)");
        break;
    }
}

// ---------------- Resistor ----------------

Resistor::Resistor(int d1x, int d1y, int d2x, int d2y, double r)
    : Component(COMP_RESISTOR, d1x, d1y, d2x, d2y, r > 1e-6 ? r : 1000.0) {}

void Resistor::stamp(MNASystem& mna, double /*dt*/) {
    double g = 1.0 / value;
    mna.stampConductance(node1, node2, g);
}

void Resistor::updateState(const MNASystem& mna, double /*dt*/) {
    voltage = mna.getNodeVoltage(node1) - mna.getNodeVoltage(node2);
    current = voltage / value;
}

// ---------------- Voltage Source ----------------

VoltageSource::VoltageSource(int d1x, int d1y, int d2x, int d2y, double v)
    : Component(COMP_VOLTAGE_SRC, d1x, d1y, d2x, d2y, v) {}

void VoltageSource::stamp(MNASystem& mna, double /*dt*/) {
    int idx = mna.numNodes + aux_index;
    // V(node1) - V(node2) = value
    if (node1 > 0) {
        mna.stampMatrix(idx, node1 - 1, +1.0);
        mna.stampMatrix(node1 - 1, idx, +1.0);
    }
    if (node2 > 0) {
        mna.stampMatrix(idx, node2 - 1, -1.0);
        mna.stampMatrix(node2 - 1, idx, -1.0);
    }
    mna.stampRHS(idx, value);
}

void VoltageSource::updateState(const MNASystem& mna, double /*dt*/) {
    voltage = value;
    // Current exiting node1
    current = mna.getAuxCurrent(aux_index);
}

// ---------------- Current Source ----------------

CurrentSource::CurrentSource(int d1x, int d1y, int d2x, int d2y, double i)
    : Component(COMP_CURRENT_SRC, d1x, d1y, d2x, d2y, i) {}

void CurrentSource::stamp(MNASystem& mna, double /*dt*/) {
    // Current leaves node1 (-value) and enters node2 (+value)
    if (node1 > 0) {
        mna.stampRHS(node1 - 1, -value);
    }
    if (node2 > 0) {
        mna.stampRHS(node2 - 1, +value);
    }
}

void CurrentSource::updateState(const MNASystem& mna, double /*dt*/) {
    voltage = mna.getNodeVoltage(node1) - mna.getNodeVoltage(node2);
    current = value;
}

// ---------------- Capacitor (Backward Euler) ----------------

Capacitor::Capacitor(int d1x, int d1y, int d2x, int d2y, double c)
    : Component(COMP_CAPACITOR, d1x, d1y, d2x, d2y, c > 1e-15 ? c : 10e-6) {}

void Capacitor::stamp(MNASystem& mna, double dt) {
    // Companion model:
    // Geq = C / dt
    // Ieq = Geq * v_prev
    double Geq = value / dt;
    double Ieq = Geq * state_v;

    mna.stampConductance(node1, node2, Geq);
    // Parallel current source flows from node1 to node2
    if (node1 > 0) {
        mna.stampRHS(node1 - 1, +Ieq);
    }
    if (node2 > 0) {
        mna.stampRHS(node2 - 1, -Ieq);
    }
}

void Capacitor::updateState(const MNASystem& mna, double dt) {
    double v_new = mna.getNodeVoltage(node1) - mna.getNodeVoltage(node2);
    double Geq = value / dt;
    voltage = v_new;
    current = Geq * (v_new - state_v);
    state_v = v_new;
}

// ---------------- Inductor (Backward Euler) ----------------

Inductor::Inductor(int d1x, int d1y, int d2x, int d2y, double l)
    : Component(COMP_INDUCTOR, d1x, d1y, d2x, d2y, l > 1e-12 ? l : 10e-3) {}

void Inductor::stamp(MNASystem& mna, double dt) {
    // Companion model:
    // Geq = dt / L
    // Ieq = i_prev
    double Geq = dt / value;
    double Ieq = state_i;

    mna.stampConductance(node1, node2, Geq);
    // Parallel current source flows from node1 to node2: KCL stamp:
    if (node1 > 0) {
        mna.stampRHS(node1 - 1, -Ieq);
    }
    if (node2 > 0) {
        mna.stampRHS(node2 - 1, +Ieq);
    }
}

void Inductor::updateState(const MNASystem& mna, double dt) {
    double v_new = mna.getNodeVoltage(node1) - mna.getNodeVoltage(node2);
    double Geq = dt / value;
    voltage = v_new;
    current = Geq * v_new + state_i;
    state_i = current;
}

// ---------------- Wire ----------------

Wire::Wire(int d1x, int d1y, int d2x, int d2y)
    : Component(COMP_WIRE, d1x, d1y, d2x, d2y, 0.0) {}

void Wire::stamp(MNASystem& /*mna*/, double /*dt*/) {
    // Handled in netlist resolution via Union-Find
}

void Wire::updateState(const MNASystem& mna, double /*dt*/) {
    voltage = mna.getNodeVoltage(node1) - mna.getNodeVoltage(node2);
    current = 0.0;
}

// ---------------- Ground ----------------

Ground::Ground(int d1x, int d1y)
    : Component(COMP_GROUND, d1x, d1y, d1x, d1y, 0.0) {}

void Ground::stamp(MNASystem& /*mna*/, double /*dt*/) {
    // Defines node 0
}

void Ground::updateState(const MNASystem& /*mna*/, double /*dt*/) {
    voltage = 0.0;
    current = 0.0;
}
