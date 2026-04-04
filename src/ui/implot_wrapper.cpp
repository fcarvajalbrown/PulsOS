#include "implot_wrapper.h"
#include <implot.h>

// only this file includes implot — C files include implot_wrapper.h only
extern "C" void implot_sparkline(const char *label, const float *values,
                                 int count, int offset,
                                 float width, float height) {
    if (!values || count <= 0) return;

    if (ImPlot::BeginPlot(label,
            ImVec2(width, height),
            ImPlotFlags_NoTitle   |
            ImPlotFlags_NoLegend  |
            ImPlotFlags_NoMenus   |
            ImPlotFlags_NoBoxSelect)) {

        ImPlot::SetupAxes(nullptr, nullptr,
            ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_NoTickMarks,
            ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_NoTickMarks);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 100, ImPlotCond_Always);

        ImPlotSpec spec;
        spec.Offset = offset;
        ImPlot::PlotLine(label, values, count, 1.0, 0.0, spec);

        ImPlot::EndPlot();
    }
}