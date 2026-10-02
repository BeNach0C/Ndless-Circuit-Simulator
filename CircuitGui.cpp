#include "CircuitGui.h"
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

CircuitGui::CircuitGui()
    : topologyChanged(true),
      screen(nullptr), fontWhite(nullptr), fontYellow(nullptr),
      fontCyan(nullptr), fontGreen(nullptr),
      cursorX(160.0), cursorY(118.0),
      hoveredDotCol(-1), hoveredDotRow(-1),
      anchorDotCol(-1), anchorDotRow(-1),
      hoveredComponent(nullptr),
      activeTool(TOOL_WIRE), isRunning(true), quitRequested(false),
      globalOmega(377.0), isPhasorMode(false), isScopeOpen(false), isEditingOmega(false),
      scopeHead(0),
      isEditingValue(false), editingComponent(nullptr), editBufferLen(0),
      prevTouchX(0), prevTouchY(0), wasTouching(false), prevClickState(false),
      prevDelPressed(false), prevEscPressed(false) {
    editBuffer[0] = '\0';
    for (int i = 0; i < SCOPE_HISTORY; ++i) {
        scopeA[i] = 0.0;
        scopeB[i] = 0.0;
        scopeC[i] = 0.0;
    }
}

CircuitGui::~CircuitGui() {
    shutdown();
}

bool CircuitGui::init() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        return false;
    }

    screen = SDL_SetVideoMode(320, 240, has_colors ? 16 : 8, SDL_SWSURFACE);
    if (!screen) {
        SDL_Quit();
        return false;
    }

    SDL_ShowCursor(SDL_DISABLE);

    fontWhite  = nSDL_LoadFont(NSDL_FONT_TINYTYPE, 255, 255, 255);
    fontYellow = nSDL_LoadFont(NSDL_FONT_TINYTYPE, 255, 220, 50);
    fontCyan   = nSDL_LoadFont(NSDL_FONT_TINYTYPE, 80, 220, 255);
    fontGreen  = nSDL_LoadFont(NSDL_FONT_TINYTYPE, 80, 240, 100);

    engine.setDt(0.0005); // 0.5ms time-step for smooth 60Hz AC waveforms

    // Load initial circuit preset: RC low pass filter
    loadPresetRC();

    return true;
}

void CircuitGui::shutdown() {
    clearCircuit();
    if (fontWhite)  { nSDL_FreeFont(fontWhite);  fontWhite = nullptr; }
    if (fontYellow) { nSDL_FreeFont(fontYellow); fontYellow = nullptr; }
    if (fontCyan)   { nSDL_FreeFont(fontCyan);   fontCyan = nullptr; }
    if (fontGreen)  { nSDL_FreeFont(fontGreen);  fontGreen = nullptr; }
    SDL_Quit();
}

void CircuitGui::clearCircuit() {
    for (size_t i = 0; i < components.size(); ++i) {
        delete components[i];
    }
    components.clear();
    anchorDotCol = -1;
    anchorDotRow = -1;
    hoveredComponent = nullptr;
    editingComponent = nullptr;
    isEditingValue = false;
    isEditingOmega = false;
    topologyChanged = true;
    mna.resize(0, 0);
    engine.reset();
}

void CircuitGui::loadPresetRC() {
    clearCircuit();
    components.push_back(new VoltageSource(3, 6, 3, 2, 5.0));
    components.push_back(new Resistor(3, 2, 7, 2, 1000.0));
    components.push_back(new Capacitor(7, 2, 7, 6, 10e-6));
    components.push_back(new Wire(3, 6, 7, 6));
    components.push_back(new Ground(5, 6));
    topologyChanged = true;
}

void CircuitGui::loadPresetRLC() {
    clearCircuit();
    components.push_back(new VoltageSource(2, 6, 2, 2, 5.0));
    components.push_back(new Resistor(2, 2, 5, 2, 100.0));
    components.push_back(new Inductor(5, 2, 8, 2, 10e-3));
    components.push_back(new Capacitor(8, 2, 8, 6, 10e-6));
    components.push_back(new Wire(2, 6, 8, 6));
    components.push_back(new Ground(5, 6));
    topologyChanged = true;
}

void CircuitGui::loadPresetBridge() {
    clearCircuit();
    components.push_back(new VoltageSource(2, 7, 2, 2, 10.0));
    components.push_back(new Wire(2, 2, 6, 2));
    components.push_back(new Resistor(6, 2, 4, 4, 1000.0));
    components.push_back(new Resistor(4, 4, 6, 7, 2000.0));
    components.push_back(new Resistor(6, 2, 8, 4, 1000.0));
    components.push_back(new Resistor(8, 4, 6, 7, 1000.0));
    components.push_back(new Resistor(4, 4, 8, 4, 500.0));
    components.push_back(new Wire(2, 7, 6, 7));
    components.push_back(new Ground(6, 7));
    topologyChanged = true;
}

// ---------------- 3-Phase Macros ----------------

void CircuitGui::addWyeGeneratorMacro() {
    clearCircuit();
    // 3 AC Sources connected in Wye (Star) with neutral rail along row 5
    // Phase A: from (2,5) to (2,2), 120V, 60Hz, 0 deg
    components.push_back(new ACVoltageSource(2, 5, 2, 2, 120.0, 60.0, 0.0));
    // Phase B: from (4,5) to (4,2), 120V, 60Hz, -120 deg
    components.push_back(new ACVoltageSource(4, 5, 4, 2, 120.0, 60.0, -120.0));
    // Phase C: from (6,5) to (6,2), 120V, 60Hz, +120 deg
    components.push_back(new ACVoltageSource(6, 5, 6, 2, 120.0, 60.0, 120.0));

    // Neutral bus wires connecting the bottoms
    components.push_back(new Wire(2, 5, 4, 5));
    components.push_back(new Wire(4, 5, 6, 5));
    components.push_back(new Ground(4, 5));

    topologyChanged = true;
    isScopeOpen = true; // Auto-open scope to view the 3-phase waveforms!
}

void CircuitGui::addDeltaGeneratorMacro() {
    clearCircuit();
    // 3 AC Sources connected in Delta (Mesh loop)
    // Source AB: from (2, 2) to (6, 2), 120V, 60Hz, 0 deg
    components.push_back(new ACVoltageSource(2, 2, 6, 2, 120.0, 60.0, 0.0));
    // Source BC: from (6, 2) to (4, 6), 120V, 60Hz, -120 deg
    components.push_back(new ACVoltageSource(6, 2, 4, 6, 120.0, 60.0, -120.0));
    // Source CA: from (4, 6) to (2, 2), 120V, 60Hz, +120 deg
    components.push_back(new ACVoltageSource(4, 6, 2, 2, 120.0, 60.0, 120.0));
    // Ground reference at C
    components.push_back(new Ground(4, 6));

    topologyChanged = true;
    isScopeOpen = true;
}

void CircuitGui::togglePhasorMode() {
    isPhasorMode = !isPhasorMode;
}

void CircuitGui::toggleScope() {
    isScopeOpen = !isScopeOpen;
}

int CircuitGui::getClosestDotCol(int px) const {
    int col = (px - GRID_ORIGIN_X + GRID_SPACING / 2) / GRID_SPACING;
    if (col >= 0 && col < GRID_COLS) {
        int dotPx = getDotPixelX(col);
        if (std::abs(dotPx - px) <= 12) return col;
    }
    return -1;
}

int CircuitGui::getClosestDotRow(int py) const {
    int row = (py - GRID_ORIGIN_Y + GRID_SPACING / 2) / GRID_SPACING;
    if (row >= 0 && row < GRID_ROWS) {
        int dotPy = getDotPixelY(row);
        if (std::abs(dotPy - py) <= 12) return row;
    }
    return -1;
}

