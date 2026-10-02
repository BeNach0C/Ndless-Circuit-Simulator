#include "CircuitGui.h"
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <algorithm>

CircuitGui::CircuitGui()
    : topologyChanged(true),
      screen(nullptr), fontWhite(nullptr), fontYellow(nullptr),
      fontCyan(nullptr), fontGreen(nullptr),
      cursorX(160.0), cursorY(118.0),
      hoveredDotCol(-1), hoveredDotRow(-1),
      anchorDotCol(-1), anchorDotRow(-1),
      hoveredComponent(nullptr),
      activeTool(TOOL_WIRE), isRunning(true), quitRequested(false),
      isEditingValue(false), editingComponent(nullptr), editBufferLen(0),
      prevTouchX(0), prevTouchY(0), wasTouching(false), prevClickState(false) {
    editBuffer[0] = '\0';
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
    topologyChanged = true;
    mna.resize(0, 0);
    engine.reset();
}

void CircuitGui::loadPresetRC() {
    clearCircuit();
    // 5V Voltage Source at col 3, rows 2 to 6
    components.push_back(new VoltageSource(3, 6, 3, 2, 5.0));
    // Resistor 1k from col 3, row 2 to col 7, row 2
    components.push_back(new Resistor(3, 2, 7, 2, 1000.0));
    // Capacitor 10uF from col 7, row 2 to col 7, row 6
    components.push_back(new Capacitor(7, 2, 7, 6, 10e-6));
    // Ground rail wire from col 3, row 6 to col 7, row 6
    components.push_back(new Wire(3, 6, 7, 6));
    // Ground symbol at col 5, row 6
    components.push_back(new Ground(5, 6));
    topologyChanged = true;
}

void CircuitGui::loadPresetRLC() {
    clearCircuit();
    // 5V Voltage Source at col 2, rows 2 to 6
    components.push_back(new VoltageSource(2, 6, 2, 2, 5.0));
    // Resistor 100 Ohm from col 2, row 2 to col 5, row 2
    components.push_back(new Resistor(2, 2, 5, 2, 100.0));
    // Inductor 10mH from col 5, row 2 to col 8, row 2
    components.push_back(new Inductor(5, 2, 8, 2, 10e-3));
    // Capacitor 10uF from col 8, row 2 to col 8, row 6
    components.push_back(new Capacitor(8, 2, 8, 6, 10e-6));
    // Ground rail wire from col 2, row 6 to col 8, row 6
    components.push_back(new Wire(2, 6, 8, 6));
    // Ground symbol at col 5, row 6
    components.push_back(new Ground(5, 6));
    topologyChanged = true;
}

