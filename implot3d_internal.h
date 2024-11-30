//--------------------------------------------------
// ImPlot3D v0.1
// implot3d_internal.h
// Date: 2024-11-17
// Author: Breno Cunha Queiroz (brenocq.com)
//
// Acknowledgments:
//  ImPlot3D is heavily inspired by ImPlot
//  (https://github.com/epezent/implot) by Evan Pezent,
//  and follows a similar code style and structure to
//  maintain consistency with ImPlot's API.
//--------------------------------------------------

// Table of Contents:
// [SECTION] Constants
// [SECTION] Generic Helpers
// [SECTION] Forward Declarations
// [SECTION] Callbacks
// [SECTION] Structs
// [SECTION] Context Pointer
// [SECTION] Context Utils
// [SECTION] Style Utils
// [SECTION] Item Utils
// [SECTION] Plot Utils
// [SECTION] Setup Utils
// [SECTION] Formatter
// [SECTION] Locator

#pragma once

#ifndef IMPLOT3D_VERSION
#include "implot3d.h"
#endif

#ifndef IMGUI_DISABLE
#include "imgui_internal.h"

//-----------------------------------------------------------------------------
// [SECTION] Constants
//-----------------------------------------------------------------------------

// Default label format for axis labels
#define IMPLOT3D_LABEL_FORMAT "%g"
// Max character size for tick labels
#define IMPLOT3D_LABEL_MAX_SIZE 32

//-----------------------------------------------------------------------------
// [SECTION] Generic Helpers
//-----------------------------------------------------------------------------

#ifndef IMPLOT_VERSION
// Define generic helpers if ImPlot didn't define them already

// Computes the common (base-10) logarithm
static inline float ImLog10(float x) { return log10f(x); }
// Returns true if flag is set
template <typename TSet, typename TFlag>
static inline bool ImHasFlag(TSet set, TFlag flag) { return (set & flag) == flag; }
// Returns true if val is NAN
static inline bool ImNan(double val) { return isnan(val); }
// Returns true if val is NAN or INFINITY
static inline bool ImNanOrInf(double val) { return !(val >= -DBL_MAX && val <= DBL_MAX) || ImNan(val); }
// True if two numbers are approximately equal using units in the last place.
static inline bool ImAlmostEqual(double v1, double v2, int ulp = 2) { return ImAbs(v1 - v2) < DBL_EPSILON * ImAbs(v1 + v2) * ulp || ImAbs(v1 - v2) < DBL_MIN; }
// Set alpha channel of 32-bit color from float in range [0.0 1.0]
static inline ImU32 ImAlphaU32(ImU32 col, float alpha) {
    return col & ~((ImU32)((1.0f - alpha) * 255) << IM_COL32_A_SHIFT);
}
// Mix color a and b by factor s in [0 256]
static inline ImU32 ImMixU32(ImU32 a, ImU32 b, ImU32 s) {
#ifdef IMPLOT_MIX64
    const ImU32 af = 256 - s;
    const ImU32 bf = s;
    const ImU64 al = (a & 0x00ff00ff) | (((ImU64)(a & 0xff00ff00)) << 24);
    const ImU64 bl = (b & 0x00ff00ff) | (((ImU64)(b & 0xff00ff00)) << 24);
    const ImU64 mix = (al * af + bl * bf);
    return ((mix >> 32) & 0xff00ff00) | ((mix & 0xff00ff00) >> 8);
#else
    const ImU32 af = 256 - s;
    const ImU32 bf = s;
    const ImU32 al = (a & 0x00ff00ff);
    const ImU32 ah = (a & 0xff00ff00) >> 8;
    const ImU32 bl = (b & 0x00ff00ff);
    const ImU32 bh = (b & 0xff00ff00) >> 8;
    const ImU32 ml = (al * af + bl * bf);
    const ImU32 mh = (ah * af + bh * bf);
    return (mh & 0xff00ff00) | ((ml & 0xff00ff00) >> 8);
#endif
}
#endif

//-----------------------------------------------------------------------------
// [SECTION] Forward Declarations
//-----------------------------------------------------------------------------

struct ImPlot3DTicker;

//------------------------------------------------------------------------------
// [SECTION] Callbacks
//------------------------------------------------------------------------------

typedef void (*ImPlot3DLocator)(ImPlot3DTicker& ticker, const ImPlot3DRange& range, ImPlot3DFormatter formatter, void* formatter_data);

