#include "ProfilerWindow.hpp"
#include "imgui.h"
#include "implot.h"
#include "Profiler.hpp"

using namespace BB;

void BB::ImGuiShowProfiler(MemoryArenaTemp temp_arena)
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
				ImGui::PushID(static_cast<int>(i));
				if (ImPlot::BeginPlot("##NoTitle", ImVec2(-1, 150))) {
					const StaticArray<double> history_buffer = CountingRingBufferLinear(temp_arena, profile_results[i].history_buffer);

					ImPlot::SetupAxis(ImAxis_X1, nullptr, ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_NoTickLabels);
					ImPlot::PlotShaded("Time in miliseconds", history_buffer.data(), static_cast<int>(history_buffer.size()));
					ImPlot::EndPlot();
				}

				ImGui::Text("File: %s", profile_results[i].file);
				ImGui::Text("Line: %d", profile_results[i].line);
				ImGui::PopID();
			}
		}
	}

	ImGui::End();
}
