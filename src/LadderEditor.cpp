#include "LadderEditor.h"
#include <cmath>
#include <algorithm>

#ifndef IM_PI
#define IM_PI 3.14159265358979323846f
#endif

static const char* ToolNames[] = {
    "Select", "NO", "NC", "Coil", "Output",
    "Timer", "Counter", "Branch", "BranchUp", "Text"
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
    IM_COL32(100, 200, 200, 255),
    IM_COL32(200, 200, 200, 255),
};

static_assert(sizeof(ToolNames) / sizeof(ToolNames[0]) == (size_t)ToolType::Count);
static_assert(sizeof(ToolColors) / sizeof(ToolColors[0]) == (size_t)ToolType::Count);

LadderEditor::LadderEditor()
    : m_gridSpacing(30.0f)
    , m_dotRadius(1.5f)
    , m_zoom(1.0f)
    , m_selectedTool(ToolType::Select)
    , m_canvasScroll(0, 0)
    , m_lastHoveredRung(-1)
    , m_lastHoveredCol(-1)
    , m_lastHoveredBranch(-1)
    , m_visibleCols(0)
    , m_prevVisibleCols(0)
    , m_rungCount(1)
    , m_isDragging(false)
    , m_dragRung(-1)
    , m_dragCol(-1)
    , m_dragBranch(-1)
    , m_dragType(ToolType::Select)
    , m_selRung(-1)
    , m_selCol(-1)
    , m_selBranch(-1)
    , m_selectedRung(-1)
{
}

LadderEditor::~LadderEditor() = default;

void LadderEditor::Render()
{
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
    ImGui::SliderFloat("##grid", &m_gridSpacing, 25.0f, 50.0f, "%.0f");
    ImGui::Text("Zoom");
    ImGui::SliderFloat("##zoom", &m_zoom, 0.5f, 2.0f, "%.2f");
    int elemCount = (int)m_elements.size();
    ImGui::Text("Elements: %d", elemCount);

    ImGui::Separator();
    ImGui::Text("Element Properties");
    {
        LadderElement* sel = nullptr;
        for (auto& e : m_elements)
            if (e.rung == m_selRung && e.col == m_selCol && e.branch == m_selBranch)
                { sel = &e; break; }
        if (sel)
        {
            ImGui::Text("Type: %s", ToolNames[(int)sel->type]);
            ImGui::Text("Rung: %d  Branch: %d  Col: %d", sel->rung, sel->branch, sel->col);
            if (sel->type != ToolType::Branch && sel->type != ToolType::BranchUp)
            {
                ImGui::Text("Tag Name");
                char buf[128];
                snprintf(buf, sizeof(buf), "%s", sel->tagName.c_str());
                if (ImGui::InputText("##tag", buf, sizeof(buf)))
                {
                    sel->tagName = buf;
                }
            }
            if (sel->type == ToolType::Timer)
            {
                ImGui::Separator();
                ImGui::Text("Timer Type");
                const char* timerItems[] = { "ON-Delay", "OFF-Delay", "Pulse" };
                int ti = sel->timerType;
                if (ImGui::Combo("##timerType", &ti, timerItems, IM_ARRAYSIZE(timerItems)))
                {
                    sel->timerType = ti;
                }
                ImGui::Text("Preset Time (s)");
                ImGui::SetNextItemWidth(120.0f);
                ImGui::InputFloat("##timerPreset", &sel->timerPreset, 0.1f, 1.0f, "%.1f");
            }
        } else
        {
            ImGui::TextDisabled("No element selected");
        }
    }
    ImGui::End();

    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    {
        m_selectedTool = ToolType::Select;
        m_selRung = -1;
        m_selCol = -1;
        m_selBranch = -1;
        m_selectedRung = -1;
        m_isDragging = false;
        m_dragRung = -1;
        m_dragCol = -1;
        m_dragBranch = -1;
        m_dragType = ToolType::Select;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && m_lastHoveredRung >= 0 && m_lastHoveredCol >= 0)
    {
        RemoveElement(m_lastHoveredRung, m_lastHoveredCol, m_lastHoveredBranch);
    }

    style.WindowRounding = prevRounding;

    RenderStatusBar();
}