//-----------------------------------------------------------------------------
// [SECTION] Structs
//-----------------------------------------------------------------------------

struct ImPlot3DNextItemData {
    ImVec4 Colors[3]; // ImPlot3DCol_Line, ImPlot3DCol_MarkerOutline, ImPlot3DCol_MarkerFill,
    float LineWeight;
    ImPlot3DMarker Marker;
    float MarkerSize;
    float MarkerWeight;
    float FillAlpha;
    bool RenderLine;
    bool RenderMarkerLine;
    bool RenderMarkerFill;
    bool Hidden;

    ImPlot3DNextItemData() { Reset(); }

    void Reset() {
        for (int i = 0; i < 3; i++)
            Colors[i] = IMPLOT3D_AUTO_COL;
        LineWeight = IMPLOT3D_AUTO;
        Marker = IMPLOT3D_AUTO;
        MarkerSize = IMPLOT3D_AUTO;
        MarkerWeight = IMPLOT3D_AUTO;
        FillAlpha = IMPLOT3D_AUTO;
        RenderLine = false;
        RenderMarkerLine = true;
        RenderMarkerFill = true;
        Hidden = false;
    }
};

// Colormap data storage
struct ImPlot3DColormapData {
    ImVector<ImU32> Keys;
    ImVector<int> KeyCounts;
    ImVector<int> KeyOffsets;
    ImVector<ImU32> Tables;
    ImVector<int> TableSizes;
    ImVector<int> TableOffsets;
    ImGuiTextBuffer Text;
    ImVector<int> TextOffsets;
    ImVector<bool> Quals;
    ImGuiStorage Map;
    int Count;

    ImPlot3DColormapData() { Count = 0; }

    int Append(const char* name, const ImU32* keys, int count, bool qual) {
        if (GetIndex(name) != -1)
            return -1;
        KeyOffsets.push_back(Keys.size());
        KeyCounts.push_back(count);
        Keys.reserve(Keys.size() + count);
        for (int i = 0; i < count; ++i)
            Keys.push_back(keys[i]);
        TextOffsets.push_back(Text.size());
        Text.append(name, name + strlen(name) + 1);
        Quals.push_back(qual);
        ImGuiID id = ImHashStr(name);
        int idx = Count++;
        Map.SetInt(id, idx);
        _AppendTable(idx);
        return idx;
    }

    void _AppendTable(ImPlot3DColormap cmap) {
        int key_count = GetKeyCount(cmap);
        const ImU32* keys = GetKeys(cmap);
        int off = Tables.size();
        TableOffsets.push_back(off);
        if (IsQual(cmap)) {
            Tables.reserve(key_count);
            for (int i = 0; i < key_count; ++i)
                Tables.push_back(keys[i]);
            TableSizes.push_back(key_count);
        } else {
            int max_size = 255 * (key_count - 1) + 1;
            Tables.reserve(off + max_size);
            // ImU32 last = keys[0];
            // Tables.push_back(last);
            // int n = 1;
            for (int i = 0; i < key_count - 1; ++i) {
                for (int s = 0; s < 255; ++s) {
                    ImU32 a = keys[i];
                    ImU32 b = keys[i + 1];
                    ImU32 c = ImMixU32(a, b, s);
                    // if (c != last) {
                    Tables.push_back(c);
                    // last = c;
                    // n++;
                    // }
                }
            }
            ImU32 c = keys[key_count - 1];
            // if (c != last) {
            Tables.push_back(c);
            // n++;
            // }
            // TableSizes.push_back(n);
            TableSizes.push_back(max_size);
        }
    }

    void RebuildTables() {
        Tables.resize(0);
        TableSizes.resize(0);
        TableOffsets.resize(0);
        for (int i = 0; i < Count; ++i)
            _AppendTable(i);
    }

    inline bool IsQual(ImPlot3DColormap cmap) const { return Quals[cmap]; }
    inline const char* GetName(ImPlot3DColormap cmap) const { return cmap < Count ? Text.Buf.Data + TextOffsets[cmap] : nullptr; }
    inline ImPlot3DColormap GetIndex(const char* name) const {
        ImGuiID key = ImHashStr(name);
        return Map.GetInt(key, -1);
    }