void CircuitGui::loadPresetBridge() {
    clearCircuit();
    // 10V Voltage Source at col 2, rows 2 to 7
    components.push_back(new VoltageSource(2, 7, 2, 2, 10.0));
    // Wire top rail: col 2, row 2 to col 6, row 2
    components.push_back(new Wire(2, 2, 6, 2));
    // Left branch: R1 (1k) from (6,2) to (4,4.5->4,5)
    components.push_back(new Resistor(6, 2, 4, 4, 1000.0));
    components.push_back(new Resistor(4, 4, 6, 7, 2000.0));
    // Right branch: R2 (1k) from (6,2) to (8,4)
    components.push_back(new Resistor(6, 2, 8, 4, 1000.0));
    components.push_back(new Resistor(8, 4, 6, 7, 1000.0));
    // Bridge resistor across (4,4) and (8,4)
    components.push_back(new Resistor(4, 4, 8, 4, 500.0));
    // Bottom ground wire: col 2, row 7 to col 6, row 7
    components.push_back(new Wire(2, 7, 6, 7));
    // Ground at (6, 7)
    components.push_back(new Ground(6, 7));
    topologyChanged = true;
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
    double minDistance = 8.0; // hit radius in pixels

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

        // Distance from point (px, py) to line segment (x1, y1)-(x2, y2)
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

    // Union dots connected by wires
    for (size_t i = 0; i < components.size(); ++i) {
        Component* c = components[i];
        if (c->type == Component::COMP_WIRE) {
            int d1 = c->dot1_y * GRID_COLS + c->dot1_x;
            int d2 = c->dot2_y * GRID_COLS + c->dot2_x;
            unite(d1, d2);
        }
    }

    // Identify Ground nets
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

    // If no ground symbol exists, pick terminal 2 of the first component as reference Ground
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

    // Number active nets
    std::vector<int> rootToNode(totalDots, -1);
    if (hasExplicitGround) {
        for (int i = 0; i < totalDots; ++i) {
            if (isGndRoot[i]) rootToNode[i] = 0; // Ground is Node 0
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

    // Assign electrical nodes and auxiliary indices
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

        if (c->type == Component::COMP_VOLTAGE_SRC) {
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

    // Run 2 discrete solver steps per display frame for smoother integration
    for (int step = 0; step < 2; ++step) {
        mna.clear();
        for (size_t i = 0; i < components.size(); ++i) {
            components[i]->stamp(mna, engine.dt);
        }

        if (mna.solve()) {
            for (size_t i = 0; i < components.size(); ++i) {
                components[i]->updateState(mna, engine.dt);
            }
            computeWireCurrents();
            engine.stepTime();
        }
    }
}

void CircuitGui::computeWireCurrents() {
    const int totalDots = GRID_COLS * GRID_ROWS;
    std::vector<double> inj(totalDots, 0.0);

    // 1. Calculate injected current at each dot from all non-wire components
    for (size_t i = 0; i < components.size(); ++i) {
        Component* c = components[i];
        if (c->type == Component::COMP_WIRE || c->type == Component::COMP_GROUND) {
            continue;
        }

        int d1 = c->dot1_y * GRID_COLS + c->dot1_x;
        int d2 = c->dot2_y * GRID_COLS + c->dot2_x;

        if (c->type == Component::COMP_VOLTAGE_SRC) {
            // Aux current flows out of terminal 1 into d1, and into terminal 2 from d2
            inj[d1] += c->current;
            inj[d2] -= c->current;
        } else {
            // Passive components & current sources: current flows from terminal 1 into terminal 2
            inj[d1] -= c->current;
            inj[d2] += c->current;
        }
    }

    // 2. Identify wires and build graph
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

    int numWires = (int)wireIndices.size();
    if (numWires == 0) return;

    std::vector<bool> wireResolved(numWires, false);

    // Identify which dots have Ground components
    std::vector<bool> isGndDot(totalDots, false);
    for (size_t i = 0; i < components.size(); ++i) {
        if (components[i]->type == Component::COMP_GROUND) {
            int d = components[i]->dot1_y * GRID_COLS + components[i]->dot1_x;
            isGndDot[d] = true;
        }
    }

    // Find leaves (degree == 1, non-ground dots first)
    std::vector<int> leaves;
    for (int i = 0; i < totalDots; ++i) {
        if (degree[i] == 1 && !isGndDot[i]) {
            leaves.push_back(i);
        }
    }
    for (int i = 0; i < totalDots; ++i) {
        if (degree[i] == 1 && isGndDot[i]) {
            leaves.push_back(i);
        }
    }

    size_t head = 0;
    while (head < leaves.size()) {
        int u = leaves[head++];
        if (degree[u] == 0) continue;

        int wireIdx = -1;
        for (int wIdx : adj[u]) {
            if (!wireResolved[wIdx]) {
                wireIdx = wIdx;
                break;
            }
        }
        if (wireIdx == -1) continue;

        Wire* w = static_cast<Wire*>(components[wireIndices[wireIdx]]);
        int d1 = w->dot1_y * GRID_COLS + w->dot1_x;
        int d2 = w->dot2_y * GRID_COLS + w->dot2_x;
        int v = (d1 == u) ? d2 : d1;

        if (d1 == u) {
            w->current = inj[u];
        } else {
            w->current = -inj[u];
        }
        wireResolved[wireIdx] = true;

        inj[v] += inj[u];
        degree[u]--;
        degree[v]--;

        if (degree[v] == 1) {
            leaves.push_back(v);
        }
    }
}

void CircuitGui::drawGrid() {
    // Draw background
    SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 14, 20, 28));

    // Draw snap dots
    Uint32 dotColor = SDL_MapRGB(screen->format, 55, 75, 95);
    for (int r = 0; r < GRID_ROWS; ++r) {
        int py = getDotPixelY(r);
        for (int c = 0; c < GRID_COLS; ++c) {
            int px = getDotPixelX(c);
            // Draw 2x2 dot
            SDL_Rect rect = { (Sint16)px, (Sint16)py, 2, 2 };
            SDL_FillRect(screen, &rect, dotColor);
        }
    }

    // Highlight hovered dot
    if (hoveredDotCol >= 0 && hoveredDotRow >= 0) {
        int hx = getDotPixelX(hoveredDotCol);
        int hy = getDotPixelY(hoveredDotRow);
        circleRGBA(screen, hx, hy, 4, 80, 220, 255, 200);
    }

    // Highlight anchor dot and rubberband preview
    if (anchorDotCol >= 0 && anchorDotRow >= 0) {
        int ax = getDotPixelX(anchorDotCol);
        int ay = getDotPixelY(anchorDotRow);
        circleRGBA(screen, ax, ay, 5, 255, 215, 0, 255);

        int targetX = (hoveredDotCol >= 0) ? getDotPixelX(hoveredDotCol) : (int)cursorX;
        int targetY = (hoveredDotRow >= 0) ? getDotPixelY(hoveredDotRow) : (int)cursorY;
        lineRGBA(screen, ax, ay, targetX, targetY, 255, 215, 0, 160);
    }
}

void CircuitGui::drawComponent(Component* comp) {
    int x1 = getDotPixelX(comp->dot1_x);
    int y1 = getDotPixelY(comp->dot1_y);
    bool isHovered = (comp == hoveredComponent);

    if (comp->type == Component::COMP_GROUND) {
        // Ground lead and 3 decreasing plates
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
    comp->formatValueString(valStr, sizeof(valStr));

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

        // Leads
        int pax = (int)(x1 + 0.25 * len * ux);
        int pay = (int)(y1 + 0.25 * len * uy);
        int pbx = (int)(x1 + 0.75 * len * ux);
        int pby = (int)(y1 + 0.75 * len * uy);

        lineRGBA(screen, x1, y1, pax, pay, r, g, b, 255);
        lineRGBA(screen, pbx, pby, x2, y2, r, g, b, 255);

        // 6 Zigzag segments
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

        // Draw label
        int tx = (int)(x1 + 0.5 * len * ux + 8.0 * nx);
        int ty = (int)(y1 + 0.5 * len * uy + 8.0 * ny - 4);
        nSDL_DrawString(screen, fontWhite, tx, ty, "%s", valStr);
        break;
    }
    case Component::COMP_CAPACITOR: {
        Uint8 r = isHovered ? 255 : 80;
        Uint8 g = isHovered ? 230 : 240;
        Uint8 b = isHovered ? 50  : 120;

        int midX = (int)(x1 + 0.5 * len * ux);
        int midY = (int)(y1 + 0.5 * len * uy);
        int p1x = (int)(midX - 3.0 * ux);
        int p1y = (int)(midY - 3.0 * uy);
        int p2x = (int)(midX + 3.0 * ux);
        int p2y = (int)(midY + 3.0 * uy);

        lineRGBA(screen, x1, y1, p1x, p1y, r, g, b, 255);
        lineRGBA(screen, p2x, p2y, x2, y2, r, g, b, 255);

        // Plate 1
        lineRGBA(screen, (int)(p1x - 7.0 * nx), (int)(p1y - 7.0 * ny),
                         (int)(p1x + 7.0 * nx), (int)(p1y + 7.0 * ny), r, g, b, 255);
        // Plate 2
        lineRGBA(screen, (int)(p2x - 7.0 * nx), (int)(p2y - 7.0 * ny),
                         (int)(p2x + 7.0 * nx), (int)(p2y + 7.0 * ny), r, g, b, 255);

        int tx = (int)(midX + 9.0 * nx);
        int ty = (int)(midY + 9.0 * ny - 4);
        nSDL_DrawString(screen, fontWhite, tx, ty, "%s", valStr);
        break;
    }
    case Component::COMP_INDUCTOR: {
        Uint8 r = isHovered ? 255 : 220;
        Uint8 g = isHovered ? 230 : 100;
        Uint8 b = isHovered ? 50  : 255;

        int pax = (int)(x1 + 0.15 * len * ux);
        int pay = (int)(y1 + 0.15 * len * uy);
        int pbx = (int)(x1 + 0.85 * len * ux);
        int pby = (int)(y1 + 0.85 * len * uy);

        lineRGBA(screen, x1, y1, pax, pay, r, g, b, 255);
        lineRGBA(screen, pbx, pby, x2, y2, r, g, b, 255);

        // Coiled inductor loops (consecutive smooth rounded semicircles)
        int numLoops = (len > 30.0) ? 4 : 3;
        double bodyLen = 0.7 * len;
        double loopW = bodyLen / numLoops;
        double loopH = 6.0;

        for (int i = 0; i < numLoops; ++i) {
            double cDist = 0.15 * len + (i + 0.5) * loopW;
            double cx = x1 + cDist * ux;
            double cy = y1 + cDist * uy;

            const int STEPS = 8;
            int prevPtX = 0, prevPtY = 0;

            for (int s = 0; s <= STEPS; ++s) {
                double theta = (double)s * (3.141592653589793 / (double)STEPS);
                double cosT = std::cos(theta);
                double sinT = std::sin(theta);

                // Start on lead axis (theta=0), arch out perpendicular to axis (theta=pi/2), return to axis (theta=pi)
                int px = (int)std::round(cx - (loopW * 0.5 * cosT) * ux - (loopH * sinT) * nx);
                int py = (int)std::round(cy - (loopW * 0.5 * cosT) * uy - (loopH * sinT) * ny);

                if (s > 0) {
                    lineRGBA(screen, prevPtX, prevPtY, px, py, r, g, b, 255);
                }
                prevPtX = px;
                prevPtY = py;
            }
        }

        int tx = (int)(x1 + 0.5 * len * ux + 8.0 * nx);
        int ty = (int)(y1 + 0.5 * len * uy + 8.0 * ny - 4);
        nSDL_DrawString(screen, fontWhite, tx, ty, "%s", valStr);
        break;
    }
    case Component::COMP_VOLTAGE_SRC: {
        Uint8 r = isHovered ? 255 : 80;
        Uint8 g = isHovered ? 230 : 170;
        Uint8 b = isHovered ? 50  : 255;

        int midX = (int)(x1 + 0.5 * len * ux);
        int midY = (int)(y1 + 0.5 * len * uy);
        int p1x = (int)(midX - 9.0 * ux);
        int p1y = (int)(midY - 9.0 * uy);
        int p2x = (int)(midX + 9.0 * ux);
        int p2y = (int)(midY + 9.0 * uy);

        lineRGBA(screen, x1, y1, p1x, p1y, r, g, b, 255);
        lineRGBA(screen, p2x, p2y, x2, y2, r, g, b, 255);

        circleRGBA(screen, midX, midY, 9, r, g, b, 255);

        // '+' near terminal 1, '-' near terminal 2
        int plusX = (int)(midX - 4.0 * ux);
        int plusY = (int)(midY - 4.0 * uy);
        lineRGBA(screen, plusX - 2, plusY, plusX + 2, plusY, r, g, b, 255);
        lineRGBA(screen, plusX, plusY - 2, plusX, plusY + 2, r, g, b, 255);

        int minusX = (int)(midX + 4.0 * ux);
        int minusY = (int)(midY + 4.0 * uy);
        lineRGBA(screen, minusX - 2, minusY, minusX + 2, minusY, r, g, b, 255);

        int tx = (int)(midX + 11.0 * nx);
        int ty = (int)(midY + 11.0 * ny - 4);
        nSDL_DrawString(screen, fontWhite, tx, ty, "%s", valStr);
        break;
    }
    case Component::COMP_CURRENT_SRC: {
        Uint8 r = isHovered ? 255 : 255;
        Uint8 g = isHovered ? 230 : 130;
        Uint8 b = isHovered ? 50  : 90;

        int midX = (int)(x1 + 0.5 * len * ux);
        int midY = (int)(y1 + 0.5 * len * uy);
        int p1x = (int)(midX - 9.0 * ux);
        int p1y = (int)(midY - 9.0 * uy);
        int p2x = (int)(midX + 9.0 * ux);
        int p2y = (int)(midY + 9.0 * uy);

        lineRGBA(screen, x1, y1, p1x, p1y, r, g, b, 255);
        lineRGBA(screen, p2x, p2y, x2, y2, r, g, b, 255);

        circleRGBA(screen, midX, midY, 9, r, g, b, 255);

        // Arrow from terminal 1 towards terminal 2 inside circle
        int arrStartX = (int)(midX - 4.0 * ux);
        int arrStartY = (int)(midY - 4.0 * uy);
        int arrEndX   = (int)(midX + 4.0 * ux);
        int arrEndY   = (int)(midY + 4.0 * uy);
        lineRGBA(screen, arrStartX, arrStartY, arrEndX, arrEndY, r, g, b, 255);
        lineRGBA(screen, arrEndX, arrEndY, (int)(arrEndX - 3.0 * ux + 2.0 * nx), (int)(arrEndY - 3.0 * uy + 2.0 * ny), r, g, b, 255);
        lineRGBA(screen, arrEndX, arrEndY, (int)(arrEndX - 3.0 * ux - 2.0 * nx), (int)(arrEndY - 3.0 * uy - 2.0 * ny), r, g, b, 255);

        int tx = (int)(midX + 11.0 * nx);
        int ty = (int)(midY + 11.0 * ny - 4);
        nSDL_DrawString(screen, fontWhite, tx, ty, "%s", valStr);
        break;
    }
    default:
        break;
    }
}

