#pragma once
#include <string>
#include <windows.h> 

namespace HEIN
{
	class EditorUtils
	{
	public:
		static std::wstring SelectFolder(HWND owner);
		static std::wstring OpenFileDialog(const wchar_t* filter, HWND owner);
		static std::wstring MakeRelativePath(const std::wstring& absolutePath);
		static std::wstring ImportFontFileToResources(const std::wstring& sourceFilePath);
		static bool DoesFontFileExistOnDisk(const std::wstring& path);
	};
}
