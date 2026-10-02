#ifndef MNA_ENGINE_H
#define MNA_ENGINE_H

#include <vector>
#include <cstddef>
#include <cmath>

/**
 * Modified Nodal Analysis (MNA) Linear System and Solver.
 * Solves A * x = z using optimized Gaussian Elimination with partial pivoting.
 * Uses 1D arrays for 2D matrices to maximize cache locality on ARM9.
 */
class MNASystem {
public:
    int dim;                      // Total system dimension (numNodes + numAux)
    int numNodes;                 // Number of non-ground nodes (indices 1..numNodes)
    int numAux;                   // Number of auxiliary branch current equations
    std::vector<double> A;        // Conductance / incidence matrix (dim * dim), row-major
    std::vector<double> z;        // RHS vector of known currents / voltages (dim)
    std::vector<double> x;        // Solution vector (dim)

public:
    MNASystem();
    void resize(int nonGroundNodes, int auxBranches);
    void clear();

    // Helper methods to stamp into A and z.
    // Node indexing: 0 = Ground (ignored in A and z), 1..numNodes = non-ground nodes.
    // Aux branch indexing: 0..numAux-1 maps to matrix index (numNodes + auxIdx).
    void stampMatrix(int r, int c, double val);
    void stampRHS(int r, double val);
    void stampConductance(int n1, int n2, double g);

    // Optimized Gaussian Elimination with partial pivoting
    bool solve();

    // Node voltage access (returns 0.0 for node 0 / ground)
    double getNodeVoltage(int node) const;
    // Auxiliary branch current access
    double getAuxCurrent(int auxIdx) const;
};

/**
 * Transient Analysis Engine for discrete time-stepping.
 */
class TransientEngine {
public:
    double dt;          // Time step (seconds), e.g. 0.001 (1ms)
    double currentTime; // Current simulated time (seconds)
    bool isRunning;     // Simulation running or paused

public:
    TransientEngine(double timeStep = 0.001);
    void reset();
    void stepTime();
    void setDt(double newDt);
};

#endif // MNA_ENGINE_H
