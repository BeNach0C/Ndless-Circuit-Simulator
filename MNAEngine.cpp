#include "MNAEngine.h"
#include <algorithm>
#include <cmath>

MNASystem::MNASystem() : dim(0), numNodes(0), numAux(0) {}

void MNASystem::resize(int nonGroundNodes, int auxBranches) {
    numNodes = nonGroundNodes;
    numAux = auxBranches;
    dim = numNodes + numAux;
    A.assign(dim * dim, 0.0);
    z.assign(dim, 0.0);
    x.assign(dim, 0.0);
}

void MNASystem::clear() {
    std::fill(A.begin(), A.end(), 0.0);
    std::fill(z.begin(), z.end(), 0.0);
    // Add tiny Gmin shunt conductance to ground for all node diagonals
    // to prevent floating-node singularities during topology edits
    const double gmin = 1e-12;
    for (int i = 0; i < numNodes; ++i) {
        A[i * dim + i] += gmin;
    }
}

void MNASystem::stampMatrix(int r, int c, double val) {
    if (r >= 0 && r < dim && c >= 0 && c < dim) {
        A[r * dim + c] += val;
    }
}

void MNASystem::stampRHS(int r, double val) {
    if (r >= 0 && r < dim) {
        z[r] += val;
    }
}

void MNASystem::stampConductance(int n1, int n2, double g) {
    // If n1 > 0: row/col index is n1 - 1
    // If n2 > 0: row/col index is n2 - 1
    // If node is 0 (Ground), it is not in the matrix
    if (n1 > 0) {
        stampMatrix(n1 - 1, n1 - 1, +g);
    }
    if (n2 > 0) {
        stampMatrix(n2 - 1, n2 - 1, +g);
    }
    if (n1 > 0 && n2 > 0) {
        stampMatrix(n1 - 1, n2 - 1, -g);
        stampMatrix(n2 - 1, n1 - 1, -g);
    }
}

bool MNASystem::solve() {
    if (dim == 0) return true;

    // Working copies for Gaussian elimination
    std::vector<double> a_work = A;
    std::vector<double> b_work = z;

    // Forward elimination with partial pivoting
    for (int k = 0; k < dim; ++k) {
        // Find best pivot row
        int pivotRow = k;
        double maxVal = std::abs(a_work[k * dim + k]);
        for (int r = k + 1; r < dim; ++r) {
            double val = std::abs(a_work[r * dim + k]);
            if (val > maxVal) {
                maxVal = val;
                pivotRow = r;
            }
        }

        if (maxVal < 1e-14) {
            // Singular matrix
            return false;
        }

        // Swap pivot row if necessary
        if (pivotRow != k) {
            for (int c = k; c < dim; ++c) {
                std::swap(a_work[k * dim + c], a_work[pivotRow * dim + c]);
            }
            std::swap(b_work[k], b_work[pivotRow]);
        }

        double pivotVal = a_work[k * dim + k];

        // Eliminate column entries below pivot
        for (int r = k + 1; r < dim; ++r) {
            double factor = a_work[r * dim + k] / pivotVal;
            a_work[r * dim + k] = 0.0;
            for (int c = k + 1; c < dim; ++c) {
                a_work[r * dim + c] -= factor * a_work[k * dim + c];
            }
            b_work[r] -= factor * b_work[k];
        }
    }

    // Back substitution
    for (int r = dim - 1; r >= 0; --r) {
        double sum = b_work[r];
        for (int c = r + 1; c < dim; ++c) {
            sum -= a_work[r * dim + c] * x[c];
        }
        x[r] = sum / a_work[r * dim + r];
    }

    return true;
}

double MNASystem::getNodeVoltage(int node) const {
    if (node <= 0 || node > numNodes) return 0.0;
    return x[node - 1];
}

double MNASystem::getAuxCurrent(int auxIdx) const {
    if (auxIdx < 0 || auxIdx >= numAux) return 0.0;
    return x[numNodes + auxIdx];
}

// ---------------- TransientEngine ----------------

TransientEngine::TransientEngine(double timeStep)
    : dt(timeStep), currentTime(0.0), isRunning(true) {}

void TransientEngine::reset() {
    currentTime = 0.0;
}

void TransientEngine::stepTime() {
    if (isRunning) {
        currentTime += dt;
    }
}

void TransientEngine::setDt(double newDt) {
    if (newDt > 1e-9) {
        dt = newDt;
    }
}