Component* CircuitGui::getComponentAt(int px, int py) {
    Component* closest = nullptr;
    double minDistance = 8.0;

    for (size_t i = 0; i < components.size(); ++i) {
        Component* c = components[i];
        int x1 = getDotPixelX(c->dot1_x);
        int y1 = getDotPixelY(c->dot1_y);

        if (c->type == Component::COMP_GROUND) {
            double d = std::hypot(px - x1, py - (y1 + 6));
            if (d < minDistance) {
                minDistance = d;
                closest = c;
            }
            continue;
        }

        int x2 = getDotPixelX(c->dot2_x);
        int y2 = getDotPixelY(c->dot2_y);

        double dx = x2 - x1;
        double dy = y2 - y1;
        double lenSq = dx * dx + dy * dy;
        if (lenSq < 1e-4) continue;

        double t = ((px - x1) * dx + (py - y1) * dy) / lenSq;
        t = std::max(0.0, std::min(1.0, t));
        double projX = x1 + t * dx;
        double projY = y1 + t * dy;
        double dist = std::hypot(px - projX, py - projY);

        if (dist < minDistance) {
            minDistance = dist;
            closest = c;
        }
    }
    return closest;
}

void CircuitGui::rebuildTopology() {
    const int totalDots = GRID_COLS * GRID_ROWS;
    std::vector<int> parent(totalDots);
    for (int i = 0; i < totalDots; ++i) parent[i] = i;

    auto findRoot = [&](int i, auto& self) -> int {
        return parent[i] == i ? i : (parent[i] = self(parent[i], self));
    };
    auto unite = [&](int a, int b) {
        int ra = findRoot(a, findRoot);
        int rb = findRoot(b, findRoot);
        if (ra != rb) parent[ra] = rb;
    };

    for (size_t i = 0; i < components.size(); ++i) {
        Component* c = components[i];
        if (c->type == Component::COMP_WIRE) {
            int d1 = c->dot1_y * GRID_COLS + c->dot1_x;
            int d2 = c->dot2_y * GRID_COLS + c->dot2_x;
            unite(d1, d2);
        }
    }

    std::vector<bool> isGndRoot(totalDots, false);
    bool hasExplicitGround = false;
    for (size_t i = 0; i < components.size(); ++i) {
        Component* c = components[i];
        if (c->type == Component::COMP_GROUND) {
            int d = c->dot1_y * GRID_COLS + c->dot1_x;
            isGndRoot[findRoot(d, findRoot)] = true;
            hasExplicitGround = true;
        }
    }

    if (!hasExplicitGround && !components.empty()) {
        for (size_t i = 0; i < components.size(); ++i) {
            Component* c = components[i];
            if (c->type != Component::COMP_GROUND && c->type != Component::COMP_WIRE) {
                int d = c->dot2_y * GRID_COLS + c->dot2_x;
                isGndRoot[findRoot(d, findRoot)] = true;
                hasExplicitGround = true;
                break;
            }
        }
    }

    std::vector<int> rootToNode(totalDots, -1);
    if (hasExplicitGround) {
        for (int i = 0; i < totalDots; ++i) {
            if (isGndRoot[i]) rootToNode[i] = 0;
        }
    }

    int nextNodeId = 1;
    for (size_t i = 0; i < components.size(); ++i) {
        Component* c = components[i];
        int d1 = c->dot1_y * GRID_COLS + c->dot1_x;
        int r1 = findRoot(d1, findRoot);
        if (rootToNode[r1] < 0) rootToNode[r1] = nextNodeId++;

        if (c->type != Component::COMP_GROUND) {
            int d2 = c->dot2_y * GRID_COLS + c->dot2_x;
            int r2 = findRoot(d2, findRoot);
            if (rootToNode[r2] < 0) rootToNode[r2] = nextNodeId++;
        }
    }

    int nonGroundNodes = nextNodeId - 1;
    int auxCount = 0;
    for (size_t i = 0; i < components.size(); ++i) {
        Component* c = components[i];
        int d1 = c->dot1_y * GRID_COLS + c->dot1_x;
        c->node1 = rootToNode[findRoot(d1, findRoot)];

        if (c->type == Component::COMP_GROUND) {
            c->node2 = 0;
        } else {
            int d2 = c->dot2_y * GRID_COLS + c->dot2_x;
            c->node2 = rootToNode[findRoot(d2, findRoot)];
        }

        if (c->type == Component::COMP_VOLTAGE_SRC || c->type == Component::COMP_AC_VOLTAGE_SRC) {
            c->aux_index = auxCount++;
        } else {
            c->aux_index = -1;
        }
    }

    mna.resize(nonGroundNodes, auxCount);
    topologyChanged = false;
}

void CircuitGui::simulationStep() {
    if (topologyChanged) {
        rebuildTopology();
    }

    if (!isRunning || mna.dim <= 0) return;

    // Run 2 discrete solver steps per display frame for accurate integration
    for (int step = 0; step < 2; ++step) {
        mna.clear();
        for (size_t i = 0; i < components.size(); ++i) {
            components[i]->stamp(mna, engine.dt, engine.currentTime);
        }

        if (mna.solve()) {
            for (size_t i = 0; i < components.size(); ++i) {
                components[i]->updateState(mna, engine.dt);
            }
            computeWireCurrents();
            engine.stepTime();
        }
    }

    // Sample voltages for mini-scope buffer
    double va = 0.0, vb = 0.0, vc = 0.0;
    int acCount = 0;
    for (size_t i = 0; i < components.size(); ++i) {
        if (components[i]->type == Component::COMP_AC_VOLTAGE_SRC) {
            if (acCount == 0) va = components[i]->voltage;
            else if (acCount == 1) vb = components[i]->voltage;
            else if (acCount == 2) vc = components[i]->voltage;
            acCount++;
        }
    }

    if (acCount == 0 && hoveredComponent) {
        va = hoveredComponent->voltage;
        vb = hoveredComponent->current * 100.0;
        vc = hoveredComponent->getRMSVoltage();
    }

    scopeA[scopeHead] = va;
    scopeB[scopeHead] = vb;
    scopeC[scopeHead] = vc;
    scopeHead = (scopeHead + 1) % SCOPE_HISTORY;
}

void CircuitGui::computeWireCurrents() {
    const int totalDots = GRID_COLS * GRID_ROWS;
    std::vector<double> inj(totalDots, 0.0);

    for (size_t i = 0; i < components.size(); ++i) {
        Component* c = components[i];
        if (c->type == Component::COMP_WIRE || c->type == Component::COMP_GROUND) {
            continue;
        }

        int d1 = c->dot1_y * GRID_COLS + c->dot1_x;
        int d2 = c->dot2_y * GRID_COLS + c->dot2_x;

        if (c->type == Component::COMP_VOLTAGE_SRC || c->type == Component::COMP_AC_VOLTAGE_SRC) {
            inj[d1] += c->current;
            inj[d2] -= c->current;
        } else {
            inj[d1] -= c->current;
            inj[d2] += c->current;
        }
    }

    std::vector<int> wireIndices;
    std::vector<int> degree(totalDots, 0);
    std::vector<std::vector<int>> adj(totalDots);

    for (size_t i = 0; i < components.size(); ++i) {
        if (components[i]->type == Component::COMP_WIRE) {
            Wire* w = static_cast<Wire*>(components[i]);
            w->current = 0.0;
            int d1 = w->dot1_y * GRID_COLS + w->dot1_x;
            int d2 = w->dot2_y * GRID_COLS + w->dot2_x;
            if (d1 == d2) continue;

            int wIdx = (int)wireIndices.size();
            wireIndices.push_back(i);
            degree[d1]++;
            degree[d2]++;
            adj[d1].push_back(wIdx);
            adj[d2].push_back(wIdx);
        }
    }

    std::vector<bool> resolved(wireIndices.size(), false);
    std::vector<int> leaves;
    for (int i = 0; i < totalDots; ++i) {
        if (degree[i] == 1) leaves.push_back(i);
    }

    size_t head = 0;
    while (head < leaves.size()) {
        int u = leaves[head++];
        if (degree[u] == 0) continue;

        int chosenWIdx = -1;
        for (int wIdx : adj[u]) {
            if (!resolved[wIdx]) {
                chosenWIdx = wIdx;
                break;
            }
        }
        if (chosenWIdx == -1) continue;

        Wire* w = static_cast<Wire*>(components[wireIndices[chosenWIdx]]);
        int d1 = w->dot1_y * GRID_COLS + w->dot1_x;
        int d2 = w->dot2_y * GRID_COLS + w->dot2_x;
        int v = (d1 == u) ? d2 : d1;

        if (d1 == u) {
            w->current = inj[u];
        } else {
            w->current = -inj[u];
        }
        resolved[chosenWIdx] = true;

        inj[v] += inj[u];
        degree[u]--;
        degree[v]--;
        if (degree[v] == 1) {
            leaves.push_back(v);
        }
    }
}

