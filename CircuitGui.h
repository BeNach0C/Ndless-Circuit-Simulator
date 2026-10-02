#ifndef CIRCUIT_GUI_H
#define CIRCUIT_GUI_H

#include <os.h>
#include <SDL/SDL_config.h>
#include <SDL/SDL.h>
#include <SDL/SDL_gfxPrimitives.h>
#include "MNAEngine.h"
#include "Components.h"
#include <vector>
#include <string>

enum ToolType {
    TOOL_WIRE = 0,
    TOOL_RESISTOR,
    TOOL_CAPACITOR,
    TOOL_INDUCTOR,
    TOOL_VOLTAGE_SRC,
    TOOL_AC_VOLTAGE_SRC,
    TOOL_CURRENT_SRC,
    TOOL_IMPEDANCE,
    TOOL_GROUND,
    TOOL_DELETE,
    TOOL_EDIT,
    NUM_TOOLS
};

class CircuitGui {
public:
    CircuitGui();
    ~CircuitGui();

    bool init();
    void run();
    void shutdown();

private:
    // Simulation & Topology
    MNASystem mna;
    TransientEngine engine;
    std::vector<Component*> components;
    bool topologyChanged;
    void rebuildTopology();
    void simulationStep();
    void computeWireCurrents();

    // Built-in circuit presets & macros
    void loadPresetRC();
    void loadPresetRLC();
    void loadPresetBridge();
    void addWyeGeneratorMacro();
    void addDeltaGeneratorMacro();
    void clearCircuit();

    // Grid snap layout
    static const int GRID_COLS = 15;
    static const int GRID_ROWS = 10;
    static const int GRID_ORIGIN_X = 20;
    static const int GRID_ORIGIN_Y = 28;
    static const int GRID_SPACING = 20;

    int getDotPixelX(int col) const { return GRID_ORIGIN_X + col * GRID_SPACING; }
    int getDotPixelY(int row) const { return GRID_ORIGIN_Y + row * GRID_SPACING; }
    int getClosestDotCol(int px) const;
    int getClosestDotRow(int py) const;
    Component* getComponentAt(int px, int py);

    // SDL & Rendering
    SDL_Surface* screen;
    nSDL_Font* fontWhite;
    nSDL_Font* fontYellow;
    nSDL_Font* fontCyan;
    nSDL_Font* fontGreen;

    void drawGrid();
    void drawComponents();
    void drawComponent(Component* comp);
    void drawTopBar();
    void drawBottomToolbar();
    void drawCursor();
    void drawEditDialog();
    void drawScope();

    // Input & Mode state
    double cursorX;
    double cursorY;
    int hoveredDotCol;
    int hoveredDotRow;
    int anchorDotCol;
    int anchorDotRow;
    Component* hoveredComponent;

    int activeTool;
    bool isRunning;
    bool quitRequested;

    // 3-Phase & Phasor State
    double globalOmega;      // e.g. 377.0 rad/s (60Hz) or 0.0 if undefined
    bool isPhasorMode;       // Toggle phasor vs time domain
    bool isScopeOpen;        // Mini-scope open/close
    bool isEditingOmega;     // Editing frequency / omega

    // Mini-oscilloscope history buffers
    static const int SCOPE_HISTORY = 70;
    double scopeA[SCOPE_HISTORY];
    double scopeB[SCOPE_HISTORY];
    double scopeC[SCOPE_HISTORY];
    int scopeHead;

    void togglePhasorMode();
    void toggleScope();

    // Value edit dialog state
    bool isEditingValue;
    Component* editingComponent;
    char editBuffer[32];
    int editBufferLen;

    // Touchpad state
    uint16_t prevTouchX;
    uint16_t prevTouchY;
    bool wasTouching;
    bool prevClickState;

    void handleInputs();
    void handleKey(SDLKey key);
    void handleCanvasClick();
    void startEditingComponent(Component* comp);
    void finishEditingComponent(bool apply);
    void startEditingOmega();
    void finishEditingOmega(bool apply);
};

#endif // CIRCUIT_GUI_H
