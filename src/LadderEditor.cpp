#include "LadderEditor.h"
#include <cmath>
#include <algorithm>

#ifndef IM_PI
#define IM_PI 3.14159265358979323846f
#endif

static const char* ToolNames[] = {
    "Select", "NO", "NC", "Coil", "Output",
    "Timer", "Counter", "Branch", "Text"
};

static ImU32 ToolColors[] = {
    IM_COL32(200, 200, 200, 255),
    IM_COL32(100, 200, 100, 255),
    IM_COL32(200, 100, 100, 255),
    IM_COL32(100, 150, 255, 255),
    IM_COL32(255, 200, 50,  255),
    IM_COL32(200, 100, 255, 255),
    IM_COL32(255, 150, 100, 255),
    IM_COL32(150, 150, 150, 255),
    IM_COL32(200, 200, 200, 255),
};

static_assert(sizeof(ToolNames) / sizeof(ToolNames[0]) == (size_t)ToolType::Count);
static_assert(sizeof(ToolColors) / sizeof(ToolColors[0]) == (size_t)ToolType::Count);

LadderEditor::LadderEditor()
    : m_gridSpacing(24.0f)
    , m_dotRadius(1.5f)
    , m_zoom(1.0f)
    , m_selectedTool(ToolType::Select)
    , m_canvasScroll(0, 0)
    , m_lastHoveredRung(-1)
    , m_lastHoveredCol(-1)
    , m_visibleRungs(0)
    , m_visibleCols(0)
{
}

LadderEditor::~LadderEditor() = default;

void LadderEditor::Render() {
    RenderMenuBar();

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 wp = viewport->WorkPos;
    ImVec2 ws = viewport->WorkSize;

    float toolsW = 180.0f;
    float propsW = 250.0f;
    float statusH = 24.0f;
    float panelH = ws.y - statusH;

    ImGuiStyle& style = ImGui::GetStyle();
    float prevRounding = style.WindowRounding;
    style.WindowRounding = 12.0f;

    ImGuiWindowFlags dockFlags = ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoCollapse;

    ImGui::SetNextWindowPos(ImVec2(wp.x, wp.y));
    ImGui::SetNextWindowSize(ImVec2(toolsW, panelH));
    ImGui::Begin("Tools", nullptr, dockFlags);
    RenderToolsPanel();
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(wp.x + toolsW, wp.y));
    ImGui::SetNextWindowSize(ImVec2(ws.x - toolsW - propsW, panelH));
    ImGui::Begin("Ladder Canvas", nullptr, dockFlags);
    RenderCanvas();
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(wp.x + ws.x - propsW, wp.y));
    ImGui::SetNextWindowSize(ImVec2(propsW, panelH));
    ImGui::Begin("Properties", nullptr, dockFlags);
    ImGui::Text("Selected Tool: %s", ToolNames[(int)m_selectedTool]);
    ImGui::Separator();
    ImGui::Text("Grid Spacing");
    ImGui::SliderFloat("##grid", &m_gridSpacing, 12.0f, 48.0f, "%.0f");
    ImGui::Text("Zoom");
    ImGui::SliderFloat("##zoom", &m_zoom, 0.25f, 3.0f, "%.2f");
    int elemCount = (int)m_elements.size();
    ImGui::Text("Elements: %d", elemCount);
    ImGui::End();

    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        m_selectedTool = ToolType::Select;
    }

    style.WindowRounding = prevRounding;

    RenderStatusBar();
}

