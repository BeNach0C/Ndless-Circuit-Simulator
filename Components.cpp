#include "Components.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void formatSIValue(double val, const char* unit, char* buf, size_t sz) {
    if (std::abs(val) < 1e-15) {
        if (unit && unit[0]) snprintf(buf, sz, "0 %s", unit);
        else snprintf(buf, sz, "0");
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

    const char* space = (unit && unit[0]) ? " " : "";
    if (std::abs(scaled - std::round(scaled)) < 0.005) {
        snprintf(buf, sz, "%.0f%s%s%s", scaled, space, prefix, unit ? unit : "");
    } else if (std::abs(scaled * 10 - std::round(scaled * 10)) < 0.05) {
        snprintf(buf, sz, "%.1f%s%s%s", scaled, space, prefix, unit ? unit : "");
    } else {
        snprintf(buf, sz, "%.2f%s%s%s", scaled, space, prefix, unit ? unit : "");
    }
}

// ---------------- Base Component ----------------

Component::Component(Type t, int d1x, int d1y, int d2x, int d2y, double val)
    : type(t), dot1_x(d1x), dot1_y(d1y), dot2_x(d2x), dot2_y(d2y),
      node1(0), node2(0), aux_index(-1), value(val),
      voltage(0.0), current(0.0), state_v(0.0), state_i(0.0),
      peak_v(0.0), peak_i(0.0) {}

void Component::resetState() {
    voltage = 0.0;
    current = 0.0;
    state_v = 0.0;
    state_i = 0.0;
    peak_v = 0.0;
    peak_i = 0.0;
}

void Component::formatValueString(char* buf, size_t sz, bool isPhasor, double omega) const {
    if (isPhasor) {
        formatPhasorString(buf, sz, omega);
    } else {
        formatSIValue(value, getUnit(), buf, sz);
    }
}

void Component::formatPhasorString(char* buf, size_t sz, double /*omega*/) const {
    formatSIValue(value, getUnit(), buf, sz);
}

void Component::formatInfoString(char* buf, size_t sz, bool isPhasor, double omega) const {
    char vBuf[24], iBuf[24], valBuf[32];
    formatSIValue(voltage, "V", vBuf, sizeof(vBuf));
    formatSIValue(current, "A", iBuf, sizeof(iBuf));
    formatValueString(valBuf, sizeof(valBuf), isPhasor, omega);

    double vRms = getRMSVoltage();
    double iRms = getRMSCurrent();
    char vRmsBuf[24], iRmsBuf[24];
    formatSIValue(vRms, "V", vRmsBuf, sizeof(vRmsBuf));
    formatSIValue(iRms, "A", iRmsBuf, sizeof(iRmsBuf));

    bool isAC = (type == COMP_AC_VOLTAGE_SRC) || (vRms > 0.05 && std::abs(voltage - peak_v) > 0.05 * peak_v);

    switch (type) {
    case COMP_RESISTOR: {
        double power = std::abs(voltage * current);
        char pBuf[24];
        formatSIValue(power, "W", pBuf, sizeof(pBuf));
        if (isAC) {
            snprintf(buf, sz, "Resistor (%s): Vrms=%s, Irms=%s", valBuf, vRmsBuf, iRmsBuf);
        } else {
            snprintf(buf, sz, "Resistor (%s): V=%s, I=%s, P=%s", valBuf, vBuf, iBuf, pBuf);
        }
        break;
    }
    case COMP_CAPACITOR:
        if (isAC) snprintf(buf, sz, "Capacitor (%s): Vrms=%s, Irms=%s", valBuf, vRmsBuf, iRmsBuf);
        else snprintf(buf, sz, "Capacitor (%s): V=%s, I=%s", valBuf, vBuf, iBuf);
        break;
    case COMP_INDUCTOR:
        if (isAC) snprintf(buf, sz, "Inductor (%s): Vrms=%s, Irms=%s", valBuf, vRmsBuf, iRmsBuf);
        else snprintf(buf, sz, "Inductor (%s): V=%s, I=%s", valBuf, vBuf, iBuf);
        break;
    case COMP_VOLTAGE_SRC: {
        double power = std::abs(voltage * current);
        char pBuf[24];
        formatSIValue(power, "W", pBuf, sizeof(pBuf));
        snprintf(buf, sz, "DC Vsrc (%s): I=%s, P=%s", valBuf, iBuf, pBuf);
        break;
    }
    case COMP_AC_VOLTAGE_SRC:
        snprintf(buf, sz, "AC Vsrc (%s): Vrms=%s, Irms=%s", valBuf, vRmsBuf, iRmsBuf);
        break;
    case COMP_IMPEDANCE:
        snprintf(buf, sz, "Z (%s): Vrms=%s, Irms=%s", valBuf, vRmsBuf, iRmsBuf);
        break;
    case COMP_CURRENT_SRC:
        snprintf(buf, sz, "Isrc (%s): V=%s", valBuf, vBuf);
        break;
    case COMP_WIRE:
        if (isAC) snprintf(buf, sz, "Wire: Irms=%s", iRmsBuf);
        else snprintf(buf, sz, "Wire: I=%s", iBuf);
        break;
    case COMP_GROUND:
        snprintf(buf, sz, "Ground: 0 V (Ref Node 0)");
        break;
    }
}

