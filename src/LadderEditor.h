#pragma once
#include "imgui.h"
#include <vector>
#include <string>

enum class ToolType {
    Select,
    NormallyOpen,
    NormallyClosed,
    Coil,
    Output,
    Timer,
    Counter,
    Branch,
    Text,
    Count
};

struct LadderElement {
    ToolType type;
    int rung;
    int col;
    std::string label;
};

class LadderEditor {
public:
    LadderEditor();
    ~LadderEditor();

    void Render();
    bool HasUnsavedChanges() const { return !m_elements.empty(); }

    void SetGridSpacing(float spacing) { m_gridSpacing = spacing; }
    void SetDotRadius(float radius) { m_dotRadius = radius; }

private:
    void RenderMenuBar();
    void RenderCanvas();
    void RenderToolsPanel();
    void RenderStatusBar();
    void DrawDottedGrid(ImDrawList* drawList, ImVec2 canvasPos, ImVec2 canvasSize,
                        float spacing, float radius, ImU32 color);
    void DrawElementPreview(ImDrawList* drawList, ImVec2 cellCenter, float cellSize,
                            ToolType type, ImU32 color);
    void PlaceElement(ToolType type, int rung, int col);
    void RemoveElement(int rung, int col);

    float m_gridSpacing;
    float m_dotRadius;
    float m_zoom;
    ToolType m_selectedTool;
    ImVec2 m_canvasScroll;

    int m_lastHoveredRung;
    int m_lastHoveredCol;
    int m_visibleRungs;
    int m_visibleCols;

    std::vector<LadderElement> m_elements;
};