void CircuitGui::drawComponents() {
    for (size_t i = 0; i < components.size(); ++i) {
        drawComponent(components[i]);
    }
}

void CircuitGui::drawTopBar() {
    // Top bar background
    boxRGBA(screen, 0, 0, 319, 19, 20, 28, 40, 255);
    lineRGBA(screen, 0, 19, 319, 19, 45, 65, 88, 255);

    // Simulator state tag on the far left
    if (isRunning) {
        nSDL_DrawString(screen, fontGreen, 4, 4, "[RUN]");
    } else {
        nSDL_DrawString(screen, fontYellow, 4, 4, "[PAUSE]");
    }

    // Hover readout has maximum width starting at x=46
    if (hoveredComponent) {
        char infoBuf[64];
        hoveredComponent->formatInfoString(infoBuf, sizeof(infoBuf));
        nSDL_DrawString(screen, fontYellow, 46, 4, "%s", infoBuf);
    } else if (hoveredDotCol >= 0 && hoveredDotRow >= 0) {
        double v = 0.0;
        bool found = false;
        int nodeId = -1;
        for (size_t i = 0; i < components.size(); ++i) {
            Component* c = components[i];
            if (c->dot1_x == hoveredDotCol && c->dot1_y == hoveredDotRow) {
                nodeId = c->node1; found = true; break;
            }
            if (c->type != Component::COMP_GROUND && c->dot2_x == hoveredDotCol && c->dot2_y == hoveredDotRow) {
                nodeId = c->node2; found = true; break;
            }
        }
        if (found) {
            v = mna.getNodeVoltage(nodeId);
            char vBuf[24];
            formatSIValue(v, "V", vBuf, sizeof(vBuf));
            if (nodeId == 0) {
                nSDL_DrawString(screen, fontCyan, 46, 4, "Ground (Ref Node 0): 0 V");
            } else {
                nSDL_DrawString(screen, fontCyan, 46, 4, "Node %d Voltage: %s", nodeId, vBuf);
            }
        } else {
            char timeBuf[24];
            formatSIValue(engine.currentTime, "s", timeBuf, sizeof(timeBuf));
            nSDL_DrawString(screen, fontWhite, 46, 4, "Time: %s", timeBuf);
        }
    } else {
        // Nothing hovered: show title & time
        nSDL_DrawString(screen, fontCyan, 46, 4, "NaiveSim");
        char timeBuf[24];
        formatSIValue(engine.currentTime, "s", timeBuf, sizeof(timeBuf));
        nSDL_DrawString(screen, fontWhite, 110, 4, "t = %s", timeBuf);
        nSDL_DrawString(screen, fontCyan, 210, 4, "(Hover for V, I)");
    }
}