// ---------------- Resistor ----------------

Resistor::Resistor(int d1x, int d1y, int d2x, int d2y, double r)
    : Component(COMP_RESISTOR, d1x, d1y, d2x, d2y, r > 1e-6 ? r : 1000.0) {}

void Resistor::stamp(MNASystem& mna, double /*dt*/, double /*t*/) {
    double g = 1.0 / value;
    mna.stampConductance(node1, node2, g);
}

void Resistor::updateState(const MNASystem& mna, double /*dt*/) {
    voltage = mna.getNodeVoltage(node1) - mna.getNodeVoltage(node2);
    current = voltage / value;
    if (std::abs(voltage) > peak_v) peak_v = std::abs(voltage);
    else peak_v *= 0.995;
    if (std::abs(current) > peak_i) peak_i = std::abs(current);
    else peak_i *= 0.995;
}

void Resistor::formatPhasorString(char* buf, size_t sz, double /*omega*/) const {
    formatSIValue(value, "Ohm", buf, sz);
}

// ---------------- DC Voltage Source ----------------

VoltageSource::VoltageSource(int d1x, int d1y, int d2x, int d2y, double v)
    : Component(COMP_VOLTAGE_SRC, d1x, d1y, d2x, d2y, v) {}

void VoltageSource::stamp(MNASystem& mna, double /*dt*/, double /*t*/) {
    int idx = mna.numNodes + aux_index;
    if (node1 > 0) {
        mna.stampMatrix(idx, node1 - 1, +1.0);
        mna.stampMatrix(node1 - 1, idx, +1.0);
    }
    if (node2 > 0) {
        mna.stampMatrix(idx, node2 - 1, -1.0);
        mna.stampMatrix(node2 - 1, idx, -1.0);
    }
    // Internal resistance to break voltage source loops (e.g. Delta loops)
    mna.stampMatrix(idx, idx, -1e-4);
    mna.stampRHS(idx, value);
}

void VoltageSource::updateState(const MNASystem& mna, double /*dt*/) {
    voltage = value;
    current = mna.getAuxCurrent(aux_index);
    peak_v = std::abs(voltage);
    if (std::abs(current) > peak_i) peak_i = std::abs(current);
    else peak_i *= 0.995;
}

// ---------------- AC Voltage Source ----------------

ACVoltageSource::ACVoltageSource(int d1x, int d1y, int d2x, int d2y, double v, double f, double ph)
    : Component(COMP_AC_VOLTAGE_SRC, d1x, d1y, d2x, d2y, v), freq(f), phase(ph) {
    peak_v = std::abs(v) * 1.41421356;
}

void ACVoltageSource::stamp(MNASystem& mna, double /*dt*/, double t) {
    int idx = mna.numNodes + aux_index;
    if (node1 > 0) {
        mna.stampMatrix(idx, node1 - 1, +1.0);
        mna.stampMatrix(node1 - 1, idx, +1.0);
    }
    if (node2 > 0) {
        mna.stampMatrix(idx, node2 - 1, -1.0);
        mna.stampMatrix(node2 - 1, idx, -1.0);
    }
    // Internal resistance to break voltage source loops (e.g. Delta loops)
    mna.stampMatrix(idx, idx, -1e-4);

    double omega = 2.0 * M_PI * freq;
    double phaseRad = phase * (M_PI / 180.0);
    // Instantaneous peak voltage = RMS * sqrt(2)
    double v_peak = value * 1.41421356;
    double v_instant = v_peak * std::sin(omega * t + phaseRad);
    mna.stampRHS(idx, v_instant);
}

void ACVoltageSource::updateState(const MNASystem& mna, double /*dt*/) {
    voltage = mna.getNodeVoltage(node1) - mna.getNodeVoltage(node2);
    current = mna.getAuxCurrent(aux_index);
    if (std::abs(voltage) > peak_v) peak_v = std::abs(voltage);
    else peak_v = peak_v * 0.99 + std::abs(voltage) * 0.01;
    if (std::abs(current) > peak_i) peak_i = std::abs(current);
    else peak_i = peak_i * 0.99 + std::abs(current) * 0.01;
}