// ---------------- Rendering ----------------

void CircuitGui::drawGrid() {
    SDL_FillRect(screen, nullptr, SDL_MapRGB(screen->format, 10, 15, 22));

    for (int r = 0; r < GRID_ROWS; ++r) {
        for (int c = 0; c < GRID_COLS; ++c) {
            int px = getDotPixelX(c);
            int py = getDotPixelY(r);
            bool isHovered = (c == hoveredDotCol && r == hoveredDotRow);
            bool isAnchor = (c == anchorDotCol && r == anchorDotRow);

            if (isAnchor) {
                circleRGBA(screen, px, py, 4, 255, 215, 0, 255);
            } else if (isHovered) {
                circleRGBA(screen, px, py, 3, 80, 220, 255, 255);
            } else {
                pixelRGBA(screen, px, py, 45, 65, 85, 255);
            }
        }
    }
}

void CircuitGui::drawComponents() {
    for (size_t i = 0; i < components.size(); ++i) {
        drawComponent(components[i]);
    }

    if (anchorDotCol >= 0 && anchorDotRow >= 0 && hoveredDotCol >= 0 && hoveredDotRow >= 0) {
        if (anchorDotCol != hoveredDotCol || anchorDotRow != hoveredDotRow) {
            int x1 = getDotPixelX(anchorDotCol);
            int y1 = getDotPixelY(anchorDotRow);
            int x2 = getDotPixelX(hoveredDotCol);
            int y2 = getDotPixelY(hoveredDotRow);
            lineRGBA(screen, x1, y1, x2, y2, 255, 220, 50, 180);
        }
    }
}