void CircuitGui::drawBottomToolbar() {
    boxRGBA(screen, 0, 218, 319, 239, 20, 28, 40, 255);
    lineRGBA(screen, 0, 218, 319, 218, 45, 65, 88, 255);

    const char* toolLabels[NUM_TOOLS] = {
        "W", "R", "C", "L", "V", "I", "G", "Del", "Edit"
    };

    int btnWidth = 28;
    int btnHeight = 18;
    int startX = 2;
    int startY = 220;

    for (int i = 0; i < NUM_TOOLS; ++i) {
        int bx = startX + i * (btnWidth + 1);
        bool isActive = (i == activeTool);

        if (isActive) {
            boxRGBA(screen, bx, startY, bx + btnWidth, startY + btnHeight, 40, 90, 160, 255);
            rectangleRGBA(screen, bx, startY, bx + btnWidth, startY + btnHeight, 255, 215, 0, 255);
            nSDL_DrawString(screen, fontYellow, bx + 5, startY + 4, "%s", toolLabels[i]);
        } else {
            boxRGBA(screen, bx, startY, bx + btnWidth, startY + btnHeight, 28, 38, 52, 255);
            rectangleRGBA(screen, bx, startY, bx + btnWidth, startY + btnHeight, 55, 75, 100, 255);
            nSDL_DrawString(screen, fontWhite, bx + 5, startY + 4, "%s", toolLabels[i]);
        }
    }

    // Play/Pause button
    int pbx = startX + NUM_TOOLS * (btnWidth + 1);
    boxRGBA(screen, pbx, startY, pbx + 26, startY + btnHeight, 28, 38, 52, 255);
    rectangleRGBA(screen, pbx, startY, pbx + 26, startY + btnHeight, 55, 75, 100, 255);
    nSDL_DrawString(screen, isRunning ? fontGreen : fontYellow, pbx + 5, startY + 4, "%s", isRunning ? "||" : ">");

    // Clear button
    int clrX = pbx + 28;
    boxRGBA(screen, clrX, startY, clrX + 26, startY + btnHeight, 28, 38, 52, 255);
    rectangleRGBA(screen, clrX, startY, clrX + 26, startY + btnHeight, 55, 75, 100, 255);
    nSDL_DrawString(screen, fontWhite, clrX + 4, startY + 4, "Clr");
}