void LadderEditor::RenderMenuBar()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            ImGui::MenuItem("New Project", "Ctrl+N");
            ImGui::MenuItem("Open", "Ctrl+O");
            ImGui::MenuItem("Save", "Ctrl+S");
            ImGui::Separator();
            ImGui::MenuItem("Exit", "Alt+F4");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit"))
        {
            ImGui::MenuItem("Undo", "Ctrl+Z");
            ImGui::MenuItem("Redo", "Ctrl+Y");
            ImGui::Separator();
            ImGui::MenuItem("Cut", "Ctrl+X");
            ImGui::MenuItem("Copy", "Ctrl+C");
            ImGui::MenuItem("Paste", "Ctrl+V");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem("Zoom In", "Ctrl++");
            ImGui::MenuItem("Zoom Out", "Ctrl+-");
            ImGui::MenuItem("Reset Zoom", "Ctrl+0");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help"))
        {
            ImGui::MenuItem("About");
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void LadderEditor::RenderCanvas()
{
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
    numCols = std::min(numCols, 100);
    m_rungCount = std::max(1, m_rungCount);

    if (numCols != m_prevVisibleCols)
    {
        int newRight = numCols - 1;

        if (numCols > m_prevVisibleCols)
        {
            for (auto& elem : m_elements)
            {
                if ((elem.type == ToolType::Coil || elem.type == ToolType::Output) && elem.col != newRight)
                {
                    bool occupied = false;
                    for (const auto& other : m_elements)
                        if (&other != &elem && other.rung == elem.rung && other.branch == elem.branch && other.col == newRight)
                            { occupied = true; break; }
                    if (!occupied)
                        elem.col = newRight;
                }
            }
        }

        if (numCols < m_prevVisibleCols)
        {
            for (auto& coil : m_elements)
            {
                if (coil.type == ToolType::Coil || coil.type == ToolType::Output)
                {
                    bool conflict = false;
                    for (const auto& other : m_elements)
                        if (&other != &coil && other.rung == coil.rung && other.branch == coil.branch && other.col == newRight
                            && other.type != ToolType::Coil && other.type != ToolType::Output)
                            { conflict = true; break; }

                    if (conflict || coil.col > newRight)
                    {
                        std::vector<LadderElement*> nonCoils;
                        for (auto& other : m_elements)
                            if (&other != &coil && other.rung == coil.rung && other.branch == coil.branch
                                && other.type != ToolType::Coil && other.type != ToolType::Output)
                                nonCoils.push_back(&other);
                        std::sort(nonCoils.begin(), nonCoils.end(),
                            [](const LadderElement* a, const LadderElement* b) { return a->col < b->col; });
                        for (size_t i = 0; i < nonCoils.size(); ++i)
                        {
                            int maxCol = newRight > 0 ? newRight - 1 : 0;
                            nonCoils[i]->col = (int)i < maxCol ? (int)i : maxCol;
                        }
                    }

                    coil.col = newRight;
                }
            }
        }

        m_prevVisibleCols = numCols;
    }
    m_visibleCols = numCols;

    ImVec2 gridOrigin = ImVec2(canvasPos.x + marginLeft, canvasPos.y + marginTop);

    DrawDottedGrid(drawList, canvasPos, canvasSize, m_gridSpacing, m_dotRadius,
                   IM_COL32(100, 100, 100, 80));

    float rightRailX = gridOrigin.x + numCols * colWidth + 6 * z;

    m_lastHoveredRung = -1;
    m_lastHoveredCol = -1;
    m_lastHoveredBranch = -1;

    float branchExtraOffset = 0.0f;
    float cw = colWidth;
    float railL = gridOrigin.x - 6 * z;
    float railR = rightRailX + 6 * z;
    ImU32 wireCol = IM_COL32(200, 210, 230, 160);
    float wireThick = 1.5f * z;

    auto colCenter = [&](int col) { return gridOrigin.x + col * cw + cw * 0.5f; };

    for (int r = 0; r < m_rungCount; ++r)
    {
        auto termHalf = [&](int col, int branch) -> float {
            for (const auto& elem : m_elements)
                if (elem.rung == r && elem.branch == branch && elem.col == col)
                {
                    switch (elem.type)
                    {
                    case ToolType::NormallyOpen:
                    case ToolType::NormallyClosed:
                        return 0.06f * cw;
                    case ToolType::Coil:
                    case ToolType::Output:
                        return 0.18f * cw;
                    case ToolType::Timer:
                        return 0.40f * cw;
                    case ToolType::Counter:
                        return 0.20f * cw;
                    case ToolType::Branch:
                    case ToolType::BranchUp:
                        return 0.0f;
                    default:
                        return 0.20f * cw;
                    }
                }
            return 0.0f;
        };

        int maxBranch = 0;
        for (const auto& e : m_elements)
            if (e.rung == r && e.branch > maxBranch)
                maxBranch = e.branch;

        for (const auto& e : m_elements)
            if (e.rung == r && e.type == ToolType::Branch)
            {
                int target = e.branch + 1;
                if (target > maxBranch) maxBranch = target;
            }

        // Precompute BranchUp reconnection targets for this rung
        std::vector<int> buTarget(maxBranch + 1, -1);
        for (const auto& bu : m_elements)
            if (bu.rung == r && bu.branch > 0 && bu.type == ToolType::BranchUp)
            {
                int target = -1;
                for (int tb = bu.branch - 1; tb >= 0; tb--)
                {
                    bool terminated = false;
                    for (const auto& e2 : m_elements)
                        if (e2.rung == r && e2.branch == tb && e2.type == ToolType::BranchUp)
                        {
                            if (bu.col >= e2.col) { terminated = true; }
                            break;
                        }
                    if (!terminated) { target = tb; break; }
                }
                if (target < 0) target = 0;
                buTarget[bu.branch] = target;
            }

        struct BlockedCell { int branch; int col; };
        std::vector<BlockedCell> blockedCells;
        for (const auto& bu : m_elements)
            if (bu.rung == r && bu.branch > 0 && bu.type == ToolType::BranchUp)
            {
                int target = buTarget[bu.branch];
                for (int b = target; b < bu.branch; ++b)
                    blockedCells.push_back({b, bu.col});
            }

        float rungY = gridOrigin.y + r * (rungHeight + spacing * 2) + branchExtraOffset + m_canvasScroll.y;
        float totalRungH = rungHeight + maxBranch * (rungHeight + spacing * 2);

        if (rungY + totalRungH < canvasPos.y)
        {
            branchExtraOffset += maxBranch * (rungHeight + spacing * 2);
            continue;
        }

        if (rungY > canvasPos.y + canvasSize.y)
            break;

        drawList->AddRectFilled(
            ImVec2(gridOrigin.x - 8 * z, rungY - 4 * z),
            ImVec2(gridOrigin.x - 4 * z, rungY + totalRungH + 4 * z),
            IM_COL32(60, 120, 200, 200));
        drawList->AddRectFilled(
            ImVec2(rightRailX + 4 * z, rungY - 4 * z),
            ImVec2(rightRailX + 8 * z, rungY + totalRungH + 4 * z),
            IM_COL32(60, 120, 200, 200));

        for (int b = 0; b <= maxBranch; ++b)
        {
            float rowY = rungY + b * (rungHeight + spacing * 2);
            float wireY = rowY + rungHeight * 0.5f;

            int branchCol = -1;
            int branchUpCol = -1;
            float branchStartX = railL;
            if (b > 0)
            {
                for (const auto& e : m_elements)
                    if (e.rung == r && e.branch == b - 1 && e.type == ToolType::Branch)
                    {
                        branchStartX = colCenter(e.col);
                        branchCol = e.col;
                        break;
                    }
                for (const auto& e : m_elements)
                    if (e.rung == r && e.branch == b && e.type == ToolType::BranchUp)
                        { branchUpCol = e.col; break; }
            }

            {
                float wireEndX = (branchUpCol >= 0) ? colCenter(branchUpCol) : railR;

                std::vector<int> elemCols;
                for (const auto& elem : m_elements)
                    if (elem.rung == r && elem.branch == b)
                        elemCols.push_back(elem.col);
                std::sort(elemCols.begin(), elemCols.end());

                if (branchUpCol >= 0)
                {
                    elemCols.erase(
                        std::remove_if(elemCols.begin(), elemCols.end(),
                            [branchUpCol](int c) { return c >= branchUpCol; }),
                        elemCols.end());
                }

                if (elemCols.empty())
                {
                    drawList->AddLine(ImVec2(branchStartX, wireY), ImVec2(wireEndX, wireY), wireCol, wireThick);
                } else
                {
                    drawList->AddLine(ImVec2(branchStartX, wireY), ImVec2(colCenter(elemCols[0]) - termHalf(elemCols[0], b), wireY), wireCol, wireThick);
                    for (size_t ei = 1; ei < elemCols.size(); ++ei)
                    {
                        float x1 = colCenter(elemCols[ei - 1]) + termHalf(elemCols[ei - 1], b);
                        float x2 = colCenter(elemCols[ei]) - termHalf(elemCols[ei], b);
                        drawList->AddLine(ImVec2(x1, wireY), ImVec2(x2, wireY), wireCol, wireThick);
                    }
                    drawList->AddLine(ImVec2(colCenter(elemCols.back()) + termHalf(elemCols.back(), b), wireY), ImVec2(wireEndX, wireY), wireCol, wireThick);
                }
            }

            for (int c = 0; c < numCols; ++c)
            {
                float cellX = gridOrigin.x + c * colWidth;
                float cellCenterX = cellX + colWidth * 0.5f;
                float cellCenterY = rowY + rungHeight * 0.5f;

                bool hovered = ImGui::IsMouseHoveringRect(
                    ImVec2(cellX, rowY),
                    ImVec2(cellX + colWidth, rowY + rungHeight));

                bool blockedUpBelow = false;
                for (const auto& bc : blockedCells)
                    if (bc.branch == b && bc.col == c) { blockedUpBelow = true; break; }

                if (hovered)
                {
                    m_lastHoveredRung = r;
                    m_lastHoveredCol = c;
                    m_lastHoveredBranch = b;

                    drawList->AddRectFilled(
                        ImVec2(cellX, rowY),
                        ImVec2(cellX + colWidth, rowY + rungHeight),
                        IM_COL32(255, 255, 255, 20));

                    if (m_selectedTool != ToolType::Select && ImGui::IsMouseClicked(0)
                        && !(b > 0 && branchCol >= 0 && c <= branchCol)
                        && !(b > 0 && branchUpCol >= 0 && c >= branchUpCol && m_selectedTool != ToolType::BranchUp)
                        && !blockedUpBelow)
                    {
                        PlaceElement(m_selectedTool, r, c, b);
                        m_selectedRung = r;
                        ImGui::ResetMouseDragDelta();
                    }

                    if (m_selectedTool == ToolType::Select && ImGui::IsMouseClicked(0))
                    {
                        bool found = false;
                        for (const auto& elem : m_elements)
                            if (elem.rung == r && elem.branch == b && elem.col == c)
                            {
                                m_selRung = r;
                                m_selCol = c;
                                m_selBranch = b;
                                m_selectedRung = r;
                                found = true;
                                break;
                            }
                        if (!found) { m_selRung = -1; m_selCol = -1; m_selBranch = -1; }
                    }
                }

                bool hasElement = false;
                for (const auto& elem : m_elements)
                {
                    if (elem.rung == r && elem.branch == b && elem.col == c)
                    {
                        if (m_selRung == r && m_selCol == c && m_selBranch == b)
                        {
                            drawList->AddRect(
                                ImVec2(cellX + 2, rowY + 2),
                                ImVec2(cellX + colWidth - 2, rowY + rungHeight - 2),
                                IM_COL32(255, 255, 100, 200), 0.0f, 0, 2.5f);
                        }
                        ImU32 col = ToolColors[(int)elem.type];
                        if (m_isDragging && elem.rung == m_dragRung && elem.col == m_dragCol && elem.branch == m_dragBranch)
                            col = (col & 0x00FFFFFF) | 0x40000000;
                        DrawElementPreview(drawList,
                            ImVec2(cellCenterX, cellCenterY), colWidth * 0.4f,
                            elem.type, col, &elem);
                        if (elem.type == ToolType::Branch)
                        {
                            float vArm = rungHeight * 0.5f;
                            drawList->AddLine(ImVec2(cellCenterX, cellCenterY),
                                              ImVec2(cellCenterX, cellCenterY + vArm), col, 2.5f);
                        }
                        if (elem.type == ToolType::BranchUp)
                        {
                            float vArm = rungHeight * 0.5f;
                            drawList->AddLine(ImVec2(cellCenterX, cellCenterY),
                                              ImVec2(cellCenterX, cellCenterY - vArm), col, 2.5f);
                        }
                        if (elem.type != ToolType::Branch && elem.type != ToolType::BranchUp)
                        {
                            ImVec2 ts = ImGui::CalcTextSize(elem.tagName.c_str());
                            float fontSize = 24.0f * z;
                            float tsx = ts.x * (fontSize / ImGui::GetFontSize());
                            float tx = cellCenterX - tsx * 0.5f;
                            float ty = cellCenterY - rungHeight * 0.5f - fontSize * 1.1f;
                            drawList->AddText(ImGui::GetFont(), fontSize,
                                ImVec2(tx, ty), IM_COL32(255, 255, 255, 240),
                                elem.tagName.c_str());
                        }
                        hasElement = true;
                        break;
                    }
                }

                if (!hasElement && hovered && m_selectedTool != ToolType::Select
                    && !(b > 0 && branchCol >= 0 && c <= branchCol)
                    && !(b > 0 && branchUpCol >= 0 && c >= branchUpCol && m_selectedTool != ToolType::BranchUp)
                    && !blockedUpBelow)
                {
                    DrawElementPreview(drawList,
                        ImVec2(cellCenterX, cellCenterY), colWidth * 0.4f,
                        m_selectedTool, ToolColors[(int)m_selectedTool]);
                }
            }
        }

        if (ImGui::IsMouseHoveringRect(
            ImVec2(gridOrigin.x - 8 * z, rungY),
            ImVec2(rightRailX + 8 * z, rungY + totalRungH))
            && ImGui::IsMouseClicked(0) && m_selectedTool == ToolType::Select)
        {
            m_selectedRung = r;
        }

        if (m_selectedRung == r)
        {
            drawList->AddRectFilled(
                ImVec2(gridOrigin.x - 12 * z, rungY),
                ImVec2(gridOrigin.x - 8 * z, rungY + totalRungH),
                IM_COL32(255, 255, 100, 200));
        }

        // vertical connection from Branch element down to the branch row below
        for (const auto& e : m_elements)
            if (e.rung == r && e.type == ToolType::Branch)
            {
                int targetBranch = e.branch + 1;
                if (targetBranch > maxBranch) continue;
                float bcolCx = gridOrigin.x + e.col * colWidth + colWidth * 0.5f;
                float upperY = rungY + e.branch * (rungHeight + spacing * 2);
                float lowerY = rungY + targetBranch * (rungHeight + spacing * 2);
                float topY = upperY + rungHeight * 0.5f + rungHeight * 0.5f;
                float lowerWireY = lowerY + rungHeight * 0.5f;
                bool blocked = false;
                for (const auto& o : m_elements)
                    if (o.rung == r && o.branch == targetBranch && o.col == e.col)
                        { blocked = true; break; }
                if (!blocked)
                    drawList->AddLine(ImVec2(bcolCx, topY),
                                      ImVec2(bcolCx, lowerWireY), wireCol, wireThick);
            }

        // vertical reconnection from BranchUp to nearest continuous wire above
        for (const auto& bu : m_elements)
            if (bu.rung == r && bu.branch > 0 && bu.type == ToolType::BranchUp)
            {
                int targetBranch = buTarget[bu.branch];
                float bcolCx = gridOrigin.x + bu.col * colWidth + colWidth * 0.5f;
                float lowerWireY = rungY + bu.branch * (rungHeight + spacing * 2) + rungHeight * 0.5f;
                float upperWireY = rungY + targetBranch * (rungHeight + spacing * 2) + rungHeight * 0.5f;
                drawList->AddLine(ImVec2(bcolCx, lowerWireY),
                                  ImVec2(bcolCx, upperWireY), wireCol, wireThick);
            }

        branchExtraOffset += maxBranch * (rungHeight + spacing * 2);
    }

    ImGui::SetCursorScreenPos(canvasPos);
    ImGui::InvisibleButton("canvas_bg", canvasSize,
                           ImGuiButtonFlags_MouseButtonLeft |
                           ImGuiButtonFlags_MouseButtonRight);

    if (ImGui::IsItemHovered())
    {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f)
        {
            float scrollStep = rungHeight + spacing * 2;
            m_canvasScroll.y -= wheel * scrollStep;
        }
    }

    if (!m_isDragging && m_selRung >= 0 && ImGui::IsMouseDragging(0, 4.0f))
    {
        for (const auto& e : m_elements)
            if (e.rung == m_selRung && e.col == m_selCol && e.branch == m_selBranch)
            {
                m_isDragging = true;
                m_dragRung = m_selRung;
                m_dragCol = m_selCol;
                m_dragBranch = m_selBranch;
                m_dragType = e.type;
                break;
            }
    }

    if (m_isDragging && ImGui::IsMouseReleased(0))
    {
        if (m_lastHoveredRung >= 0 && m_lastHoveredCol >= 0
            && !(m_lastHoveredRung == m_dragRung && m_lastHoveredCol == m_dragCol && m_lastHoveredBranch == m_dragBranch))
        {
            std::string savedTag;
            for (const auto& e : m_elements)
                if (e.rung == m_dragRung && e.col == m_dragCol && e.branch == m_dragBranch)
                    { savedTag = e.tagName; break; }
            int oldCount = (int)m_elements.size();
            PlaceElement(m_dragType, m_lastHoveredRung, m_lastHoveredCol, m_lastHoveredBranch);
            if ((int)m_elements.size() > oldCount)
            {
                RemoveElement(m_dragRung, m_dragCol, m_dragBranch);
                for (auto& e : m_elements)
                    if (e.rung == m_lastHoveredRung && e.col == m_lastHoveredCol && e.branch == m_lastHoveredBranch)
                        { e.tagName = savedTag; break; }
                m_selRung = m_lastHoveredRung;
                m_selCol = m_lastHoveredCol;
                m_selBranch = m_lastHoveredBranch;
            }
        }
        m_isDragging = false;
    }

    if (m_isDragging)
    {
        ImVec2 mp = ImGui::GetMousePos();
        drawList->AddRectFilled(
            ImVec2(mp.x - colWidth * 0.5f, mp.y - rungHeight * 0.5f),
            ImVec2(mp.x + colWidth * 0.5f, mp.y + rungHeight * 0.5f),
            IM_COL32(255, 255, 255, 25));
        DrawElementPreview(drawList, mp, colWidth * 0.4f,
            m_dragType, ToolColors[(int)m_dragType]);
    }

    if (ImGui::BeginPopupContextWindow("ElementMenu"))
    {
        ImGui::Text("Insert Element");
        ImGui::Separator();
        if (ImGui::MenuItem("NO Contact"))
        {
            if (m_lastHoveredRung >= 0)
                PlaceElement(ToolType::NormallyOpen, m_lastHoveredRung, m_lastHoveredCol, m_lastHoveredBranch);
        }
        if (ImGui::MenuItem("NC Contact"))
        {
            if (m_lastHoveredRung >= 0)
                PlaceElement(ToolType::NormallyClosed, m_lastHoveredRung, m_lastHoveredCol, m_lastHoveredBranch);
        }
        if (ImGui::MenuItem("Coil"))
        {
            if (m_lastHoveredRung >= 0)
                PlaceElement(ToolType::Coil, m_lastHoveredRung, m_lastHoveredCol, m_lastHoveredBranch);
        }
        if (ImGui::MenuItem("Output"))
        {
            if (m_lastHoveredRung >= 0)
                PlaceElement(ToolType::Output, m_lastHoveredRung, m_lastHoveredCol, m_lastHoveredBranch);
        }
        if (ImGui::MenuItem("Timer"))
        {
            if (m_lastHoveredRung >= 0)
                PlaceElement(ToolType::Timer, m_lastHoveredRung, m_lastHoveredCol, m_lastHoveredBranch);
        }
        if (ImGui::MenuItem("Counter"))
        {
            if (m_lastHoveredRung >= 0)
                PlaceElement(ToolType::Counter, m_lastHoveredRung, m_lastHoveredCol, m_lastHoveredBranch);
        }
        if (ImGui::MenuItem("Branch Up", nullptr, false, m_lastHoveredBranch > 0))
        {
            if (m_lastHoveredRung >= 0)
                PlaceElement(ToolType::BranchUp, m_lastHoveredRung, m_lastHoveredCol, m_lastHoveredBranch);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Remove", nullptr, false, m_lastHoveredRung >= 0))
        {
            RemoveElement(m_lastHoveredRung, m_lastHoveredCol, m_lastHoveredBranch);
        }
        ImGui::EndPopup();
    }
}

void LadderEditor::PlaceElement(ToolType type, int rung, int col, int branch)
{
    // BranchUp: only on branch rows, one per branch
    if (type == ToolType::BranchUp)
    {
        if (branch == 0) return;
        for (const auto& e : m_elements)
            if (e.rung == rung && e.branch == branch && e.type == ToolType::BranchUp)
                return;
        // Don't strand existing elements past the new BranchUp column
        for (const auto& e : m_elements)
            if (e.rung == rung && e.branch == branch && e.col >= col)
                return;
    }

    bool isCoilOutput = (type == ToolType::Coil || type == ToolType::Output);

    // Coil/Output not allowed on a branch that has a BranchUp
    if (isCoilOutput && branch > 0)
        for (const auto& e : m_elements)
            if (e.rung == rung && e.branch == branch && e.type == ToolType::BranchUp)
                return;

    // Don't place where a BranchUp reconnects from a branch below
    for (const auto& bu : m_elements)
        if (bu.rung == rung && bu.branch > branch && bu.type == ToolType::BranchUp && bu.col == col)
        {
            int target = -1;
            for (int tb = bu.branch - 1; tb >= 0; tb--)
            {
                bool terminated = false;
                for (const auto& e2 : m_elements)
                    if (e2.rung == rung && e2.branch == tb && e2.type == ToolType::BranchUp)
                    {
                        if (bu.col >= e2.col) { terminated = true; }
                        break;
                    }
                if (!terminated) { target = tb; break; }
            }
            if (target < 0) target = 0;
            if (branch >= target && branch < bu.branch) return;
        }

    // Branch row column range
    if (branch > 0)
    {
        int bc = -1;
        for (const auto& e : m_elements)
            if (e.rung == rung && e.branch == branch - 1 && e.type == ToolType::Branch)
                { bc = e.col; break; }
        if (bc >= 0 && col <= bc) return;

        if (type != ToolType::BranchUp)
        {
            int buc = -1;
            for (const auto& e : m_elements)
                if (e.rung == rung && e.branch == branch && e.type == ToolType::BranchUp)
                    { buc = e.col; break; }
            if (buc >= 0 && col >= buc) return;
        }
    }

    if (isCoilOutput)
    {
        col = m_visibleCols - 1;

        for (const auto& elem : m_elements)
            if (elem.rung == rung && elem.branch == branch && (elem.type == ToolType::Coil || elem.type == ToolType::Output))
                return;
    }

    for (const auto& elem : m_elements)
        if (elem.rung == rung && elem.branch == branch && elem.col == col)
            return;

    LadderElement elem;
    elem.type = type;
    elem.rung = rung;
    elem.branch = branch;
    elem.col = col;
    elem.label = ToolNames[(int)type];
    int idx = 1;
    for (const auto& e : m_elements)
        if (e.type == type && e.branch == branch) ++idx;
    elem.tagName = ToolNames[(int)type] + std::string("_") + std::to_string(idx);
    m_elements.push_back(elem);
}

void LadderEditor::RemoveElement(int rung, int col, int branch)
{
    for (size_t i = 0; i < m_elements.size(); ++i)
    {
        if (m_elements[i].rung == rung && m_elements[i].col == col && m_elements[i].branch == branch)
        {
            bool isBranch = (m_elements[i].type == ToolType::Branch);
            if (m_selRung == rung && m_selCol == col && m_selBranch == branch)
            {
                m_selRung = -1;
                m_selCol = -1;
                m_selBranch = -1;
            }
            m_elements.erase(m_elements.begin() + i);

            if (isBranch)
            {
                int subBranch = branch + 1;
                for (int si = (int)m_elements.size() - 1; si >= 0; --si)
                {
                    if (m_elements[si].rung == rung && m_elements[si].branch >= subBranch)
                    {
                        if (m_selRung == rung && m_selCol == m_elements[si].col
                            && m_selBranch == m_elements[si].branch)
                        {
                            m_selRung = -1;
                            m_selCol = -1;
                            m_selBranch = -1;
                        }
                        m_elements.erase(m_elements.begin() + si);
                    }
                }
            }

            return;
        }
    }
}

void LadderEditor::RenderToolsPanel()
{
    ImGui::Text("Ladder Elements");
    ImGui::Separator();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));

    for (int i = 0; i < (int)ToolType::Count; ++i)
    {
        ImVec4 tint = ImGui::ColorConvertU32ToFloat4(ToolColors[i]);
        ImGui::PushStyleColor(ImGuiCol_Text, tint);

        bool isSelected = (m_selectedTool == (ToolType)i);
        if (isSelected)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.5f, 0.8f, 1.0f));
        }

        if (ImGui::Button(ToolNames[i], ImVec2(-1, 32)))
        {
            m_selectedTool = (ToolType)i;
        }

        if (isSelected)
        {
            ImGui::PopStyleColor();
        }

        ImGui::PopStyleColor();

        if (i < (int)ToolType::Count - 1)
        {
            ImGui::Spacing();
        }
    }

    ImGui::PopStyleColor(3);

    ImGui::Separator();
    ImGui::Text("Rung Info");
    ImGui::Text("Rungs: %d  Cols: %d", m_rungCount, m_visibleCols);
    ImGui::Text("Zoom: %.0f%%", m_zoom * 100);
    if (ImGui::Button("+ Add Rung", ImVec2(-1, 28)))
    {
        int insertAfter = (m_selectedRung >= 0 && m_selectedRung < m_rungCount) ? m_selectedRung : (m_rungCount - 1);
        for (auto& e : m_elements)
            if (e.rung > insertAfter)
                ++e.rung;
        ++m_rungCount;
        m_selectedRung = insertAfter + 1;
    }
    if (ImGui::Button("- Remove Selected Rung", ImVec2(-1, 28)) && m_rungCount > 1)
    {
        int target = (m_selectedRung >= 0 && m_selectedRung < m_rungCount) ? m_selectedRung : (m_rungCount - 1);
        for (auto it = m_elements.begin(); it != m_elements.end(); )
        {
            if (it->rung == target)
            {
                it = m_elements.erase(it);
            } else
            {
                if (it->rung > target) --it->rung;
                ++it;
            }
        }
        --m_rungCount;
        m_selectedRung = (target < m_rungCount) ? target : (m_rungCount - 1);
    }
}