    inline const ImU32* GetKeys(ImPlot3DColormap cmap) const { return &Keys[KeyOffsets[cmap]]; }
    inline int GetKeyCount(ImPlot3DColormap cmap) const { return KeyCounts[cmap]; }
    inline ImU32 GetKeyColor(ImPlot3DColormap cmap, int idx) const { return Keys[KeyOffsets[cmap] + idx]; }
    inline void SetKeyColor(ImPlot3DColormap cmap, int idx, ImU32 value) {
        Keys[KeyOffsets[cmap] + idx] = value;
        RebuildTables();
    }

    inline const ImU32* GetTable(ImPlot3DColormap cmap) const { return &Tables[TableOffsets[cmap]]; }
    inline int GetTableSize(ImPlot3DColormap cmap) const { return TableSizes[cmap]; }
    inline ImU32 GetTableColor(ImPlot3DColormap cmap, int idx) const { return Tables[TableOffsets[cmap] + idx]; }

    inline ImU32 LerpTable(ImPlot3DColormap cmap, float t) const {
        int off = TableOffsets[cmap];
        int siz = TableSizes[cmap];
        int idx = Quals[cmap] ? ImClamp((int)(siz * t), 0, siz - 1) : (int)((siz - 1) * t + 0.5f);
        return Tables[off + idx];
    }
};

// State information for plot items
struct ImPlot3DItem {
    ImGuiID ID;
    ImU32 Color;
    int NameOffset;
    bool Show;
    bool LegendHovered;

    ImPlot3DItem() {
        ID = 0;
        Color = IM_COL32_WHITE;
        NameOffset = -1;
        Show = true;
        LegendHovered = false;
    }
    ~ImPlot3DItem() { ID = 0; }
};

// Holds legend state
struct ImPlot3DLegend {
    ImPlot3DLegendFlags Flags;
    ImPlot3DLocation Location;
    ImVector<int> Indices;
    ImGuiTextBuffer Labels;
    ImRect Rect;
    bool Hovered;
    bool Held;

    ImPlot3DLegend() {
        Flags = ImPlot3DLegendFlags_None;
        Hovered = Held = false;
        Location = ImPlot3DLocation_NorthWest;
    }

    void Reset() {
        Indices.shrink(0);
        Labels.Buf.shrink(0);
    }
};

// Holds items
struct ImPlot3DItemGroup {
    ImPool<ImPlot3DItem> ItemPool;
    ImPlot3DLegend Legend;
    int ColormapIdx;

    ImPlot3DItemGroup() {
        ColormapIdx = 0;
    }

    int GetItemCount() const { return ItemPool.GetBufSize(); }
    ImGuiID GetItemID(const char* label_id) { return ImGui::GetID(label_id); }
    ImPlot3DItem* GetItem(ImGuiID id) { return ItemPool.GetByKey(id); }
    ImPlot3DItem* GetItem(const char* label_id) { return GetItem(GetItemID(label_id)); }
    ImPlot3DItem* GetOrAddItem(ImGuiID id) { return ItemPool.GetOrAddByKey(id); }
    ImPlot3DItem* GetItemByIndex(int i) { return ItemPool.GetByIndex(i); }
    int GetItemIndex(ImPlot3DItem* item) { return ItemPool.GetIndex(item); }
    int GetLegendCount() const { return Legend.Indices.size(); }
    ImPlot3DItem* GetLegendItem(int i) { return ItemPool.GetByIndex(Legend.Indices[i]); }
    const char* GetLegendLabel(int i) { return Legend.Labels.Buf.Data + GetLegendItem(i)->NameOffset; }
    void Reset() {
        ItemPool.Clear();
        Legend.Reset();
        ColormapIdx = 0;
    }
};

// Tick mark info
struct ImPlot3DTick {
    float PlotPos;
    bool Major;
    bool ShowLabel;
    ImVec2 LabelSize;
    int TextOffset;
    int Idx;

    ImPlot3DTick(double value, bool major, bool show_label) {
        PlotPos = value;
        Major = major;
        ShowLabel = show_label;
        TextOffset = -1;
    }
};

// Collection of ticks
struct ImPlot3DTicker {
    ImVector<ImPlot3DTick> Ticks;
    ImGuiTextBuffer TextBuffer;

    ImPlot3DTicker() {
        Reset();
    }

    ImPlot3DTick& AddTick(double value, bool major, bool show_label, const char* label) {
        ImPlot3DTick tick(value, major, show_label);
        if (show_label && label != nullptr) {
            tick.TextOffset = TextBuffer.size();
            TextBuffer.append(label, label + strlen(label) + 1);
            tick.LabelSize = ImGui::CalcTextSize(TextBuffer.Buf.Data + tick.TextOffset);
        }
        return AddTick(tick);
    }