void CircuitGui::drawComponent(Component* comp) {
    int x1 = getDotPixelX(comp->dot1_x);
    int y1 = getDotPixelY(comp->dot1_y);
    bool isHovered = (comp == hoveredComponent);

    if (comp->type == Component::COMP_GROUND) {
        Uint8 r = isHovered ? 255 : 210;
        Uint8 g = isHovered ? 230 : 215;
        Uint8 b = isHovered ? 80  : 220;
        lineRGBA(screen, x1, y1, x1, y1 + 6, r, g, b, 255);
        lineRGBA(screen, x1 - 7, y1 + 6,  x1 + 7, y1 + 6,  r, g, b, 255);
        lineRGBA(screen, x1 - 4, y1 + 9,  x1 + 4, y1 + 9,  r, g, b, 255);
        lineRGBA(screen, x1 - 1, y1 + 12, x1 + 1, y1 + 12, r, g, b, 255);
        return;
    }

    int x2 = getDotPixelX(comp->dot2_x);
    int y2 = getDotPixelY(comp->dot2_y);

    double dx = x2 - x1;
    double dy = y2 - y1;
    double len = std::hypot(dx, dy);
    if (len < 1.0) return;

    double ux = dx / len;
    double uy = dy / len;
    double nx = -uy;
    double ny = ux;

    char valStr[32];
    comp->formatValueString(valStr, sizeof(valStr), isPhasorMode, globalOmega);

    switch (comp->type) {
    case Component::COMP_WIRE: {
        Uint8 r = isHovered ? 255 : 0;
        Uint8 g = isHovered ? 240 : 220;
        Uint8 b = isHovered ? 100 : 220;
        lineRGBA(screen, x1, y1, x2, y2, r, g, b, 255);
        break;
    }
    case Component::COMP_RESISTOR: {
        Uint8 r = isHovered ? 255 : 255;
        Uint8 g = isHovered ? 230 : 180;
        Uint8 b = isHovered ? 50  : 50;

        int pax = (int)(x1 + 0.25 * len * ux);
        int pay = (int)(y1 + 0.25 * len * uy);
        int pbx = (int)(x1 + 0.75 * len * ux);
        int pby = (int)(y1 + 0.75 * len * uy);

        lineRGBA(screen, x1, y1, pax, pay, r, g, b, 255);
        lineRGBA(screen, pbx, pby, x2, y2, r, g, b, 255);

        double bodyLen = 0.5 * len;
        double zx[7], zy[7];
        zx[0] = pax; zy[0] = pay;
        zx[6] = pbx; zy[6] = pby;
        for (int i = 1; i <= 5; ++i) {
            double frac = (double)i / 6.0;
            double sign = (i % 2 == 1) ? 1.0 : -1.0;
            zx[i] = pax + frac * bodyLen * ux + sign * 5.0 * nx;
            zy[i] = pay + frac * bodyLen * uy + sign * 5.0 * ny;
        }
        for (int i = 0; i < 6; ++i) {
            lineRGBA(screen, (int)zx[i], (int)zy[i], (int)zx[i+1], (int)zy[i+1], r, g, b, 255);
        }
        int tx = (x1 + x2) / 2 + (int)(nx * 9);
        int ty = (y1 + y2) / 2 + (int)(ny * 9) - 3;
        nSDL_DrawString(screen, fontWhite, tx, ty, "%s", valStr);
        break;
    }
    case Component::COMP_CAPACITOR: {
        Uint8 r = isHovered ? 255 : 80;
        Uint8 g = isHovered ? 240 : 220;
        Uint8 b = isHovered ? 100 : 120;

        int pax = (int)(x1 + (0.5 * len - 3.0) * ux);
        int pay = (int)(y1 + (0.5 * len - 3.0) * uy);
        int pbx = (int)(x1 + (0.5 * len + 3.0) * ux);
        int pby = (int)(y1 + (0.5 * len + 3.0) * uy);

        lineRGBA(screen, x1, y1, pax, pay, r, g, b, 255);
        lineRGBA(screen, pbx, pby, x2, y2, r, g, b, 255);

        lineRGBA(screen, (int)(pax - 7.0 * nx), (int)(pay - 7.0 * ny),
                         (int)(pax + 7.0 * nx), (int)(pay + 7.0 * ny), r, g, b, 255);
        lineRGBA(screen, (int)(pbx - 7.0 * nx), (int)(pby - 7.0 * ny),
                         (int)(pbx + 7.0 * nx), (int)(pby + 7.0 * ny), r, g, b, 255);

        int tx = (x1 + x2) / 2 + (int)(nx * 10);
        int ty = (y1 + y2) / 2 + (int)(ny * 10) - 3;
        nSDL_DrawString(screen, fontWhite, tx, ty, "%s", valStr);
        break;
    }
    case Component::COMP_INDUCTOR: {
        Uint8 r = isHovered ? 255 : 180;
        Uint8 g = isHovered ? 230 : 120;
        Uint8 b = isHovered ? 80  : 255;

        int pax = (int)(x1 + 0.20 * len * ux);
        int pay = (int)(y1 + 0.20 * len * uy);
        int pbx = (int)(x1 + 0.80 * len * ux);
        int pby = (int)(y1 + 0.80 * len * uy);

        lineRGBA(screen, x1, y1, pax, pay, r, g, b, 255);
        lineRGBA(screen, pbx, pby, x2, y2, r, g, b, 255);

        const int numCoils = 4;
        const int segsPerCoil = 6;
        double coilLen = (0.60 * len) / numCoils;
        double coilRadius = 4.5;

        int prevX = pax;
        int prevY = pay;
        for (int c = 0; c < numCoils; ++c) {
            double cStartX = pax + c * coilLen * ux;
            double cStartY = pay + c * coilLen * uy;
            for (int s = 1; s <= segsPerCoil; ++s) {
                double theta = M_PI * ((double)s / segsPerCoil);
                double fAlong = (double)s / segsPerCoil;
                double h = std::sin(theta) * coilRadius;
                int curX = (int)(cStartX + fAlong * coilLen * ux + h * nx);
                int curY = (int)(cStartY + fAlong * coilLen * uy + h * ny);
                lineRGBA(screen, prevX, prevY, curX, curY, r, g, b, 255);
                prevX = curX;
                prevY = curY;
            }
        }
        lineRGBA(screen, prevX, prevY, pbx, pby, r, g, b, 255);

        int tx = (x1 + x2) / 2 + (int)(nx * 11);
        int ty = (y1 + y2) / 2 + (int)(ny * 11) - 3;
        nSDL_DrawString(screen, fontWhite, tx, ty, "%s", valStr);
        break;
    }
    case Component::COMP_VOLTAGE_SRC: {
        Uint8 r = isHovered ? 255 : 60;
        Uint8 g = isHovered ? 230 : 160;
        Uint8 b = isHovered ? 80  : 255;

        int midX = (x1 + x2) / 2;
        int midY = (y1 + y2) / 2;
        int rad = 9;

        lineRGBA(screen, x1, y1, (int)(midX - rad * ux), (int)(midY - rad * uy), r, g, b, 255);
        lineRGBA(screen, (int)(midX + rad * ux), (int)(midY + rad * uy), x2, y2, r, g, b, 255);

        circleRGBA(screen, midX, midY, rad, r, g, b, 255);
        lineRGBA(screen, (int)(midX - 3 * ux), (int)(midY - 3 * uy),
                         (int)(midX - 7 * ux), (int)(midY - 7 * uy), r, g, b, 255);
        lineRGBA(screen, (int)(midX - 5 * ux - 2 * nx), (int)(midY - 5 * uy - 2 * ny),
                         (int)(midX - 5 * ux + 2 * nx), (int)(midY - 5 * uy + 2 * ny), r, g, b, 255);
        lineRGBA(screen, (int)(midX + 5 * ux - 2 * nx), (int)(midY + 5 * uy - 2 * ny),
                         (int)(midX + 5 * ux + 2 * nx), (int)(midY + 5 * uy + 2 * ny), r, g, b, 255);

        int tx = midX + (int)(nx * 12);
        int ty = midY + (int)(ny * 12) - 3;
        nSDL_DrawString(screen, fontWhite, tx, ty, "%s", valStr);
        break;
    }
    case Component::COMP_AC_VOLTAGE_SRC: {
        Uint8 r = isHovered ? 255 : 255;
        Uint8 g = isHovered ? 230 : 150;
        Uint8 b = isHovered ? 80  : 50;

        int midX = (x1 + x2) / 2;
        int midY = (y1 + y2) / 2;
        int rad = 10;

        lineRGBA(screen, x1, y1, (int)(midX - rad * ux), (int)(midY - rad * uy), r, g, b, 255);
        lineRGBA(screen, (int)(midX + rad * ux), (int)(midY + rad * uy), x2, y2, r, g, b, 255);

        circleRGBA(screen, midX, midY, rad, r, g, b, 255);

        // Sine wave symbol inside circle
        int prevPx = midX - 6;
        int prevPy = midY;
        for (int dx = -5; dx <= 6; ++dx) {
            double ang = (dx / 6.0) * M_PI;
            int dy = (int)(std::sin(ang) * -4.0);
            int curPx = midX + dx;
            int curPy = midY + dy;
            lineRGBA(screen, prevPx, prevPy, curPx, curPy, r, g, b, 255);
            prevPx = curPx;
            prevPy = curPy;
        }

        int tx = midX + (int)(nx * 13);
        int ty = midY + (int)(ny * 13) - 3;
        nSDL_DrawString(screen, fontYellow, tx, ty, "%s", valStr);
        break;
    }
    case Component::COMP_CURRENT_SRC: {
        Uint8 r = isHovered ? 255 : 255;
        Uint8 g = isHovered ? 230 : 120;
        Uint8 b = isHovered ? 80  : 80;

        int midX = (x1 + x2) / 2;
        int midY = (y1 + y2) / 2;
        int rad = 9;

        lineRGBA(screen, x1, y1, (int)(midX - rad * ux), (int)(midY - rad * uy), r, g, b, 255);
        lineRGBA(screen, (int)(midX + rad * ux), (int)(midY + rad * uy), x2, y2, r, g, b, 255);

        circleRGBA(screen, midX, midY, rad, r, g, b, 255);
        lineRGBA(screen, (int)(midX - 5 * ux), (int)(midY - 5 * uy),
                         (int)(midX + 5 * ux), (int)(midY + 5 * uy), r, g, b, 255);
        lineRGBA(screen, (int)(midX + 5 * ux), (int)(midY + 5 * uy),
                         (int)(midX + 2 * ux - 2 * nx), (int)(midY + 2 * uy - 2 * ny), r, g, b, 255);
        lineRGBA(screen, (int)(midX + 5 * ux), (int)(midY + 5 * uy),
                         (int)(midX + 2 * ux + 2 * nx), (int)(midY + 2 * uy + 2 * ny), r, g, b, 255);

        int tx = midX + (int)(nx * 12);
        int ty = midY + (int)(ny * 12) - 3;
        nSDL_DrawString(screen, fontWhite, tx, ty, "%s", valStr);
        break;
    }
    case Component::COMP_IMPEDANCE: {
        GenericImpedance* z = static_cast<GenericImpedance*>(comp);
        Uint8 r = isHovered ? 255 : 200;
        Uint8 g = isHovered ? 230 : 180;
        Uint8 b = isHovered ? 80  : 240;

        if (isPhasorMode) {
            // Drawn as standard rectangle impedance block
            int pax = (int)(x1 + 0.25 * len * ux);
            int pay = (int)(y1 + 0.25 * len * uy);
            int pbx = (int)(x1 + 0.75 * len * ux);
            int pby = (int)(y1 + 0.75 * len * uy);

            lineRGBA(screen, x1, y1, pax, pay, r, g, b, 255);
            lineRGBA(screen, pbx, pby, x2, y2, r, g, b, 255);

            int x_tl = (int)(pax - 5.0 * nx);
            int y_tl = (int)(pay - 5.0 * ny);
            int x_tr = (int)(pbx - 5.0 * nx);
            int y_tr = (int)(pby - 5.0 * ny);
            int x_br = (int)(pbx + 5.0 * nx);
            int y_br = (int)(pby + 5.0 * ny);
            int x_bl = (int)(pax + 5.0 * nx);
            int y_bl = (int)(pay + 5.0 * ny);

            lineRGBA(screen, x_tl, y_tl, x_tr, y_tr, r, g, b, 255);
            lineRGBA(screen, x_tr, y_tr, x_br, y_br, r, g, b, 255);
            lineRGBA(screen, x_br, y_br, x_bl, y_bl, r, g, b, 255);
            lineRGBA(screen, x_bl, y_bl, x_tl, y_tl, r, g, b, 255);
        } else {
            // In Time Mode: draw as inductor or resistor depending on complex value
            if (std::abs(z->imag_x) < 1e-4) {
                // Pure Resistor
                int pax = (int)(x1 + 0.25 * len * ux);
                int pay = (int)(y1 + 0.25 * len * uy);
                int pbx = (int)(x1 + 0.75 * len * ux);
                int pby = (int)(y1 + 0.75 * len * uy);
                lineRGBA(screen, x1, y1, pax, pay, r, g, b, 255);
                lineRGBA(screen, pbx, pby, x2, y2, r, g, b, 255);
                double bodyLen = 0.5 * len;
                double zx[7], zy[7];
                zx[0] = pax; zy[0] = pay; zx[6] = pbx; zy[6] = pby;
                for (int i = 1; i <= 5; ++i) {
                    double frac = (double)i / 6.0;
                    double sign = (i % 2 == 1) ? 1.0 : -1.0;
                    zx[i] = pax + frac * bodyLen * ux + sign * 5.0 * nx;
                    zy[i] = pay + frac * bodyLen * uy + sign * 5.0 * ny;
                }
                for (int i = 0; i < 6; ++i) {
                    lineRGBA(screen, (int)zx[i], (int)zy[i], (int)zx[i+1], (int)zy[i+1], r, g, b, 255);
                }
            } else if (z->imag_x > 1e-4) {
                // Inductor
                int pax = (int)(x1 + 0.20 * len * ux);
                int pay = (int)(y1 + 0.20 * len * uy);
                int pbx = (int)(x1 + 0.80 * len * ux);
                int pby = (int)(y1 + 0.80 * len * uy);
                lineRGBA(screen, x1, y1, pax, pay, r, g, b, 255);
                lineRGBA(screen, pbx, pby, x2, y2, r, g, b, 255);
                const int numCoils = 4;
                const int segsPerCoil = 6;
                double coilLen = (0.60 * len) / numCoils;
                int prevX = pax, prevY = pay;
                for (int c = 0; c < numCoils; ++c) {
                    double cStartX = pax + c * coilLen * ux;
                    double cStartY = pay + c * coilLen * uy;
                    for (int s = 1; s <= segsPerCoil; ++s) {
                        double theta = M_PI * ((double)s / segsPerCoil);
                        double fAlong = (double)s / segsPerCoil;
                        double h = std::sin(theta) * 4.5;
                        int curX = (int)(cStartX + fAlong * coilLen * ux + h * nx);
                        int curY = (int)(cStartY + fAlong * coilLen * uy + h * ny);
                        lineRGBA(screen, prevX, prevY, curX, curY, r, g, b, 255);
                        prevX = curX; prevY = curY;
                    }
                }
                lineRGBA(screen, prevX, prevY, pbx, pby, r, g, b, 255);
            } else {
                // Capacitor
                int pax = (int)(x1 + (0.5 * len - 3.0) * ux);
                int pay = (int)(y1 + (0.5 * len - 3.0) * uy);
                int pbx = (int)(x1 + (0.5 * len + 3.0) * ux);
                int pby = (int)(y1 + (0.5 * len + 3.0) * uy);
                lineRGBA(screen, x1, y1, pax, pay, r, g, b, 255);
                lineRGBA(screen, pbx, pby, x2, y2, r, g, b, 255);
                lineRGBA(screen, (int)(pax - 7.0 * nx), (int)(pay - 7.0 * ny),
                                 (int)(pax + 7.0 * nx), (int)(pay + 7.0 * ny), r, g, b, 255);
                lineRGBA(screen, (int)(pbx - 7.0 * nx), (int)(pby - 7.0 * ny),
                                 (int)(pbx + 7.0 * nx), (int)(pby + 7.0 * ny), r, g, b, 255);
            }
        }
        int tx = (x1 + x2) / 2 + (int)(nx * 11);
        int ty = (y1 + y2) / 2 + (int)(ny * 11) - 3;
        nSDL_DrawString(screen, fontWhite, tx, ty, "%s", valStr);
        break;
    }
    default:
        break;
    }
}

