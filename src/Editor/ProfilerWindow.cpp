#include "ProfilerWindow.hpp"
#include "imgui.h"
#include "implot.h"
#include "Profiler.hpp"

using namespace BB;

void BB::ImGuiShowProfiler()
{
	if (ImGui::Begin("Profile Info", nullptr, ImGuiWindowFlags_MenuBar))
	{
		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("Menu"))
			{

				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}
		const ConstSlice<ProfileResult> profile_results = GetProfileResultsList();
		for (size_t i = 0; i < profile_results.size(); i++)
		{
			if (ImGui::CollapsingHeader(profile_results[i].name.c_str()))
			{
				ImGui::Text("Average Time in miliseconds: %.6f", profile_results[i].average_time);
				ImGui::PushID(i);
				if (ImPlot::BeginPlot("##NoTitle", ImVec2(-1, 150))) {
					ImPlot::SetupAxis(ImAxis_X1, nullptr, ImPlotAxisFlags_NoTickLabels);
					ImPlot::SetupAxisLimits(ImAxis_X1, 0, PROFILE_RESULT_HISTORY_BUFFER_SIZE, ImGuiCond_Always);
					ImPlot::PlotLine("Time in miliseconds", profile_results[i].history.data(), static_cast<int>(profile_results[i].history.size()));
					ImPlot::EndPlot();
				}

				ImGui::Text("File: %s", profile_results[i].file);
				ImGui::Text("Line: %u", profile_results[i].line);
				ImGui::PopID();
			}
		}
	}

	ImGui::End();
}
