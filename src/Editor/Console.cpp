#include "Console.hpp"
#include "imgui.h"
#include "Program.h"
#include "BBThreadScheduler.hpp"

using namespace BB;

constexpr const char* CONSOLE_FILE_NAME = "console_log.txt";
constexpr ImVec4 WARNING_COLOR_TYPE[] =
{
	ImVec4(0.f, 0.f, 1.f, 1.f),
	ImVec4(0.f, 1.f, 0.f, 1.f),
	ImVec4(1.f, 1.f, 0.f, 1.f),
	ImVec4(1.f, 1.f, 0.f, 1.f),
	ImVec4(1.f, 1.f, 0.f, 1.f),
	ImVec4(1.f, 0.f, 0.f, 1.f),
};

static bool IsWarningSet(const WarningType a_type, const WarningTypeFlags a_flags)
{
	return (a_flags & static_cast<WarningTypeFlags>(a_type)) == static_cast<WarningTypeFlags>(a_type);
}

void Console::Init(const ConsoleCreateInfo& a_create_info)
{
	m_arena = MemoryArenaCreate();
	m_lock = OSCreateRWLock();
	m_enabled_logs = a_create_info.enabled_warnings;
	m_entries_till_write_to_file = a_create_info.entries_till_resize;
	m_entry_commit_limit = a_create_info.entries_till_file_write;
	m_write_to_console = a_create_info.write_to_console;
	m_write_to_file = a_create_info.write_to_file;

	m_entry_start = reinterpret_cast<Console::ConsoleEntry*>(ArenaAlloc(
		m_arena,
		sizeof(Console::ConsoleEntry) * m_entry_commit_limit,
		__alignof(Console::ConsoleEntry)));

	m_writing_to_file = false;
	m_entry_count = 0;
	m_entries_after_last_write = 0;

	m_log_file = OSCreateFile(CONSOLE_FILE_NAME);
	Logger::LoggerSetCallback(Console::LoggerCallback, this);
}