void CircuitGui::drawTopBar() {
    boxRGBA(screen, 0, 0, 320, 18, 14, 20, 30, 245);
    lineRGBA(screen, 0, 18, 320, 18, 40, 60, 85, 255);

    // 1. Run / Pause
    if (isRunning) {
        nSDL_DrawString(screen, fontGreen, 4, 4, "[RUN]");
    } else {
        nSDL_DrawString(screen, fontYellow, 4, 4, "[PAUSE]");
    }

    // 2. Scope toggle button hint
    nSDL_DrawString(screen, isScopeOpen ? fontYellow : fontWhite, 40, 4, isScopeOpen ? "[S:Hide]" : "S:Scope");

    // 3. Frequency & Omega
    if (globalOmega > 0.0) {
        double freqHz = globalOmega / (2.0 * M_PI);
        nSDL_DrawString(screen, fontCyan, 88, 4, "f=%.0fHz (w=%.0f)", freqHz, globalOmega);
    } else {
        nSDL_DrawString(screen, fontCyan, 88, 4, "w=? (Symbolic)");
    }

    // 4. Elapsed Time
    char timeBuf[24];
    formatSIValue(engine.currentTime, "s", timeBuf, sizeof(timeBuf));
    nSDL_DrawString(screen, fontWhite, 196, 4, "t=%s", timeBuf);

    // 5. Mode indicator
    if (isPhasorMode) {
        nSDL_DrawString(screen, fontYellow, 252, 4, "[PHASOR]");
    } else {
        nSDL_DrawString(screen, fontGreen, 260, 4, "[TIME]");
    }
}

void CircuitGui::drawBottomToolbar() {
    boxRGBA(screen, 0, 218, 320, 240, 14, 20, 30, 245);
    lineRGBA(screen, 0, 218, 320, 218, 40, 60, 85, 255);

    static const char* toolLabels[NUM_TOOLS] = {
        "W", "R", "C", "L", "DC", "AC", "I", "Z", "G", "DEL", "EDT"
    };

    int btnWidth = 21;
    int btnHeight = 18;
    int startX = 2;
    int startY = 220;

    for (int i = 0; i < NUM_TOOLS; ++i) {
        int bx = startX + i * (btnWidth + 1);
        bool isActive = (i == activeTool);

        if (isActive) {
            boxRGBA(screen, bx, startY, bx + btnWidth, startY + btnHeight, 40, 90, 160, 255);
            rectangleRGBA(screen, bx, startY, bx + btnWidth, startY + btnHeight, 255, 215, 0, 255);
            nSDL_DrawString(screen, fontYellow, bx + 2, startY + 4, "%s", toolLabels[i]);
        } else {
            boxRGBA(screen, bx, startY, bx + btnWidth, startY + btnHeight, 28, 38, 52, 255);
            rectangleRGBA(screen, bx, startY, bx + btnWidth, startY + btnHeight, 55, 75, 100, 255);
            nSDL_DrawString(screen, fontWhite, bx + 2, startY + 4, "%s", toolLabels[i]);
        }
    }

    // Play/Pause button
    int pbx = startX + NUM_TOOLS * (btnWidth + 1);
    boxRGBA(screen, pbx, startY, pbx + 18, startY + btnHeight, 28, 38, 52, 255);
    rectangleRGBA(screen, pbx, startY, pbx + 18, startY + btnHeight, 55, 75, 100, 255);
    nSDL_DrawString(screen, isRunning ? fontGreen : fontYellow, pbx + 3, startY + 4, "%s", isRunning ? "||" : ">");

    // Clear button
    int clrX = pbx + 20;
    boxRGBA(screen, clrX, startY, clrX + 22, startY + btnHeight, 28, 38, 52, 255);
    rectangleRGBA(screen, clrX, startY, clrX + 22, startY + btnHeight, 55, 75, 100, 255);
    nSDL_DrawString(screen, fontWhite, clrX + 3, startY + 4, "Clr");

    // Phasor toggle button
    int phX = clrX + 24;
    boxRGBA(screen, phX, startY, phX + 24, startY + btnHeight, isPhasorMode ? 60 : 28, isPhasorMode ? 100 : 38, isPhasorMode ? 140 : 52, 255);
    rectangleRGBA(screen, phX, startY, phX + 24, startY + btnHeight, isPhasorMode ? 255 : 55, isPhasorMode ? 215 : 75, isPhasorMode ? 0 : 100, 255);
    nSDL_DrawString(screen, isPhasorMode ? fontYellow : fontWhite, phX + 4, startY + 4, "PH");
}

