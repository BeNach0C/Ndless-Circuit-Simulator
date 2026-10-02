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
        COMP_GROUND
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

public:
    Component(Type t, int d1x, int d1y, int d2x, int d2y, double val = 0.0);
    virtual ~Component() {}

    virtual void stamp(MNASystem& mna, double dt) = 0;
    virtual void updateState(const MNASystem& mna, double dt) = 0;
    virtual void resetState();

    virtual const char* getTypeName() const = 0;
    virtual const char* getUnit() const = 0;
    virtual void formatValueString(char* buf, size_t sz) const;
    virtual void formatInfoString(char* buf, size_t sz) const;
};

class Resistor : public Component {
public:
    Resistor(int d1x, int d1y, int d2x, int d2y, double r = 1000.0);
    void stamp(MNASystem& mna, double dt) override;
    void updateState(const MNASystem& mna, double dt) override;
    const char* getTypeName() const override { return "Resistor"; }
    const char* getUnit() const override { return "Ohm"; }
};

class VoltageSource : public Component {
public:
    VoltageSource(int d1x, int d1y, int d2x, int d2y, double v = 5.0);
    void stamp(MNASystem& mna, double dt) override;
    void updateState(const MNASystem& mna, double dt) override;
    const char* getTypeName() const override { return "Voltage Source"; }
    const char* getUnit() const override { return "V"; }
};

class CurrentSource : public Component {
public:
    CurrentSource(int d1x, int d1y, int d2x, int d2y, double i = 0.010);
    void stamp(MNASystem& mna, double dt) override;
    void updateState(const MNASystem& mna, double dt) override;
    const char* getTypeName() const override { return "Current Source"; }
    const char* getUnit() const override { return "A"; }
};

class Capacitor : public Component {
public:
    Capacitor(int d1x, int d1y, int d2x, int d2y, double c = 10e-6);
    void stamp(MNASystem& mna, double dt) override;
    void updateState(const MNASystem& mna, double dt) override;
    const char* getTypeName() const override { return "Capacitor"; }
    const char* getUnit() const override { return "F"; }
};

class Inductor : public Component {
public:
    Inductor(int d1x, int d1y, int d2x, int d2y, double l = 10e-3);
    void stamp(MNASystem& mna, double dt) override;
    void updateState(const MNASystem& mna, double dt) override;
    const char* getTypeName() const override { return "Inductor"; }
    const char* getUnit() const override { return "H"; }
};

class Wire : public Component {
public:
    Wire(int d1x, int d1y, int d2x, int d2y);
    void stamp(MNASystem& mna, double dt) override;
    void updateState(const MNASystem& mna, double dt) override;
    const char* getTypeName() const override { return "Wire"; }
    const char* getUnit() const override { return ""; }
    void formatValueString(char* buf, size_t sz) const override {
        if (sz > 0) buf[0] = '\0';
    }
};

class Ground : public Component {
public:
    Ground(int d1x, int d1y);
    void stamp(MNASystem& mna, double dt) override;
    void updateState(const MNASystem& mna, double dt) override;
    const char* getTypeName() const override { return "Ground"; }
    const char* getUnit() const override { return ""; }
    void formatValueString(char* buf, size_t sz) const override {
        if (sz > 0) buf[0] = '\0';
    }
};

#endif // COMPONENTS_H