void Console::ImGuiShowConsole(MemoryArena& a_arena, const uint2 a_window_size)
{
	if (ImGui::Begin("Console log", nullptr, ImGuiWindowFlags_MenuBar))
	{
		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("Menu"))
			{
				if (ImGui::Button("Clear console logs"))
				{
					OSAcquireSRWLockWrite(&m_lock);
					memset(m_entry_start, 0, sizeof(Console::ConsoleEntry) * m_entry_count);
					m_entry_count = 0;
					m_entries_after_last_write = 0;
					OSReleaseSRWLockWrite(&m_lock);
				}
				if (ImGui::MenuItem("Write to file"))
				{
					LoggerToFile(a_arena, this);
				}
				ImGui::SliderFloat("popup window size: %.3f", &m_popup_window_x_size_factor, 1.f, 8.f);
				if (ImGui::MenuItem("[DEBUG] add 5 entries"))
				{
					for (uint32_t i = 0; i < 5; i++)
						BB_LOG("DEBUG ENTRY");
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}

		if (ImGui::BeginTable("log enable all options", 2, ImGuiTableFlags_NoPadOuterX, ImVec2(512.f, 0.f)))
		{
			ImGui::TableNextColumn();
			if (ImGui::Button("enable all logs"))
				m_enabled_logs = WARNING_TYPES_ALL;

			ImGui::TableNextColumn();
			if (ImGui::Button("disable all logs"))
				m_enabled_logs = 0;

			ImGui::EndTable();
		}

		if (ImGui::BeginTable("log enable options", 3, 0, ImVec2(512.f, 0.f)))
		{
			ImGui::TableNextColumn();
			ImGui::CheckboxFlags("warning info", &m_enabled_logs, static_cast<WarningTypeFlags>(WarningType::INFO));

			ImGui::TableNextColumn();
			ImGui::CheckboxFlags("warning optimization", &m_enabled_logs, static_cast<WarningTypeFlags>(WarningType::OPTIMIZATION));

			ImGui::TableNextColumn();
			ImGui::CheckboxFlags("warning low", &m_enabled_logs, static_cast<WarningTypeFlags>(WarningType::LOW));

			ImGui::TableNextColumn();
			ImGui::CheckboxFlags("warning medium", &m_enabled_logs, static_cast<WarningTypeFlags>(WarningType::MEDIUM));

			ImGui::TableNextColumn();
			ImGui::CheckboxFlags("warning high", &m_enabled_logs, static_cast<WarningTypeFlags>(WarningType::HIGH));

			ImGui::TableNextColumn();
			ImGui::CheckboxFlags("asserts", &m_enabled_logs, static_cast<WarningTypeFlags>(WarningType::ASSERT));

			ImGui::EndTable();
		}
		static bool show_file_info = false;

		if (ImGui::BeginTable("Extra options console", 3, ImGuiTableFlags_NoPadOuterX, ImVec2(512.f, 0.f)))
		{
			ImGui::TableNextColumn();
			ImGui::Checkbox("file info", &show_file_info);

			ImGui::TableNextColumn();
			ImGui::Checkbox("write to console", &m_write_to_console);

			ImGui::TableNextColumn();
			ImGui::Checkbox("write to file", &m_write_to_file);

			ImGui::EndTable();
		}


		int columns = 2;
		if (show_file_info)
			columns += 2;

		if (ImGui::BeginTable("table_scrollx", columns, ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg))
		{
			ImGui::TableSetupColumn("Severity"); // Make the first column not hideable to match our use of TableSetupScrollFreeze()
			ImGui::TableSetupColumn("Message");
			if (show_file_info)
			{
				ImGui::TableSetupColumn("source file");
				ImGui::TableSetupColumn("line");
			}
			ImGui::TableHeadersRow();
			for (size_t i = 0; i < m_entry_count; i++)
			{
				ImGui::PushID(static_cast<int>(i));
				const Console::ConsoleEntry& entry = m_entry_start[i];
				static int selected = -1;
				if ((m_enabled_logs & static_cast<WarningTypeFlags>(entry.warning_type)) == static_cast<WarningTypeFlags>(entry.warning_type))
				{
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::TextColored(WARNING_COLOR_TYPE[static_cast<uint32_t>(entry.warning_type)], "[%s]", Logger::WarningTypeToCChar(entry.warning_type));
					ImGui::TableSetColumnIndex(1);

					// popup for error message
					ImGui::SetNextWindowSize(ImVec2(static_cast<float>(a_window_size.x) / m_popup_window_x_size_factor, 0.f));
					ImGui::Selectable(entry.message.c_str(), selected == static_cast<int>(i));
					if (ImGui::BeginPopupContextItem())
					{
						selected = static_cast<int>(i);
						ImGui::TextWrapped("%s", entry.message.c_str());
						ImGui::EndPopup();
					}

					if (show_file_info)
					{
						ImGui::TableSetColumnIndex(2);
						ImGui::TextUnformatted(entry.file_name.c_str());
						ImGui::TableSetColumnIndex(3);
						ImGui::Text("%d", entry.line);
					}
				}
				ImGui::PopID();
			}
			ImGui::EndTable();
		}
	}
	ImGui::End();
}