void CircuitGui::drawScope() {
    if (!isScopeOpen) return;

    int sx1 = 12, sy1 = 136, sx2 = 308, sy2 = 214;
    boxRGBA(screen, sx1, sy1, sx2, sy2, 10, 16, 26, 240);
    rectangleRGBA(screen, sx1, sy1, sx2, sy2, 80, 200, 240, 220);

    int midY = (sy1 + sy2) / 2 + 6;
    lineRGBA(screen, sx1 + 4, midY, sx2 - 4, midY, 60, 80, 110, 180);

    nSDL_DrawString(screen, fontCyan, sx1 + 6, sy1 + 3, "SCOPE: 3-Phase AC [Yel=A, Grn=B, Cyn=C]");

    double maxVal = 1.0;
    for (int i = 0; i < SCOPE_HISTORY; ++i) {
        if (std::abs(scopeA[i]) > maxVal) maxVal = std::abs(scopeA[i]);
        if (std::abs(scopeB[i]) > maxVal) maxVal = std::abs(scopeB[i]);
        if (std::abs(scopeC[i]) > maxVal) maxVal = std::abs(scopeC[i]);
    }
    double scale = 26.0 / maxVal;

    int plotWidth = (sx2 - sx1 - 16);
    int prevXa = 0, prevYa = 0;
    int prevXb = 0, prevYb = 0;
    int prevXc = 0, prevYc = 0;

    for (int i = 0; i < SCOPE_HISTORY; ++i) {
        int idx = (scopeHead + i) % SCOPE_HISTORY;
        int px = sx1 + 8 + (i * plotWidth) / (SCOPE_HISTORY - 1);
        int ya = midY - (int)(scopeA[idx] * scale);
        int yb = midY - (int)(scopeB[idx] * scale);
        int yc = midY - (int)(scopeC[idx] * scale);

        ya = std::max(sy1 + 16, std::min(sy2 - 4, ya));
        yb = std::max(sy1 + 16, std::min(sy2 - 4, yb));
        yc = std::max(sy1 + 16, std::min(sy2 - 4, yc));

        if (i > 0) {
            lineRGBA(screen, prevXa, prevYa, px, ya, 255, 220, 50, 255); // Phase A: Yellow
            lineRGBA(screen, prevXb, prevYb, px, yb, 80, 240, 100, 255); // Phase B: Green
            lineRGBA(screen, prevXc, prevYc, px, yc, 80, 220, 255, 255); // Phase C: Cyan
        }
        prevXa = px; prevYa = ya;
        prevXb = px; prevYb = yb;
        prevXc = px; prevYc = yc;
    }
}

void CircuitGui::drawCursor() {
    int cx = (int)cursorX;
    int cy = (int)cursorY;

    static const int cursor_mask[11][8] = {
        {1, 0, 0, 0, 0, 0, 0, 0},
        {1, 1, 0, 0, 0, 0, 0, 0},
        {1, 2, 1, 0, 0, 0, 0, 0},
        {1, 2, 2, 1, 0, 0, 0, 0},
        {1, 2, 2, 2, 1, 0, 0, 0},
        {1, 2, 2, 2, 2, 1, 0, 0},
        {1, 2, 2, 1, 1, 1, 0, 0},
        {1, 2, 1, 2, 1, 0, 0, 0},
        {1, 1, 0, 1, 2, 1, 0, 0},
        {0, 0, 0, 0, 1, 2, 1, 0},
        {0, 0, 0, 0, 0, 1, 1, 0}
    };

    for (int r = 0; r < 11; ++r) {
        int py = cy + r;
        if (py < 0 || py >= 240) continue;
        for (int c = 0; c < 8; ++c) {
            int px = cx + c;
            if (px < 0 || px >= 320) continue;
            int val = cursor_mask[r][c];
            if (val == 1) {
                pixelRGBA(screen, px, py, 0, 0, 0, 255);
            } else if (val == 2) {
                pixelRGBA(screen, px, py, 220, 245, 255, 255);
            }
        }
    }

    // Live tooltip near cursor
    if (hoveredComponent && !isEditingValue && !isEditingOmega && cursorY < 214) {
        char line1[36], line2[52];
        char vBuf[24], iBuf[24], valBuf[32];
        formatSIValue(hoveredComponent->voltage, "V", vBuf, sizeof(vBuf));
        formatSIValue(hoveredComponent->current, "A", iBuf, sizeof(iBuf));
        hoveredComponent->formatValueString(valBuf, sizeof(valBuf), isPhasorMode, globalOmega);

        double vRms = hoveredComponent->getRMSVoltage();
        double iRms = hoveredComponent->getRMSCurrent();
        char vRmsBuf[24], iRmsBuf[24];
        formatSIValue(vRms, "V", vRmsBuf, sizeof(vRmsBuf));
        formatSIValue(iRms, "A", iRmsBuf, sizeof(iRmsBuf));

        bool isAC = (hoveredComponent->type == Component::COMP_AC_VOLTAGE_SRC) ||
                    (vRms > 0.05 && std::abs(hoveredComponent->voltage - hoveredComponent->peak_v) > 0.05 * hoveredComponent->peak_v);

        if (hoveredComponent->type == Component::COMP_WIRE) {
            snprintf(line1, sizeof(line1), "Wire");
            if (isAC) snprintf(line2, sizeof(line2), "Irms = %s", iRmsBuf);
            else snprintf(line2, sizeof(line2), "I = %s", iBuf);
        } else if (hoveredComponent->type == Component::COMP_GROUND) {
            snprintf(line1, sizeof(line1), "Ground");
            snprintf(line2, sizeof(line2), "0 V (Ref 0)");
        } else if (hoveredComponent->type == Component::COMP_AC_VOLTAGE_SRC) {
            snprintf(line1, sizeof(line1), "AC Vsrc (%s)", valBuf);
            snprintf(line2, sizeof(line2), "Vrms=%s  Irms=%s", vRmsBuf, iRmsBuf);
        } else if (hoveredComponent->type == Component::COMP_VOLTAGE_SRC) {
            snprintf(line1, sizeof(line1), "DC Vsrc (%s)", valBuf);
            snprintf(line2, sizeof(line2), "I = %s", iBuf);
        } else if (hoveredComponent->type == Component::COMP_IMPEDANCE) {
            snprintf(line1, sizeof(line1), "Z (%s)", valBuf);
            snprintf(line2, sizeof(line2), "Vrms=%s  Irms=%s", vRmsBuf, iRmsBuf);
        } else {
            snprintf(line1, sizeof(line1), "%s (%s)", hoveredComponent->getTypeName(), valBuf);
            if (isAC) {
                snprintf(line2, sizeof(line2), "Vrms=%s  Irms=%s", vRmsBuf, iRmsBuf);
            } else {
                snprintf(line2, sizeof(line2), "V=%s  I=%s", vBuf, iBuf);
            }
        }

        int w1 = nSDL_GetStringWidth(fontYellow, line1);
        int w2 = nSDL_GetStringWidth(fontWhite, line2);
        int tw = std::max(w1, w2) + 8;
        int th = 24;

        int tx = cx + 12;
        int ty = cy - 26;
        if (tx + tw > 316) tx = cx - tw - 4;
        if (ty < 22) ty = cy + 14;

        boxRGBA(screen, tx, ty, tx + tw, ty + th, 12, 18, 28, 240);
        rectangleRGBA(screen, tx, ty, tx + tw, ty + th, 255, 215, 0, 220);
        nSDL_DrawString(screen, fontYellow, tx + 4, ty + 2, "%s", line1);
        nSDL_DrawString(screen, fontWhite, tx + 4, ty + 13, "%s", line2);
    }
}