void CircuitGui::drawCursor() {
    int cx = (int)cursorX;
    int cy = (int)cursorY;

    // High-contrast cursor sprite
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

    // Floating live tooltip near cursor for component or node voltage
    if (hoveredComponent && !isEditingValue && cursorY < 214) {
        char line1[32], line2[48];
        char vBuf[24], iBuf[24], valBuf[24];
        formatSIValue(hoveredComponent->voltage, "V", vBuf, sizeof(vBuf));
        formatSIValue(hoveredComponent->current, "A", iBuf, sizeof(iBuf));
        hoveredComponent->formatValueString(valBuf, sizeof(valBuf));

        if (hoveredComponent->type == Component::COMP_WIRE) {
            snprintf(line1, sizeof(line1), "Wire");
            snprintf(line2, sizeof(line2), "I = %s", iBuf);
        } else if (hoveredComponent->type == Component::COMP_GROUND) {
            snprintf(line1, sizeof(line1), "Ground");
            snprintf(line2, sizeof(line2), "0 V");
        } else if (hoveredComponent->type == Component::COMP_VOLTAGE_SRC) {
            snprintf(line1, sizeof(line1), "Vsrc (%s)", valBuf);
            snprintf(line2, sizeof(line2), "I = %s", iBuf);
        } else {
            snprintf(line1, sizeof(line1), "%s (%s)", hoveredComponent->getTypeName(), valBuf);
            snprintf(line2, sizeof(line2), "V=%s  I=%s", vBuf, iBuf);
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
    } else if (hoveredDotCol >= 0 && hoveredDotRow >= 0 && !isEditingValue && cursorY < 214) {
        int nodeId = -1;
        for (size_t i = 0; i < components.size(); ++i) {
            Component* c = components[i];
            if (c->dot1_x == hoveredDotCol && c->dot1_y == hoveredDotRow) {
                nodeId = c->node1; break;
            }
            if (c->type != Component::COMP_GROUND && c->dot2_x == hoveredDotCol && c->dot2_y == hoveredDotRow) {
                nodeId = c->node2; break;
            }
        }
        if (nodeId >= 0) {
            double v = mna.getNodeVoltage(nodeId);
            char vBuf[24];
            formatSIValue(v, "V", vBuf, sizeof(vBuf));
            char tipBuf[32];
            if (nodeId == 0) {
                snprintf(tipBuf, sizeof(tipBuf), "GND: 0 V");
            } else {
                snprintf(tipBuf, sizeof(tipBuf), "Node %d: %s", nodeId, vBuf);
            }
            int tw = nSDL_GetStringWidth(fontCyan, tipBuf) + 8;
            int tx = cx + 12;
            int ty = cy - 16;
            if (tx + tw > 316) tx = cx - tw - 4;
            if (ty < 22) ty = cy + 14;
            boxRGBA(screen, tx, ty, tx + tw, ty + 13, 12, 18, 28, 240);
            rectangleRGBA(screen, tx, ty, tx + tw, ty + 13, 80, 220, 255, 220);
            nSDL_DrawString(screen, fontCyan, tx + 4, ty + 2, "%s", tipBuf);
        }
    }
}

void CircuitGui::drawEditDialog() {
    if (!isEditingValue || !editingComponent) return;

    int boxW = 180;
    int boxH = 74;
    int boxX = (320 - boxW) / 2;
    int boxY = (240 - boxH) / 2;

    boxRGBA(screen, boxX, boxY, boxX + boxW, boxY + boxH, 22, 32, 46, 240);
    rectangleRGBA(screen, boxX, boxY, boxX + boxW, boxY + boxH, 255, 215, 0, 255);

    nSDL_DrawString(screen, fontYellow, boxX + 10, boxY + 8, "Edit %s", editingComponent->getTypeName());

    // Input box
    boxRGBA(screen, boxX + 10, boxY + 26, boxX + boxW - 10, boxY + 46, 12, 18, 26, 255);
    rectangleRGBA(screen, boxX + 10, boxY + 26, boxX + boxW - 10, boxY + 46, 80, 140, 200, 255);

    nSDL_DrawString(screen, fontWhite, boxX + 16, boxY + 31, "%s_ %s", editBuffer, editingComponent->getUnit());

    nSDL_DrawString(screen, fontCyan, boxX + 10, boxY + 54, "[Enter] OK  [Esc] Cancel  [+/-] x10");
}

void CircuitGui::startEditingComponent(Component* comp) {
    if (!comp) return;
    editingComponent = comp;
    isEditingValue = true;
    snprintf(editBuffer, sizeof(editBuffer), "%g", comp->value);
    editBufferLen = strlen(editBuffer);
}

void CircuitGui::finishEditingComponent(bool apply) {
    if (apply && editingComponent && editBufferLen > 0) {
        char* endPtr = nullptr;
        double parsed = strtod(editBuffer, &endPtr);
        if (parsed > 0.0 || (editingComponent->type == Component::COMP_VOLTAGE_SRC) || (editingComponent->type == Component::COMP_CURRENT_SRC)) {
            editingComponent->value = parsed;
            topologyChanged = true;
        }
    }
    isEditingValue = false;
    editingComponent = nullptr;
    editBuffer[0] = '\0';
    editBufferLen = 0;
}

void CircuitGui::handleCanvasClick() {
    int px = (int)cursorX;
    int py = (int)cursorY;

    // Check if clicked in bottom toolbar
    if (py >= 218) {
        int btnWidth = 28;
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
        }
        return;
    }

    // In edit dialog mode
    if (isEditingValue) {
        finishEditingComponent(true);
        return;
    }

    // Canvas click actions
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
            // Set first terminal
            anchorDotCol = hoveredDotCol;
            anchorDotRow = hoveredDotRow;
        } else {
            // Connect to second terminal if different
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
                case TOOL_CURRENT_SRC:
                    newComp = new CurrentSource(anchorDotCol, anchorDotRow, hoveredDotCol, hoveredDotRow, 0.010);
                    break;
                default:
                    break;
                }
                if (newComp) {
                    components.push_back(newComp);
                    topologyChanged = true;
                }
            }
            // Reset anchor
            anchorDotCol = -1;
            anchorDotRow = -1;
        }
    } else {
        // Clicked outside dots cancels anchor
        anchorDotCol = -1;
        anchorDotRow = -1;
    }
}