void Console::LoggerCallback(const char* a_file_name, int a_line, const WarningType a_warning_type, const char* a_str, void* a_puserdata, va_list a_args)
{
	Console::ConsoleEntry entry{};
	entry.warning_type = a_warning_type;
	entry.file_name = PathString(a_file_name);
	entry.line = a_line;

    const size_t new_size = _FormatString(entry.message.data(), entry.message.capacity(), a_str, a_args);
    if (new_size == size_t(-1))
    {
        BB_WARNING(new_size != size_t(-1), "logger message too large", WarningType::HIGH);
        return;
    }

    entry.message.RecalculateStringSize(new_size);

	Console* pconsole = reinterpret_cast<Console*>(a_puserdata);
	const size_t index = pconsole->m_entry_count.fetch_add(1);

	// TODO this if is not thread safe, double resize could happen.
	if (index >= pconsole->m_entry_commit_limit)
	{
		OSAcquireSRWLockWrite(&pconsole->m_lock);
		const size_t new_commit_limit = pconsole->m_entry_commit_limit * 2;

		pconsole->m_entry_start = reinterpret_cast<Console::ConsoleEntry*>(ArenaRealloc(
			pconsole->m_arena,
			pconsole->m_entry_start,
			sizeof(Console::ConsoleEntry) * pconsole->m_entry_commit_limit,
			sizeof(Console::ConsoleEntry) * new_commit_limit,
			__alignof(Console::ConsoleEntry)));

		pconsole->m_entry_commit_limit = new_commit_limit;
		OSReleaseSRWLockWrite(&pconsole->m_lock);
	}
	pconsole->m_entry_start[index] = entry;

	// check if write to file
	const uint32_t entries_till_write = pconsole->m_entries_after_last_write.fetch_add(1);
	// TODO this if is not thread safe
	if (entries_till_write == pconsole->m_entries_till_write_to_file && !pconsole->m_writing_to_file && pconsole->m_write_to_file)
	{
		Threads::StartTaskThread(LoggerToFile, pconsole, sizeof(Console), L"logger to file");
	}

	if (IsWarningSet(a_warning_type, pconsole->m_enabled_logs) && pconsole->m_write_to_console)
		Logger::LogToConsole(entry.file_name.c_str(), entry.line, entry.warning_type, entry.message.c_str());
}

void Console::LoggerToFile(MemoryArena& a_arena, void* a_puserdata)
{
	Console* pconsole = reinterpret_cast<Console*>(a_puserdata);

	bool is_writing = false;
	if (pconsole->m_writing_to_file.compare_exchange_strong(is_writing, true))
	{
		return;
	}
	const size_t write_end = pconsole->m_entry_count;
	pconsole->m_entries_after_last_write = 0;
	MemoryArenaScope(a_arena)
	{
		// way to big, TODO, add a resize function that uses realloc
		String massive_string(a_arena, mbSize * 32);
		OSAcquireSRWLockRead(&pconsole->m_lock);
		// put it all in a massive string so that we only have one I/O operation.
		for (size_t i = pconsole->m_last_written_entry; i < write_end; i++)
		{
			constexpr const char LOG_MESSAGE_ERROR_LEVEL_0[]{ "Severity: " };
			constexpr const char LOG_MESSAGE_FILE_0[]{ "\nFile: " };
			constexpr const char LOG_MESSAGE_LINE_NUMBER_1[]{ "\nLine Number: " };
			constexpr const char LOG_MESSAGE_MESSAGE_TXT_2[]{ "\nThe Message: " };

			const Console::ConsoleEntry& entry = pconsole->m_entry_start[i];

			//Start with the warning level
			massive_string.append(LOG_MESSAGE_ERROR_LEVEL_0, sizeof(LOG_MESSAGE_ERROR_LEVEL_0) - 1);
			massive_string.append(Logger::WarningTypeToCChar(entry.warning_type));
			//Get the file.
			massive_string.append(LOG_MESSAGE_FILE_0, sizeof(LOG_MESSAGE_FILE_0) - 1);
			massive_string.append(entry.file_name.c_str(), entry.file_name.size());

			//Get the line number into the buffer
			char lineNumString[8]{};
			sprintf_s(lineNumString, 8, "%u", entry.line);

			massive_string.append(LOG_MESSAGE_LINE_NUMBER_1, sizeof(LOG_MESSAGE_LINE_NUMBER_1) - 1);
			massive_string.append(lineNumString);

			//Get the message(s).
			massive_string.append(LOG_MESSAGE_MESSAGE_TXT_2, sizeof(LOG_MESSAGE_MESSAGE_TXT_2) - 1);
			massive_string.append(entry.message.c_str(), entry.message.size());
			//double line ending for better reading.
			massive_string.append("\n\n", 2);
		}
		OSReleaseSRWLockRead(&pconsole->m_lock);
		bool success = OSWriteFile(pconsole->m_log_file, massive_string.data(), massive_string.size());
		BB_ASSERT(success, "failed to write file to log");
	}
	pconsole->m_last_written_entry = false;
}