void CircuitGui::drawEditDialog() {
    if (!isEditingValue && !isEditingOmega) return;

    int boxW = 220;
    int boxH = 70;
    int boxX = (320 - boxW) / 2;
    int boxY = (240 - boxH) / 2;

    boxRGBA(screen, boxX, boxY, boxX + boxW, boxY + boxH, 18, 24, 36, 245);
    rectangleRGBA(screen, boxX, boxY, boxX + boxW, boxY + boxH, 255, 215, 0, 255);

    if (isEditingOmega) {
        nSDL_DrawString(screen, fontYellow, boxX + 10, boxY + 8, "Set Frequency w (rad/s)");
        boxRGBA(screen, boxX + 10, boxY + 26, boxX + boxW - 10, boxY + 46, 10, 14, 20, 255);
        rectangleRGBA(screen, boxX + 10, boxY + 26, boxX + boxW - 10, boxY + 46, 80, 140, 200, 255);
        nSDL_DrawString(screen, fontWhite, boxX + 16, boxY + 31, "%s_ rad/s", editBuffer);
        nSDL_DrawString(screen, fontCyan, boxX + 10, boxY + 54, "[Enter] OK  (0 = Symbolic)");
    } else if (editingComponent) {
        nSDL_DrawString(screen, fontYellow, boxX + 10, boxY + 8, "Edit %s", editingComponent->getTypeName());
        boxRGBA(screen, boxX + 10, boxY + 26, boxX + boxW - 10, boxY + 46, 10, 14, 20, 255);
        rectangleRGBA(screen, boxX + 10, boxY + 26, boxX + boxW - 10, boxY + 46, 80, 140, 200, 255);
        nSDL_DrawString(screen, fontWhite, boxX + 16, boxY + 31, "%s_ %s", editBuffer, editingComponent->getUnit());
        nSDL_DrawString(screen, fontCyan, boxX + 10, boxY + 54, "[Enter] OK  [Esc] Cancel  [+/-] x10");
    }
}

void CircuitGui::startEditingComponent(Component* comp) {
    if (!comp) return;
    editingComponent = comp;
    isEditingValue = true;
    isEditingOmega = false;
    snprintf(editBuffer, sizeof(editBuffer), "%g", comp->value);
    editBufferLen = strlen(editBuffer);
}

void CircuitGui::finishEditingComponent(bool apply) {
    if (apply && editingComponent && editBufferLen > 0) {
        char* endPtr = nullptr;
        double parsed = strtod(editBuffer, &endPtr);
        if (parsed > 0.0 || (editingComponent->type == Component::COMP_VOLTAGE_SRC) ||
            (editingComponent->type == Component::COMP_AC_VOLTAGE_SRC) ||
            (editingComponent->type == Component::COMP_CURRENT_SRC)) {
            editingComponent->value = parsed;
            topologyChanged = true;
        }
    }
    isEditingValue = false;
    editingComponent = nullptr;
    editBuffer[0] = '\0';
    editBufferLen = 0;
}

void CircuitGui::startEditingOmega() {
    isEditingOmega = true;
    isEditingValue = false;
    snprintf(editBuffer, sizeof(editBuffer), "%g", globalOmega);
    editBufferLen = strlen(editBuffer);
}

void CircuitGui::finishEditingOmega(bool apply) {
    if (apply && editBufferLen > 0) {
        char* endPtr = nullptr;
        double parsed = strtod(editBuffer, &endPtr);
        if (parsed >= 0.0) {
            globalOmega = parsed;
        }
    }
    isEditingOmega = false;
    editBuffer[0] = '\0';
    editBufferLen = 0;
}

void CircuitGui::handleCanvasClick() {
    int px = (int)cursorX;
    int py = (int)cursorY;

    // Check if clicked in bottom toolbar
    if (py >= 218) {
        int btnWidth = 21;
        int startX = 2;
        int clickedIdx = (px - startX) / (btnWidth + 1);

        if (clickedIdx >= 0 && clickedIdx < NUM_TOOLS) {
            activeTool = clickedIdx;
            anchorDotCol = -1;
            anchorDotRow = -1;
        } else if (clickedIdx == NUM_TOOLS) {
            // Play/Pause
            isRunning = !isRunning;
        } else if (clickedIdx == NUM_TOOLS + 1) {
            // Clear
            clearCircuit();
        } else if (clickedIdx == NUM_TOOLS + 2) {
            // Phasor toggle
            togglePhasorMode();
        }
        return;
    }

    if (isEditingValue) {
        finishEditingComponent(true);
        return;
    }

    if (isEditingOmega) {
        finishEditingOmega(true);
        return;
    }

    if (activeTool == TOOL_DELETE) {
        if (hoveredComponent) {
            auto it = std::find(components.begin(), components.end(), hoveredComponent);
            if (it != components.end()) {
                delete *it;
                components.erase(it);
                hoveredComponent = nullptr;
                topologyChanged = true;
            }
        }
        return;
    }

    if (activeTool == TOOL_EDIT) {
        if (hoveredComponent) {
            startEditingComponent(hoveredComponent);
        }
        return;
    }

    if (activeTool == TOOL_GROUND) {
        if (hoveredDotCol >= 0 && hoveredDotRow >= 0) {
            components.push_back(new Ground(hoveredDotCol, hoveredDotRow));
            topologyChanged = true;
        }
        return;
    }

    // Two-terminal component placement
    if (hoveredDotCol >= 0 && hoveredDotRow >= 0) {
        if (anchorDotCol < 0 || anchorDotRow < 0) {
            anchorDotCol = hoveredDotCol;
            anchorDotRow = hoveredDotRow;
        } else {
            if (anchorDotCol != hoveredDotCol || anchorDotRow != hoveredDotRow) {
                Component* newComp = nullptr;
                switch (activeTool) {
                case TOOL_WIRE:
                    newComp = new Wire(anchorDotCol, anchorDotRow, hoveredDotCol, hoveredDotRow);
                    break;
                case TOOL_RESISTOR:
                    newComp = new Resistor(anchorDotCol, anchorDotRow, hoveredDotCol, hoveredDotRow, 1000.0);
                    break;
                case TOOL_CAPACITOR:
                    newComp = new Capacitor(anchorDotCol, anchorDotRow, hoveredDotCol, hoveredDotRow, 10e-6);
                    break;
                case TOOL_INDUCTOR:
                    newComp = new Inductor(anchorDotCol, anchorDotRow, hoveredDotCol, hoveredDotRow, 10e-3);
                    break;
                case TOOL_VOLTAGE_SRC:
                    newComp = new VoltageSource(anchorDotCol, anchorDotRow, hoveredDotCol, hoveredDotRow, 5.0);
                    break;
                case TOOL_AC_VOLTAGE_SRC:
                    newComp = new ACVoltageSource(anchorDotCol, anchorDotRow, hoveredDotCol, hoveredDotRow, 120.0, 60.0, 0.0);
                    break;
                case TOOL_CURRENT_SRC:
                    newComp = new CurrentSource(anchorDotCol, anchorDotRow, hoveredDotCol, hoveredDotRow, 0.010);
                    break;
                case TOOL_IMPEDANCE:
                    newComp = new GenericImpedance(anchorDotCol, anchorDotRow, hoveredDotCol, hoveredDotRow, 10.0, 20.0);
                    break;
                default:
                    break;
                }
                if (newComp) {
                    components.push_back(newComp);
                    topologyChanged = true;
                }
            }
            anchorDotCol = -1;
            anchorDotRow = -1;
        }
    } else {
        anchorDotCol = -1;
        anchorDotRow = -1;
    }
}

