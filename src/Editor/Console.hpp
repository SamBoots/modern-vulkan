#pragma once
#include "Common.h"
#include "MemoryArena.hpp"
#include <atomic>
#include "AssetLoader.hpp"

namespace BB
{
	struct ConsoleCreateInfo
	{
		uint32_t entries_till_resize;
		uint32_t entries_till_file_write;
		WarningTypeFlags enabled_warnings;
		bool write_to_console;
		bool write_to_file;
	};

	class Console
	{
	public:
		void Init(const ConsoleCreateInfo& a_create_info);

		void ImGuiShowConsole(MemoryArena& a_arena, const uint2 a_window_size);

		static void LoggerCallback(const char* a_file_name, int a_line, const WarningType a_warning_type, const char* a_formats, void* a_puserdata, va_list a_args);
		static void LoggerToFile(MemoryArena& a_arena, void* a_puserdata);

	private:
		MemoryArena m_arena;
		BBRWLock m_lock; // possible get a lockfree arena
		WarningTypeFlags m_enabled_logs;

		std::atomic<uint32_t >m_entries_till_write_to_file;
		std::atomic<uint32_t> m_entries_after_last_write;

		OSFileHandle m_log_file;

		struct ConsoleEntry
		{
			StackString<2048> message;
			WarningType warning_type;
			PathString file_name;
			int line;
		};

		std::atomic<size_t> m_entry_count;
		std::atomic<size_t> m_entry_commit_limit;
		size_t m_last_written_entry;
		ConsoleEntry* m_entry_start;

		std::atomic<bool> m_writing_to_file;
		bool m_write_to_console;
		bool m_write_to_file;

		// constants
		float m_popup_window_x_size_factor = 1.3f;
	};
}