void LadderEditor::RenderMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            ImGui::MenuItem("New Project", "Ctrl+N");
            ImGui::MenuItem("Open", "Ctrl+O");
            ImGui::MenuItem("Save", "Ctrl+S");
            ImGui::Separator();
            ImGui::MenuItem("Exit", "Alt+F4");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            ImGui::MenuItem("Undo", "Ctrl+Z");
            ImGui::MenuItem("Redo", "Ctrl+Y");
            ImGui::Separator();
            ImGui::MenuItem("Cut", "Ctrl+X");
            ImGui::MenuItem("Copy", "Ctrl+C");
            ImGui::MenuItem("Paste", "Ctrl+V");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Zoom In", "Ctrl++");
            ImGui::MenuItem("Zoom Out", "Ctrl+-");
            ImGui::MenuItem("Reset Zoom", "Ctrl+0");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            ImGui::MenuItem("About");
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void LadderEditor::RenderCanvas() {
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    float z = m_zoom;
    float spacing = m_gridSpacing * z;
    float rungHeight = spacing * 4;
    float colWidth = spacing * 3;
    float marginLeft = 60 * z;
    float marginTop = 30 * z;
    float marginRight = 20 * z;

    int numCols = std::max(1, (int)((canvasSize.x - marginLeft - marginRight) / colWidth));
    int numRungs = std::max(1, (int)((canvasSize.y - marginTop - marginRight) / (rungHeight + spacing * 2)));
    numCols = std::min(numCols, 100);
    numRungs = std::min(numRungs, 500);
    m_visibleRungs = numRungs;
    m_visibleCols = numCols;

    ImVec2 gridOrigin = ImVec2(canvasPos.x + marginLeft, canvasPos.y + marginTop);

    DrawDottedGrid(drawList, canvasPos, canvasSize, m_gridSpacing, m_dotRadius,
                   IM_COL32(100, 100, 100, 80));

    float rightRailX = gridOrigin.x + numCols * colWidth + 6 * z;

    m_lastHoveredRung = -1;
    m_lastHoveredCol = -1;

    for (int r = 0; r < numRungs; ++r) {
        float rungY = gridOrigin.y + r * (rungHeight + spacing * 2);

        if (rungY + rungHeight < canvasPos.y)
            continue;

        if (rungY > canvasPos.y + canvasSize.y)
            break;

        drawList->AddRectFilled(
            ImVec2(gridOrigin.x - 8 * z, rungY - 4 * z),
            ImVec2(gridOrigin.x - 4 * z, rungY + rungHeight + 4 * z),
            IM_COL32(60, 120, 200, 200)
        );

        drawList->AddRectFilled(
            ImVec2(rightRailX + 4 * z, rungY - 4 * z),
            ImVec2(rightRailX + 8 * z, rungY + rungHeight + 4 * z),
            IM_COL32(60, 120, 200, 200)
        );

        drawList->AddLine(
            ImVec2(gridOrigin.x - 6 * z, rungY),
            ImVec2(rightRailX + 6 * z, rungY),
            IM_COL32(80, 140, 220, 180)
        );

        drawList->AddLine(
            ImVec2(gridOrigin.x - 6 * z, rungY + rungHeight),
            ImVec2(rightRailX + 6 * z, rungY + rungHeight),
            IM_COL32(80, 140, 220, 180)
        );

        float wireY = rungY + rungHeight * 0.5f;
        {
            std::vector<int> elemCols;
            for (const auto& elem : m_elements)
                if (elem.rung == r)
                    elemCols.push_back(elem.col);
            std::sort(elemCols.begin(), elemCols.end());

            float cw = colWidth;
            float railL = gridOrigin.x - 6 * z;
            float railR = rightRailX + 6 * z;
            ImU32 wireCol = IM_COL32(200, 210, 230, 160);
            float wireThick = 1.5f * z;

            auto termHalf = [&](int col) -> float {
                for (const auto& elem : m_elements)
                    if (elem.rung == r && elem.col == col) {
                        switch (elem.type) {
                        case ToolType::NormallyOpen:
                        case ToolType::NormallyClosed:
                            return 0.06f * cw;
                        case ToolType::Coil:
                        case ToolType::Output:
                            return 0.18f * cw;
                        case ToolType::Timer:
                        case ToolType::Counter:
                            return 0.20f * cw;
                        default:
                            return 0.20f * cw;
                        }
                    }
                return 0.0f;
            };

            auto colCenter = [&](int col) { return gridOrigin.x + col * cw + cw * 0.5f; };

            if (elemCols.empty()) {
                drawList->AddLine(ImVec2(railL, wireY), ImVec2(railR, wireY), wireCol, wireThick);
            } else {
                drawList->AddLine(ImVec2(railL, wireY), ImVec2(colCenter(elemCols[0]) - termHalf(elemCols[0]), wireY), wireCol, wireThick);
                for (size_t ei = 1; ei < elemCols.size(); ++ei) {
                    float x1 = colCenter(elemCols[ei - 1]) + termHalf(elemCols[ei - 1]);
                    float x2 = colCenter(elemCols[ei]) - termHalf(elemCols[ei]);
                    drawList->AddLine(ImVec2(x1, wireY), ImVec2(x2, wireY), wireCol, wireThick);
                }
                drawList->AddLine(ImVec2(colCenter(elemCols.back()) + termHalf(elemCols.back()), wireY), ImVec2(railR, wireY), wireCol, wireThick);
            }
        }

        for (int c = 0; c < numCols; ++c) {
            float cellX = gridOrigin.x + c * colWidth;
            float cellY = rungY;
            float cellCenterX = cellX + colWidth * 0.5f;
            float cellCenterY = cellY + rungHeight * 0.5f;

            bool hovered = ImGui::IsMouseHoveringRect(
                ImVec2(cellX, cellY),
                ImVec2(cellX + colWidth, cellY + rungHeight)
            );

            if (hovered) {
                m_lastHoveredRung = r;
                m_lastHoveredCol = c;

                drawList->AddRectFilled(
                    ImVec2(cellX, cellY),
                    ImVec2(cellX + colWidth, cellY + rungHeight),
                    IM_COL32(255, 255, 255, 20)
                );

                if (m_selectedTool != ToolType::Select && ImGui::IsMouseClicked(0)) {
                    PlaceElement(m_selectedTool, r, c);
                    ImGui::ResetMouseDragDelta();
                }
            }

            bool hasElement = false;
            for (const auto& elem : m_elements) {
                if (elem.rung == r && elem.col == c) {
                    DrawElementPreview(drawList,
                        ImVec2(cellCenterX, cellCenterY), colWidth * 0.4f,
                        elem.type, ToolColors[(int)elem.type]);
                    hasElement = true;
                    break;
                }
            }

            if (!hasElement && hovered && m_selectedTool != ToolType::Select) {
                DrawElementPreview(drawList,
                    ImVec2(cellCenterX, cellCenterY), colWidth * 0.4f,
                    m_selectedTool, ToolColors[(int)m_selectedTool]);
            }
        }
    }

    ImGui::SetCursorScreenPos(canvasPos);
    ImGui::InvisibleButton("canvas_bg", canvasSize,
                           ImGuiButtonFlags_MouseButtonLeft |
                           ImGuiButtonFlags_MouseButtonRight);

    if (ImGui::BeginPopupContextWindow("ElementMenu")) {
        ImGui::Text("Insert Element");
        ImGui::Separator();
        if (ImGui::MenuItem("NO Contact")) {
            if (m_lastHoveredRung >= 0)
                PlaceElement(ToolType::NormallyOpen, m_lastHoveredRung, m_lastHoveredCol);
        }
        if (ImGui::MenuItem("NC Contact")) {
            if (m_lastHoveredRung >= 0)
                PlaceElement(ToolType::NormallyClosed, m_lastHoveredRung, m_lastHoveredCol);
        }
        if (ImGui::MenuItem("Coil")) {
            if (m_lastHoveredRung >= 0)
                PlaceElement(ToolType::Coil, m_lastHoveredRung, m_lastHoveredCol);
        }
        if (ImGui::MenuItem("Output")) {
            if (m_lastHoveredRung >= 0)
                PlaceElement(ToolType::Output, m_lastHoveredRung, m_lastHoveredCol);
        }
        if (ImGui::MenuItem("Timer")) {
            if (m_lastHoveredRung >= 0)
                PlaceElement(ToolType::Timer, m_lastHoveredRung, m_lastHoveredCol);
        }
        if (ImGui::MenuItem("Counter")) {
            if (m_lastHoveredRung >= 0)
                PlaceElement(ToolType::Counter, m_lastHoveredRung, m_lastHoveredCol);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Remove", nullptr, false, m_lastHoveredRung >= 0)) {
            RemoveElement(m_lastHoveredRung, m_lastHoveredCol);
        }
        ImGui::EndPopup();
    }
}