    ImPlot3DTick& AddTick(double value, bool major, bool show_label, ImPlot3DFormatter formatter, void* data) {
        ImPlot3DTick tick(value, major, show_label);
        if (show_label && formatter != nullptr) {
            char buff[IMPLOT3D_LABEL_MAX_SIZE];
            tick.TextOffset = TextBuffer.size();
            formatter(tick.PlotPos, buff, sizeof(buff), data);
            TextBuffer.append(buff, buff + strlen(buff) + 1);
            tick.LabelSize = ImGui::CalcTextSize(TextBuffer.Buf.Data + tick.TextOffset);
        }
        return AddTick(tick);
    }

    inline ImPlot3DTick& AddTick(ImPlot3DTick tick) {
        tick.Idx = Ticks.size();
        Ticks.push_back(tick);
        return Ticks.back();
    }

    const char* GetText(int idx) const {
        return TextBuffer.Buf.Data + Ticks[idx].TextOffset;
    }

    const char* GetText(const ImPlot3DTick& tick) const {
        return GetText(tick.Idx);
    }

    void Reset() {
        Ticks.shrink(0);
        TextBuffer.Buf.shrink(0);
    }

    int TickCount() const {
        return Ticks.Size;
    }
};

// Holds axis information
struct ImPlot3DAxis {
    ImPlot3DAxisFlags Flags;
    ImPlot3DRange Range;
    int LabelOffset;
    // Ticks
    ImPlot3DTicker Ticker;
    ImPlot3DFormatter Formatter;
    void* FormatterData;
    ImPlot3DLocator Locator;
    // Fit data
    bool FitThisFrame;
    ImPlot3DRange FitExtents;

    // Constructor
    ImPlot3DAxis() {
        Flags = ImPlot3DAxisFlags_None;
        Range.Min = 0.0f;
        Range.Max = 1.0f;
        LabelOffset = -1;
        // Ticks
        Formatter = nullptr;
        FormatterData = nullptr;
        Locator = nullptr;
        // Fit data
        FitThisFrame = true;
        FitExtents.Min = HUGE_VAL;
        FitExtents.Max = -HUGE_VAL;
    }

    bool HasLabel() const;
    void ExtendFit(float value);
    void ApplyFit();
    float PlotToNDC(float value) const;
    float NDCToPlot(float value) const;
};

// Holds plot state information that must persist after EndPlot
struct ImPlot3DPlot {
    ImGuiID ID;
    ImPlot3DFlags Flags;
    ImGuiTextBuffer TextBuffer;
    // Bounding rectangles
    ImRect FrameRect;  // Outermost bounding rectangle that encapsulates whole the plot/title/padding/etc
    ImRect CanvasRect; // Frame rectangle reduced by padding
    ImRect PlotRect;   // Bounding rectangle for the actual plot area
    // Rotation and range
    ImPlot3DQuat Rotation;
    ImPlot3DAxis Axes[3];
    // User input
    bool SetupLocked;
    bool Hovered;
    bool Held;
    // Fit data
    bool FitThisFrame;
    // Items
    ImPlot3DItemGroup Items;
    ImPlot3DItem* CurrentItem;

    ImPlot3DPlot() {
        Flags = ImPlot3DFlags_None;
        Rotation = ImPlot3DQuat(0.0f, 0.0f, 0.0f, 1.0f);
        for (int i = 0; i < 3; i++)
            Axes[i] = ImPlot3DAxis();
        SetupLocked = false;
        Hovered = Held = false;
        FitThisFrame = true;
        CurrentItem = nullptr;
    }

    void ExtendFit(const ImPlot3DPoint& point);
    ImPlot3DPoint RangeMin() const;
    ImPlot3DPoint RangeMax() const;
    ImPlot3DPoint RangeCenter() const;
    void SetRange(const ImPlot3DPoint& min, const ImPlot3DPoint& max);
    void SetAxisLabel(ImPlot3DAxis& axis, const char* label);
    const char* GetAxisLabel(const ImPlot3DAxis& axis) const;
};