void LadderEditor::RenderStatusBar()
{
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
                                   float radius, ImU32 color)
{
    float z = m_zoom;
    float startX = canvasPos.x + fmodf(m_canvasScroll.x, spacing);
    float startY = canvasPos.y + fmodf(m_canvasScroll.y, spacing);

    for (float x = startX; x < canvasPos.x + canvasSize.x; x += spacing * z)
    {
        for (float y = startY; y < canvasPos.y + canvasSize.y; y += spacing * z)
        {
            drawList->AddCircleFilled(ImVec2(x, y), radius * z, color);
        }
    }
}

void LadderEditor::DrawElementPreview(ImDrawList* drawList, ImVec2 center,
                                       float size, ToolType type, ImU32 color,
                                       const LadderElement* elem)
{
    float half = size * 0.5f;
    switch (type)
    {
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
        {
            float rh = half * 2.0f;
            drawList->AddRect(ImVec2(center.x - half * 2.0f, center.y - rh),
                              ImVec2(center.x + half * 2.0f, center.y + rh),
                              color, 0.0f, 0, 2.0f);
            const char* timerLabels[] = { "TON", "TOF", "TP" };
            int ti = elem ? elem->timerType : 0;
            if (ti < 0 || ti > 2) ti = 0;
            const char* lab = timerLabels[ti];
            ImVec2 ls = ImGui::CalcTextSize(lab);
            float lh = rh * 0.9f / ls.y;
            drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize() * lh,
                ImVec2(center.x - ls.x * lh * 0.5f, center.y - rh + 2),
                color, lab);
            if (elem)
            {
                char val[32];
                snprintf(val, sizeof(val), "%.1fs", elem->timerPreset);
                ImVec2 vs = ImGui::CalcTextSize(val);
                float vh = rh * 0.9f / vs.y;
                drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize() * vh,
                    ImVec2(center.x - vs.x * vh * 0.5f,
                           center.y + rh - vs.y * vh - 2),
                    color, val);
            }
        }
        break;
    case ToolType::Counter:
        drawList->AddRect(ImVec2(center.x - half, center.y - half * 0.6f),
                          ImVec2(center.x + half, center.y + half * 0.6f),
                          color, 0.0f, 0, 2.0f);
        {
            const char* txt = "CTU";
            ImVec2 ts = ImGui::CalcTextSize(txt);
            float scale = (half * 1.2f) / ts.y * 0.7f;
            drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize() * scale,
                ImVec2(center.x - ts.x * scale * 0.5f, center.y - ts.y * scale * 0.5f),
                color, txt);
        }
        break;

    case ToolType::Branch:
    case ToolType::BranchUp:
        // full shape drawn inline in RenderCanvas
        drawList->AddCircle(center, 3.0f, color, 0, 1.5f);
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