void CircuitGui::handleKey(SDLKey key) {
    if (isEditingValue) {
        if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
            finishEditingComponent(true);
            return;
        }
        if (key == SDLK_ESCAPE) {
            finishEditingComponent(false);
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

        // Numeric typing
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
    case SDLK_i:
        activeTool = TOOL_CURRENT_SRC;
        anchorDotCol = -1; anchorDotRow = -1;
        break;
    case SDLK_g:
        activeTool = TOOL_GROUND;
        anchorDotCol = -1; anchorDotRow = -1;
        break;
    case SDLK_d:
    case SDLK_DELETE:
        activeTool = TOOL_DELETE;
        anchorDotCol = -1; anchorDotRow = -1;
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
    // 1. Touchpad scan
    touchpad_report_t report;
    if (touchpad_scan(&report) == 0 && report.contact) {
        if (wasTouching) {
            int dx = (int)report.x - (int)prevTouchX;
            int dy = (int)report.y - (int)prevTouchY;
            // Sensible scaling for touchpad to screen delta (Y inverted to match screen coords)
            cursorX += (double)dx * 0.35;
            cursorY -= (double)dy * 0.35;
        }
        prevTouchX = report.x;
        prevTouchY = report.y;
        wasTouching = true;
    } else {
        wasTouching = false;
    }

    // 2. Calculator arrow keys
    double cursorSpeed = 3.0;
    if (isKeyPressed(KEY_NSPIRE_SHIFT)) {
        cursorSpeed = 7.0;
    }

    if (isKeyPressed(KEY_NSPIRE_LEFT))  cursorX -= cursorSpeed;
    if (isKeyPressed(KEY_NSPIRE_RIGHT)) cursorX += cursorSpeed;
    if (isKeyPressed(KEY_NSPIRE_UP))    cursorY -= cursorSpeed;
    if (isKeyPressed(KEY_NSPIRE_DOWN))  cursorY += cursorSpeed;

    // Clamp cursor to screen bounds
    if (cursorX < 2.0)   cursorX = 2.0;
    if (cursorX > 317.0) cursorX = 317.0;
    if (cursorY < 2.0)   cursorY = 2.0;
    if (cursorY > 237.0) cursorY = 237.0;

    // 3. Center click / Enter
    bool currentClickState = (isKeyPressed(KEY_NSPIRE_CLICK) ||
                              isKeyPressed(KEY_NSPIRE_ENTER) ||
                              (wasTouching && report.pressed));
    if (currentClickState && !prevClickState) {
        handleCanvasClick();
    }
    prevClickState = currentClickState;

    if (isKeyPressed(KEY_NSPIRE_ESC)) {
        if (isEditingValue) {
            finishEditingComponent(false);
            while (isKeyPressed(KEY_NSPIRE_ESC)) SDL_Delay(20);
        } else {
            quitRequested = true;
        }
    }

    // 4. SDL Event queue
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

    // Update hover states
    hoveredDotCol = getClosestDotCol((int)cursorX);
    hoveredDotRow = getClosestDotRow((int)cursorY);
    hoveredComponent = getComponentAt((int)cursorX, (int)cursorY);
}

void CircuitGui::run() {
    Uint32 lastTime = SDL_GetTicks();

    while (!quitRequested) {
        handleInputs();
        simulationStep();

        // Render scene
        drawGrid();
        drawComponents();
        drawTopBar();
        drawBottomToolbar();
        drawCursor();
        drawEditDialog();

        SDL_Flip(screen);

        // Frame rate limit (~40 FPS) to conserve calculator battery
        Uint32 now = SDL_GetTicks();
        Uint32 elapsed = now - lastTime;
        if (elapsed < 25) {
            SDL_Delay(25 - elapsed);
        }
        lastTime = SDL_GetTicks();
    }
}