struct ImPlot3DContext {
    ImPool<ImPlot3DPlot> Plots;
    ImPlot3DPlot* CurrentPlot;
    ImPlot3DItemGroup* CurrentItems;
    ImPlot3DNextItemData NextItemData;
    ImPlot3DStyle Style;
    ImVector<ImGuiStyleMod> StyleModifiers;
    ImPlot3DColormapData ColormapData;
};

//-----------------------------------------------------------------------------
// [SECTION] Context Pointer
//-----------------------------------------------------------------------------

namespace ImPlot3D {

#ifndef GImPlot3D
extern IMPLOT3D_API ImPlot3DContext* GImPlot3D; // Current context pointer
#endif

//-----------------------------------------------------------------------------
// [SECTION] Context Utils
//-----------------------------------------------------------------------------

IMPLOT3D_API void InitializeContext(ImPlot3DContext* ctx); // Initialize ImPlot3DContext
IMPLOT3D_API void ResetContext(ImPlot3DContext* ctx);      // Reset ImPlot3DContext

//-----------------------------------------------------------------------------
// [SECTION] Style Utils
//-----------------------------------------------------------------------------

IMPLOT3D_API bool IsColorAuto(const ImVec4& col);
IMPLOT3D_API bool IsColorAuto(ImPlot3DCol idx);
IMPLOT3D_API ImVec4 GetAutoColor(ImPlot3DCol idx);
IMPLOT3D_API const char* GetStyleColorName(ImPlot3DCol idx);

// Get styling data for next item (call between BeginItem/EndItem)
IMPLOT3D_API const ImPlot3DNextItemData& GetItemData();

// Returns a color from the Color map given an index >= 0 (modulo will be performed)
IMPLOT3D_API ImU32 GetColormapColorU32(int idx, ImPlot3DColormap cmap);

// Returns the next unused colormap color and advances the colormap. Can be used to skip colors if desired
IMPLOT3D_API ImU32 NextColormapColorU32();

//-----------------------------------------------------------------------------
// [SECTION] Item Utils
//-----------------------------------------------------------------------------

IMPLOT3D_API bool BeginItem(const char* label_id, ImPlot3DItemFlags flags = 0, ImPlot3DCol recolor_from = IMPLOT3D_AUTO);
IMPLOT3D_API void EndItem();

// Register or get an existing item from the current plot
IMPLOT3D_API ImPlot3DItem* RegisterOrGetItem(const char* label_id, ImPlot3DItemFlags flags, bool* just_created = nullptr);
// Get a plot item from the current plot
IMPLOT3D_API ImPlot3DItem* GetItem(const char* label_id);
// Get the current item
IMPLOT3D_API ImPlot3DItem* GetCurrentItem();

//-----------------------------------------------------------------------------
// [SECTION] Plot Utils
//-----------------------------------------------------------------------------

// Gets the current plot from the current ImPlot3DContext
IMPLOT3D_API ImPlot3DPlot* GetCurrentPlot();

// Convert a position in the current plot's coordinate system to the current plot's normalized device coordinate system (NDC)
// When the cube aspect ratio is [1,1,1], the NDC varies from [-0.5, 0.5] in each axis
IMPLOT3D_API ImPlot3DPoint PlotToNDC(const ImPlot3DPoint& point);
IMPLOT3D_API ImPlot3DPoint NDCToPlot(const ImPlot3DPoint& point);
// Convert a position in the current plot's NDC to pixels
IMPLOT3D_API ImVec2 NDCToPixels(const ImPlot3DPoint& point);
// Convert a pixel coordinate to a ray in the NDC
IMPLOT3D_API ImPlot3DRay PixelsToNDCRay(const ImVec2& pix);
// Convert a ray in the NDC to a ray in the current plot's coordinate system
IMPLOT3D_API ImPlot3DRay NDCRayToPlotRay(const ImPlot3DRay& ray);

//-----------------------------------------------------------------------------
// [SECTION] Setup Utils
//-----------------------------------------------------------------------------

IMPLOT3D_API void SetupLock();

//-----------------------------------------------------------------------------
// [SECTION] Formatter
//-----------------------------------------------------------------------------

int Formatter_Default(float value, char* buff, int size, void* data);

//------------------------------------------------------------------------------
// [SECTION] Locator
//------------------------------------------------------------------------------

void Locator_Default(ImPlot3DTicker& ticker, const ImPlot3DRange& range, ImPlot3DFormatter formatter, void* formatter_data);

} // namespace ImPlot3D

#endif // #ifndef IMGUI_DISABLE
