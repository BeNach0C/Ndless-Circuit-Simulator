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
    TOOL_CURRENT_SRC,
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

    // Built-in circuit presets
    void loadPresetRC();
    void loadPresetRLC();
    void loadPresetBridge();
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

    // Input state
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
};

#endif // CIRCUIT_GUI_H
