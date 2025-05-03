#include "watcher.h"
#include <windows.h>

char g_watcher_memory[512];
OVERLAPPED g_watcher_overlapped;
HANDLE g_watcher_dir;

void watcher_init(const char* directory_path)
{
	g_watcher_dir = CreateFileA(
				   directory_path,           // pointer to the file name
				   FILE_LIST_DIRECTORY,    // access (read/write) mode
				   FILE_SHARE_READ         // share mode
				   | FILE_SHARE_WRITE
				   | FILE_SHARE_DELETE,
				   NULL, // security descriptor
				   OPEN_EXISTING,         // how to create
				   FILE_FLAG_BACKUP_SEMANTICS // file attributes
				   | FILE_FLAG_OVERLAPPED,
				   NULL);                 // file with attributes to copy

	
	BOOL res = ReadDirectoryChangesW(g_watcher_dir,
				   g_watcher_memory,
				   sizeof(g_watcher_memory),
				   FALSE,
				   FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION | FILE_NOTIFY_CHANGE_FILE_NAME,
				   NULL,
				   &g_watcher_overlapped,
				   NULL);
	assert(res);
}

bool watcher_tick()
{
	bool atleast_one_change = false;
	if (HasOverlappedIoCompleted(&g_watcher_overlapped)) {
		char *buffer = g_watcher_memory;
		for (;;) {
			char ascii_path[512] = {0};
			
			FILE_NOTIFY_INFORMATION* fni = (FILE_NOTIFY_INFORMATION*)buffer;
			wchar_t const* wide_chars = fni->FileName;
			uint32_t wide_chars_length = fni->FileNameLength / sizeof(wchar_t);

			int path_length = WideCharToMultiByte (CP_UTF8,
							       0,
							       fni->FileName,
							       wide_chars_length,
							       ascii_path,
							       sizeof(ascii_path),
							       NULL,
							       NULL);
			fprintf(stderr, "change: %s\n", ascii_path);
			atleast_one_change = true;
			if (!fni->NextEntryOffset)
				break;
			buffer += fni->NextEntryOffset;
		}

		BOOL res = ReadDirectoryChangesW(g_watcher_dir,
						 g_watcher_memory,
						 sizeof(g_watcher_memory),
						 FALSE,
						 FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION | FILE_NOTIFY_CHANGE_FILE_NAME,
						 NULL,
						 &g_watcher_overlapped,
						 NULL);
		assert(res);		
	}
	return atleast_one_change;
}