void ACVoltageSource::formatPhasorString(char* buf, size_t sz, double /*omega*/) const {
    char vBuf[24];
    formatSIValue(value, "V", vBuf, sizeof(vBuf));
    if (std::abs(phase) < 0.1) {
        snprintf(buf, sz, "%s < 0 deg", vBuf);
    } else {
        snprintf(buf, sz, "%s < %.0f deg", vBuf, phase);
    }
}

// ---------------- Current Source ----------------

CurrentSource::CurrentSource(int d1x, int d1y, int d2x, int d2y, double i)
    : Component(COMP_CURRENT_SRC, d1x, d1y, d2x, d2y, i) {}

void CurrentSource::stamp(MNASystem& mna, double /*dt*/, double /*t*/) {
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
    if (std::abs(voltage) > peak_v) peak_v = std::abs(voltage);
    else peak_v *= 0.995;
    peak_i = std::abs(current);
}

// ---------------- Capacitor (Backward Euler) ----------------

Capacitor::Capacitor(int d1x, int d1y, int d2x, int d2y, double c)
    : Component(COMP_CAPACITOR, d1x, d1y, d2x, d2y, c > 1e-15 ? c : 10e-6) {}

void Capacitor::stamp(MNASystem& mna, double dt, double /*t*/) {
    double Geq = value / dt;
    double Ieq = Geq * state_v;

    mna.stampConductance(node1, node2, Geq);
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

    if (std::abs(voltage) > peak_v) peak_v = std::abs(voltage);
    else peak_v *= 0.995;
    if (std::abs(current) > peak_i) peak_i = std::abs(current);
    else peak_i *= 0.995;
}

void Capacitor::formatPhasorString(char* buf, size_t sz, double omega) const {
    if (omega > 0.0) {
        double Xc = 1.0 / (omega * value);
        char xBuf[24];
        formatSIValue(Xc, "Ohm", xBuf, sizeof(xBuf));
        snprintf(buf, sz, "-j%s", xBuf);
    } else {
        double invC = 1.0 / value;
        char cBuf[24];
        formatSIValue(invC, "", cBuf, sizeof(cBuf));
        snprintf(buf, sz, "-j%s/w", cBuf);
    }
}

// ---------------- Inductor (Backward Euler) ----------------

Inductor::Inductor(int d1x, int d1y, int d2x, int d2y, double l)
    : Component(COMP_INDUCTOR, d1x, d1y, d2x, d2y, l > 1e-12 ? l : 10e-3) {}

void Inductor::stamp(MNASystem& mna, double dt, double /*t*/) {
    double Geq = dt / value;
    double Ieq = state_i;

    mna.stampConductance(node1, node2, Geq);
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

    if (std::abs(voltage) > peak_v) peak_v = std::abs(voltage);
    else peak_v *= 0.995;
    if (std::abs(current) > peak_i) peak_i = std::abs(current);
    else peak_i *= 0.995;
}

void Inductor::formatPhasorString(char* buf, size_t sz, double omega) const {
    if (omega > 0.0) {
        double Xl = omega * value;
        char xBuf[24];
        formatSIValue(Xl, "Ohm", xBuf, sizeof(xBuf));
        snprintf(buf, sz, "+j%s", xBuf);
    } else {
        char lBuf[24];
        formatSIValue(value, "H", lBuf, sizeof(lBuf));
        snprintf(buf, sz, "j*w*%s", lBuf);
    }
}

// ---------------- Generic Impedance Z (Phasor) ----------------

GenericImpedance::GenericImpedance(int d1x, int d1y, int d2x, int d2y, double r, double x)
    : Component(COMP_IMPEDANCE, d1x, d1y, d2x, d2y, std::hypot(r, x)),
      real_r(r), imag_x(x) {}

void GenericImpedance::stamp(MNASystem& mna, double dt, double /*t*/) {
    // Companion model in time domain using default 60Hz (omega=377) if needed
    double g_tot = 0.0;
    if (std::abs(real_r) > 1e-4) {
        g_tot += 1.0 / real_r;
    }

    if (imag_x > 1e-4) {
        // Inductive: L = X / omega (use 377 rad/s)
        double L = imag_x / 377.0;
        double Geq = dt / (L > 1e-6 ? L : 1e-6);
        double Ieq = state_i;
        g_tot += Geq;
        if (node1 > 0) mna.stampRHS(node1 - 1, -Ieq);
        if (node2 > 0) mna.stampRHS(node2 - 1, +Ieq);
    } else if (imag_x < -1e-4) {
        // Capacitive: C = -1 / (omega * X)
        double C = -1.0 / (377.0 * imag_x);
        double Geq = (C > 1e-12 ? C : 1e-12) / dt;
        double Ieq = Geq * state_v;
        g_tot += Geq;
        if (node1 > 0) mna.stampRHS(node1 - 1, +Ieq);
        if (node2 > 0) mna.stampRHS(node2 - 1, -Ieq);
    }

    if (g_tot < 1e-6) g_tot = 1e-6;
    mna.stampConductance(node1, node2, g_tot);
}