void CircuitGui::handleKey(SDLKey key) {
    if (isEditingValue || isEditingOmega) {
        if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
            if (isEditingOmega) finishEditingOmega(true);
            else finishEditingComponent(true);
            return;
        }
        if (key == SDLK_ESCAPE) {
            if (isEditingOmega) finishEditingOmega(false);
            else finishEditingComponent(false);
            return;
        }
        if (key == SDLK_BACKSPACE) {
            if (editBufferLen > 0) {
                editBuffer[--editBufferLen] = '\0';
            }
            return;
        }
        if (key == SDLK_PLUS || key == SDLK_KP_PLUS) {
            double v = strtod(editBuffer, nullptr);
            v *= 10.0;
            snprintf(editBuffer, sizeof(editBuffer), "%g", v);
            editBufferLen = strlen(editBuffer);
            return;
        }
        if (key == SDLK_MINUS || key == SDLK_KP_MINUS) {
            double v = strtod(editBuffer, nullptr);
            v /= 10.0;
            snprintf(editBuffer, sizeof(editBuffer), "%g", v);
            editBufferLen = strlen(editBuffer);
            return;
        }

        char ch = '\0';
        if (key >= SDLK_0 && key <= SDLK_9) ch = '0' + (key - SDLK_0);
        else if (key >= SDLK_KP0 && key <= SDLK_KP9) ch = '0' + (key - SDLK_KP0);
        else if (key == SDLK_PERIOD || key == SDLK_KP_PERIOD) ch = '.';
        else if (key == SDLK_e) ch = 'e';

        if (ch != '\0' && editBufferLen < (int)sizeof(editBuffer) - 2) {
            editBuffer[editBufferLen++] = ch;
            editBuffer[editBufferLen] = '\0';
        }
        return;
    }

    switch (key) {
    case SDLK_ESCAPE:
        quitRequested = true;
        break;
    case SDLK_w:
        activeTool = TOOL_WIRE;
        anchorDotCol = -1; anchorDotRow = -1;
        break;
    case SDLK_r:
        activeTool = TOOL_RESISTOR;
        anchorDotCol = -1; anchorDotRow = -1;
        break;
    case SDLK_c:
        activeTool = TOOL_CAPACITOR;
        anchorDotCol = -1; anchorDotRow = -1;
        break;
    case SDLK_l:
        activeTool = TOOL_INDUCTOR;
        anchorDotCol = -1; anchorDotRow = -1;
        break;
    case SDLK_v:
        activeTool = TOOL_VOLTAGE_SRC;
        anchorDotCol = -1; anchorDotRow = -1;
        break;
    case SDLK_a:
        activeTool = TOOL_AC_VOLTAGE_SRC;
        anchorDotCol = -1; anchorDotRow = -1;
        break;
    case SDLK_i:
        activeTool = TOOL_CURRENT_SRC;
        anchorDotCol = -1; anchorDotRow = -1;
        break;
    case SDLK_z:
        activeTool = TOOL_IMPEDANCE;
        anchorDotCol = -1; anchorDotRow = -1;
        break;
    case SDLK_g:
        activeTool = TOOL_GROUND;
        anchorDotCol = -1; anchorDotRow = -1;
        break;
    case SDLK_y:
        addWyeGeneratorMacro();
        break;
    case SDLK_d:
        addDeltaGeneratorMacro();
        break;
    case SDLK_s:
        toggleScope();
        break;
    case SDLK_f:
        togglePhasorMode();
        break;
    case SDLK_o:
        startEditingOmega();
        break;
    case SDLK_DELETE:
    case SDLK_BACKSPACE:
        if (hoveredComponent) {
            auto it = std::find(components.begin(), components.end(), hoveredComponent);
            if (it != components.end()) {
                delete *it;
                components.erase(it);
                hoveredComponent = nullptr;
                topologyChanged = true;
            }
        } else {
            activeTool = TOOL_DELETE;
            anchorDotCol = -1; anchorDotRow = -1;
        }
        break;
    case SDLK_e:
        if (hoveredComponent) {
            startEditingComponent(hoveredComponent);
        } else {
            activeTool = TOOL_EDIT;
        }
        break;
    case SDLK_SPACE:
    case SDLK_p:
        isRunning = !isRunning;
        break;
    case SDLK_1:
        loadPresetRC();
        break;
    case SDLK_2:
        loadPresetRLC();
        break;
    case SDLK_3:
        loadPresetBridge();
        break;
    case SDLK_0:
        clearCircuit();
        break;
    default:
        break;
    }
}

void CircuitGui::handleInputs() {
    touchpad_report_t report;
    if (touchpad_scan(&report) == 0 && report.contact) {
        if (wasTouching) {
            int dx = (int)report.x - (int)prevTouchX;
            int dy = (int)report.y - (int)prevTouchY;
            // Halved sensitivity as requested
            cursorX += (double)dx * 0.175;
            cursorY -= (double)dy * 0.175;
        }
        prevTouchX = report.x;
        prevTouchY = report.y;
        wasTouching = true;
    } else {
        wasTouching = false;
    }

    // Halved arrow key speed
    double cursorSpeed = 1.5;
    if (isKeyPressed(KEY_NSPIRE_SHIFT)) {
        cursorSpeed = 3.5;
    }

    if (isKeyPressed(KEY_NSPIRE_LEFT))  cursorX -= cursorSpeed;
    if (isKeyPressed(KEY_NSPIRE_RIGHT)) cursorX += cursorSpeed;
    if (isKeyPressed(KEY_NSPIRE_UP))    cursorY -= cursorSpeed;
    if (isKeyPressed(KEY_NSPIRE_DOWN))  cursorY += cursorSpeed;

    if (cursorX < 2.0)   cursorX = 2.0;
    if (cursorX > 317.0) cursorX = 317.0;
    if (cursorY < 2.0)   cursorY = 2.0;
    if (cursorY > 237.0) cursorY = 237.0;

    bool currentClickState = (isKeyPressed(KEY_NSPIRE_CLICK) ||
                              isKeyPressed(KEY_NSPIRE_ENTER) ||
                              (wasTouching && report.pressed));
    if (currentClickState && !prevClickState) {
        handleCanvasClick();
    }
    prevClickState = currentClickState;

    // Delete hovered component directly with calculator DEL key (edge-triggered, no while loop)
    bool currentDel = isKeyPressed(KEY_NSPIRE_DEL);
    if (currentDel && !prevDelPressed) {
        if (hoveredComponent) {
            auto it = std::find(components.begin(), components.end(), hoveredComponent);
            if (it != components.end()) {
                delete *it;
                components.erase(it);
                hoveredComponent = nullptr;
                topologyChanged = true;
            }
        } else {
            activeTool = TOOL_DELETE;
            anchorDotCol = -1; anchorDotRow = -1;
        }
    }
    prevDelPressed = currentDel;

    // Escape handling (edge-triggered, no while loop)
    bool currentEsc = isKeyPressed(KEY_NSPIRE_ESC);
    if (currentEsc && !prevEscPressed) {
        if (isEditingValue) {
            finishEditingComponent(false);
        } else if (isEditingOmega) {
            finishEditingOmega(false);
        } else if (isScopeOpen) {
            isScopeOpen = false;
        } else {
            quitRequested = true;
        }
    }
    prevEscPressed = currentEsc;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            quitRequested = true;
            break;
        case SDL_KEYDOWN:
            handleKey(event.key.keysym.sym);
            break;
        case SDL_MOUSEMOTION:
            cursorX = event.motion.x;
            cursorY = event.motion.y;
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_LEFT) {
                cursorX = event.button.x;
                cursorY = event.button.y;
                handleCanvasClick();
            }
            break;
        default:
            break;
        }
    }

    hoveredDotCol = getClosestDotCol((int)cursorX);
    hoveredDotRow = getClosestDotRow((int)cursorY);
    hoveredComponent = getComponentAt((int)cursorX, (int)cursorY);
}

void CircuitGui::run() {
    Uint32 lastTime = SDL_GetTicks();

    while (!quitRequested) {
        handleInputs();
        simulationStep();

        drawGrid();
        drawComponents();
        drawTopBar();
        drawBottomToolbar();
        drawScope();
        drawCursor();
        drawEditDialog();

        SDL_Flip(screen);

        Uint32 now = SDL_GetTicks();
        Uint32 elapsed = now - lastTime;
        if (elapsed < 25) {
            SDL_Delay(25 - elapsed);
        }
        lastTime = SDL_GetTicks();
    }
}