void LadderEditor::PlaceElement(ToolType type, int rung, int col) {
    for (auto& elem : m_elements) {
        if (elem.rung == rung && elem.col == col) {
            elem.type = type;
            return;
        }
    }
    LadderElement elem;
    elem.type = type;
    elem.rung = rung;
    elem.col = col;
    elem.label = ToolNames[(int)type];
    m_elements.push_back(elem);
}

void LadderEditor::RemoveElement(int rung, int col) {
    for (size_t i = 0; i < m_elements.size(); ++i) {
        if (m_elements[i].rung == rung && m_elements[i].col == col) {
            m_elements.erase(m_elements.begin() + i);
            return;
        }
    }
}

void LadderEditor::RenderToolsPanel() {
    ImGui::Text("Ladder Elements");
    ImGui::Separator();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));

    for (int i = 0; i < (int)ToolType::Count; ++i) {
        ImVec4 tint = ImGui::ColorConvertU32ToFloat4(ToolColors[i]);
        ImGui::PushStyleColor(ImGuiCol_Text, tint);

        bool isSelected = (m_selectedTool == (ToolType)i);
        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.5f, 0.8f, 1.0f));
        }

        if (ImGui::Button(ToolNames[i], ImVec2(-1, 32))) {
            m_selectedTool = (ToolType)i;
        }

        if (isSelected) {
            ImGui::PopStyleColor();
        }

        ImGui::PopStyleColor();

        if (i < (int)ToolType::Count - 1) {
            ImGui::Spacing();
        }
    }

    ImGui::PopStyleColor(3);

    ImGui::Separator();
    ImGui::Text("Rung Info");
    ImGui::Text("Rungs: %d  Cols: %d", m_visibleRungs, m_visibleCols);
    ImGui::Text("Zoom: %.0f%%", m_zoom * 100);
}