void GenericImpedance::updateState(const MNASystem& mna, double dt) {
    double v_new = mna.getNodeVoltage(node1) - mna.getNodeVoltage(node2);
    voltage = v_new;

    if (imag_x > 1e-4) {
        double L = imag_x / 377.0;
        double Geq = dt / (L > 1e-6 ? L : 1e-6);
        current = Geq * v_new + state_i;
        state_i = current;
    } else if (imag_x < -1e-4) {
        double C = -1.0 / (377.0 * imag_x);
        double Geq = (C > 1e-12 ? C : 1e-12) / dt;
        current = Geq * (v_new - state_v);
        state_v = v_new;
    } else {
        current = real_r > 1e-6 ? v_new / real_r : 0.0;
    }

    if (std::abs(voltage) > peak_v) peak_v = std::abs(voltage);
    else peak_v *= 0.995;
    if (std::abs(current) > peak_i) peak_i = std::abs(current);
    else peak_i *= 0.995;
}

void GenericImpedance::formatPhasorString(char* buf, size_t sz, double /*omega*/) const {
    char rBuf[24], xBuf[24];
    formatSIValue(real_r, "", rBuf, sizeof(rBuf));
    formatSIValue(std::abs(imag_x), "", xBuf, sizeof(xBuf));

    if (std::abs(imag_x) < 1e-4) {
        snprintf(buf, sz, "%s Ohm", rBuf);
    } else if (std::abs(real_r) < 1e-4) {
        snprintf(buf, sz, "%sj%s Ohm", imag_x < 0 ? "-" : "", xBuf);
    } else {
        snprintf(buf, sz, "%s%sj%s", rBuf, imag_x >= 0 ? "+" : "-", xBuf);
    }
}

void GenericImpedance::formatValueString(char* buf, size_t sz, bool isPhasor, double omega) const {
    if (isPhasor) {
        formatPhasorString(buf, sz, omega);
    } else {
        // Time domain equivalent representation
        if (std::abs(imag_x) < 1e-4) {
            formatSIValue(real_r, "Ohm", buf, sz);
        } else if (imag_x > 1e-4) {
            double effectiveOmega = (omega > 0.0) ? omega : 377.0;
            double L = imag_x / effectiveOmega;
            char lBuf[24];
            formatSIValue(L, "H", lBuf, sizeof(lBuf));
            if (real_r > 1e-4) {
                char rBuf[24];
                formatSIValue(real_r, "Ohm", rBuf, sizeof(rBuf));
                snprintf(buf, sz, "%s+%s", rBuf, lBuf);
            } else {
                snprintf(buf, sz, "%s", lBuf);
            }
        } else {
            double effectiveOmega = (omega > 0.0) ? omega : 377.0;
            double C = -1.0 / (effectiveOmega * imag_x);
            char cBuf[24];
            formatSIValue(C, "F", cBuf, sizeof(cBuf));
            if (real_r > 1e-4) {
                char rBuf[24];
                formatSIValue(real_r, "Ohm", rBuf, sizeof(rBuf));
                snprintf(buf, sz, "%s+%s", rBuf, cBuf);
            } else {
                snprintf(buf, sz, "%s", cBuf);
            }
        }
    }
}

// ---------------- Wire ----------------

Wire::Wire(int d1x, int d1y, int d2x, int d2y)
    : Component(COMP_WIRE, d1x, d1y, d2x, d2y, 0.0) {}

void Wire::stamp(MNASystem& /*mna*/, double /*dt*/, double /*t*/) {}

void Wire::updateState(const MNASystem& mna, double /*dt*/) {
    voltage = mna.getNodeVoltage(node1) - mna.getNodeVoltage(node2);
    // current is computed by computeWireCurrents()
    if (std::abs(current) > peak_i) peak_i = std::abs(current);
    else peak_i *= 0.995;
}

// ---------------- Ground ----------------

Ground::Ground(int d1x, int d1y)
    : Component(COMP_GROUND, d1x, d1y, d1x, d1y, 0.0) {}

void Ground::stamp(MNASystem& /*mna*/, double /*dt*/, double /*t*/) {}

void Ground::updateState(const MNASystem& /*mna*/, double /*dt*/) {
    voltage = 0.0;
    current = 0.0;
    peak_v = 0.0;
    peak_i = 0.0;
}
