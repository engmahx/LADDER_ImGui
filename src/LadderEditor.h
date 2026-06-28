#pragma once
#include "imgui.h"
#include <vector>
#include <string>

enum class ToolType
{
    Select,
    NormallyOpen,
    NormallyClosed,
    Coil,
    Output,
    Timer,
    Counter,
    Branch,
    BranchUp,
    Text,
    Count
};

struct LadderElement
{
    ToolType type;
    int rung;
    int branch = 0;
    int col;
    std::string label;
    std::string tagName;
    int timerType = 0;   // 0=TON, 1=TOF, 2=TP
    float timerPreset = 10.0f;
};

class LadderEditor
{
public:
    LadderEditor();
    ~LadderEditor();

    void Render();
    bool HasUnsavedChanges() const
    {
        return !m_elements.empty();
    }

    void SetGridSpacing(float spacing)
    {
        m_gridSpacing = spacing;
    }
    void SetDotRadius(float radius)
    {
        m_dotRadius = radius;
    }

private:
    void RenderMenuBar();
    void RenderCanvas();
    void RenderToolsPanel();
    void RenderStatusBar();
    void DrawDottedGrid(ImDrawList* drawList, ImVec2 canvasPos, ImVec2 canvasSize,
                        float spacing, float radius, ImU32 color);
    void DrawElementPreview(ImDrawList* drawList, ImVec2 cellCenter, float cellSize,
                            ToolType type, ImU32 color,
                            const LadderElement* elem = nullptr);
    void PlaceElement(ToolType type, int rung, int col, int branch = 0);
    void RemoveElement(int rung, int col, int branch = 0);

    float m_gridSpacing;
    float m_dotRadius;
    float m_zoom;
    ToolType m_selectedTool;
    ImVec2 m_canvasScroll;

    int m_lastHoveredRung;
    int m_lastHoveredCol;
    int m_lastHoveredBranch;
    int m_rungCount;
    int m_visibleCols;
    int m_prevVisibleCols;

    bool m_isDragging;
    int m_dragRung;
    int m_dragCol;
    int m_dragBranch;
    ToolType m_dragType;

    int m_selRung;
    int m_selCol;
    int m_selBranch;

    int m_selectedRung;

    std::vector<LadderElement> m_elements;
};