void LadderEditor::RenderStatusBar() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + viewport->Size.y - 24));
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, 24));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));

    ImGui::Begin("StatusBar", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoSavedSettings);

    ImGui::Text("Tool: %s", ToolNames[(int)m_selectedTool]);
    ImGui::SameLine();
    ImGui::Text("  |  Grid: %.0fpx  |  Zoom: %.0f%%  |  Elements: %d",
                m_gridSpacing, m_zoom * 100, (int)m_elements.size());

    ImGui::End();
    ImGui::PopStyleColor(1);
    ImGui::PopStyleVar(2);
}

void LadderEditor::DrawDottedGrid(ImDrawList* drawList, ImVec2 canvasPos,
                                   ImVec2 canvasSize, float spacing,
                                   float radius, ImU32 color) {
    float z = m_zoom;
    float startX = canvasPos.x + fmodf(m_canvasScroll.x, spacing);
    float startY = canvasPos.y + fmodf(m_canvasScroll.y, spacing);

    for (float x = startX; x < canvasPos.x + canvasSize.x; x += spacing * z) {
        for (float y = startY; y < canvasPos.y + canvasSize.y; y += spacing * z) {
            drawList->AddCircleFilled(ImVec2(x, y), radius * z, color);
        }
    }
}

void LadderEditor::DrawElementPreview(ImDrawList* drawList, ImVec2 center,
                                       float size, ToolType type, ImU32 color) {
    float half = size * 0.5f;
    switch (type) {
    case ToolType::NormallyOpen:
        drawList->AddLine(ImVec2(center.x - half, center.y),
                          ImVec2(center.x - half * 0.3f, center.y), color, 2.0f);
        drawList->AddLine(ImVec2(center.x + half * 0.3f, center.y),
                          ImVec2(center.x + half, center.y), color, 2.0f);
        drawList->AddLine(ImVec2(center.x - half * 0.3f, center.y - half * 0.7f),
                          ImVec2(center.x - half * 0.3f, center.y + half * 0.7f), color, 2.0f);
        drawList->AddLine(ImVec2(center.x + half * 0.3f, center.y - half * 0.7f),
                          ImVec2(center.x + half * 0.3f, center.y + half * 0.7f), color, 2.0f);
        break;

    case ToolType::NormallyClosed:
        drawList->AddLine(ImVec2(center.x - half, center.y),
                          ImVec2(center.x - half * 0.3f, center.y), color, 2.0f);
        drawList->AddLine(ImVec2(center.x + half * 0.3f, center.y),
                          ImVec2(center.x + half, center.y), color, 2.0f);
        drawList->AddLine(ImVec2(center.x - half * 0.3f, center.y - half * 0.7f),
                          ImVec2(center.x - half * 0.3f, center.y + half * 0.7f), color, 2.0f);
        drawList->AddLine(ImVec2(center.x + half * 0.3f, center.y - half * 0.7f),
                          ImVec2(center.x + half * 0.3f, center.y + half * 0.7f), color, 2.0f);
        drawList->AddLine(ImVec2(center.x - half * 0.3f, center.y - half * 0.7f),
                          ImVec2(center.x + half * 0.3f, center.y + half * 0.7f), color, 2.0f);
        break;

    case ToolType::Coil:
    case ToolType::Output: {
        float r = half * 0.45f;
        drawList->PathArcTo(ImVec2(center.x - r, center.y), r,
            IM_PI * 0.5f, IM_PI * 1.5f);
        drawList->PathStroke(color, false, 3.0f);
        drawList->PathArcTo(ImVec2(center.x + r, center.y), r,
            IM_PI * 1.5f, IM_PI * 2.5f);
        drawList->PathStroke(color, false, 3.0f);
        break;
    }

    case ToolType::Timer:
    case ToolType::Counter:
        drawList->AddRect(ImVec2(center.x - half, center.y - half * 0.6f),
                          ImVec2(center.x + half, center.y + half * 0.6f),
                          color, 0.0f, 0, 2.0f);
        break;

    case ToolType::Branch:
        drawList->AddLine(ImVec2(center.x, center.y - half),
                          ImVec2(center.x, center.y + half), color, 2.0f);
        drawList->AddLine(ImVec2(center.x - half, center.y),
                          ImVec2(center.x, center.y), color, 2.0f);
        break;

    case ToolType::Text:
        drawList->AddLine(ImVec2(center.x - half, center.y),
                          ImVec2(center.x + half, center.y), color, 2.0f);
        drawList->AddLine(ImVec2(center.x - half, center.y - half * 0.3f),
                          ImVec2(center.x - half, center.y + half * 0.3f), color, 2.0f);
        break;

    default:
        drawList->AddCircle(center, half * 0.4f, color, 0, 1.5f);
        break;
    }
}
