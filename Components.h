#ifndef COMPONENTS_H
#define COMPONENTS_H

#include "MNAEngine.h"
#include <string>
#include <cstdio>

// Helper to format values with SI prefixes (e.g. 1.2k, 100u, 5.0m)
void formatSIValue(double val, const char* unit, char* buf, size_t sz);

class Component {
public:
    enum Type {
        COMP_WIRE = 0,
        COMP_RESISTOR,
        COMP_CAPACITOR,
        COMP_INDUCTOR,
        COMP_VOLTAGE_SRC,
        COMP_CURRENT_SRC,
        COMP_GROUND,
        COMP_AC_VOLTAGE_SRC,
        COMP_IMPEDANCE
    };

    Type type;
    int dot1_x, dot1_y; // Grid coordinate of terminal 1 (in snap grid index)
    int dot2_x, dot2_y; // Grid coordinate of terminal 2
    int node1;          // Electrical node ID assigned to terminal 1 (0 = Ground)
    int node2;          // Electrical node ID assigned to terminal 2
    int aux_index;      // Auxiliary current variable index (for voltage sources)

    double value;       // Component value (Ohms, Farads, Henrys, Volts, Amps)
    double voltage;     // Dynamic across-voltage: V(node1) - V(node2)
    double current;     // Dynamic through-current: I(node1 -> node2)

    // Companion model historical states (Backward Euler)
    double state_v;     // Capacitor previous voltage
    double state_i;     // Inductor previous current

    // AC peak / RMS tracking
    double peak_v;
    double peak_i;

public:
    Component(Type t, int d1x, int d1y, int d2x, int d2y, double val = 0.0);
    virtual ~Component() {}

    virtual void stamp(MNASystem& mna, double dt, double t = 0.0) = 0;
    virtual void updateState(const MNASystem& mna, double dt) = 0;
    virtual void resetState();

    double getRMSVoltage() const { return peak_v * 0.70710678; }
    double getRMSCurrent() const { return peak_i * 0.70710678; }

    virtual const char* getTypeName() const = 0;
    virtual const char* getUnit() const = 0;
    virtual void formatValueString(char* buf, size_t sz, bool isPhasor = false, double omega = 0.0) const;
    virtual void formatInfoString(char* buf, size_t sz, bool isPhasor = false, double omega = 0.0) const;
    virtual void formatPhasorString(char* buf, size_t sz, double omega) const;
};

class Resistor : public Component {
public:
    Resistor(int d1x, int d1y, int d2x, int d2y, double r = 1000.0);
    void stamp(MNASystem& mna, double dt, double t = 0.0) override;
    void updateState(const MNASystem& mna, double dt) override;
    const char* getTypeName() const override { return "Resistor"; }
    const char* getUnit() const override { return "Ohm"; }
    void formatPhasorString(char* buf, size_t sz, double omega) const override;
};

class VoltageSource : public Component {
public:
    VoltageSource(int d1x, int d1y, int d2x, int d2y, double v = 5.0);
    void stamp(MNASystem& mna, double dt, double t = 0.0) override;
    void updateState(const MNASystem& mna, double dt) override;
    const char* getTypeName() const override { return "DC Voltage Source"; }
    const char* getUnit() const override { return "V"; }
};

class ACVoltageSource : public Component {
public:
    double freq;   // Frequency in Hz (e.g. 60.0)
    double phase;  // Phase angle in degrees (e.g. 0, -120, +120)

    ACVoltageSource(int d1x, int d1y, int d2x, int d2y, double v = 120.0, double f = 60.0, double ph = 0.0);
    void stamp(MNASystem& mna, double dt, double t = 0.0) override;
    void updateState(const MNASystem& mna, double dt) override;
    const char* getTypeName() const override { return "AC Voltage Source"; }
    const char* getUnit() const override { return "V"; }
    void formatPhasorString(char* buf, size_t sz, double omega) const override;
};

class CurrentSource : public Component {
public:
    CurrentSource(int d1x, int d1y, int d2x, int d2y, double i = 0.010);
    void stamp(MNASystem& mna, double dt, double t = 0.0) override;
    void updateState(const MNASystem& mna, double dt) override;
    const char* getTypeName() const override { return "Current Source"; }
    const char* getUnit() const override { return "A"; }
};

class Capacitor : public Component {
public:
    Capacitor(int d1x, int d1y, int d2x, int d2y, double c = 10e-6);
    void stamp(MNASystem& mna, double dt, double t = 0.0) override;
    void updateState(const MNASystem& mna, double dt) override;
    const char* getTypeName() const override { return "Capacitor"; }
    const char* getUnit() const override { return "F"; }
    void formatPhasorString(char* buf, size_t sz, double omega) const override;
};

class Inductor : public Component {
public:
    Inductor(int d1x, int d1y, int d2x, int d2y, double l = 10e-3);
    void stamp(MNASystem& mna, double dt, double t = 0.0) override;
    void updateState(const MNASystem& mna, double dt) override;
    const char* getTypeName() const override { return "Inductor"; }
    const char* getUnit() const override { return "H"; }
    void formatPhasorString(char* buf, size_t sz, double omega) const override;
};

class GenericImpedance : public Component {
public:
    double real_r; // Resistance (Ohms)
    double imag_x; // Reactance (Ohms)

    GenericImpedance(int d1x, int d1y, int d2x, int d2y, double r = 10.0, double x = 20.0);
    void stamp(MNASystem& mna, double dt, double t = 0.0) override;
    void updateState(const MNASystem& mna, double dt) override;
    const char* getTypeName() const override { return "Impedance (Z)"; }
    const char* getUnit() const override { return "Ohm"; }
    void formatPhasorString(char* buf, size_t sz, double omega) const override;
    void formatValueString(char* buf, size_t sz, bool isPhasor = false, double omega = 0.0) const override;
};

class Wire : public Component {
public:
    Wire(int d1x, int d1y, int d2x, int d2y);
    void stamp(MNASystem& mna, double dt, double t = 0.0) override;
    void updateState(const MNASystem& mna, double dt) override;
    const char* getTypeName() const override { return "Wire"; }
    const char* getUnit() const override { return ""; }
    void formatValueString(char* buf, size_t sz, bool isPhasor = false, double omega = 0.0) const override {
        (void)isPhasor; (void)omega;
        if (sz > 0) buf[0] = '\0';
    }
};

class Ground : public Component {
public:
    Ground(int d1x, int d1y);
    void stamp(MNASystem& mna, double dt, double t = 0.0) override;
    void updateState(const MNASystem& mna, double dt) override;
    const char* getTypeName() const override { return "Ground"; }
    const char* getUnit() const override { return ""; }
    void formatValueString(char* buf, size_t sz, bool isPhasor = false, double omega = 0.0) const override {
        (void)isPhasor; (void)omega;
        if (sz > 0) buf[0] = '\0';
    }
};

#endif // COMPONENTS_H
