#include "pch.h"
#include "EditorUtils.h"
#include <windows.h>
#include <commdlg.h>
#include <shobjidl.h>
#include <filesystem>

namespace HEIN
{
	std::wstring EditorUtils::SelectFolder(HWND owner)
	{
		std::wstring folderPath;
		IFileOpenDialog* pFileOpen = nullptr;
		HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

		if (SUCCEEDED(hr))
		{
			DWORD dwOptions;
			if (SUCCEEDED(pFileOpen->GetOptions(&dwOptions))) pFileOpen->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR);

			if (SUCCEEDED(pFileOpen->Show(owner))) // <--- Uses the passed HWND
			{
				IShellItem* pItem;
				if (SUCCEEDED(pFileOpen->GetResult(&pItem)))
				{
					PWSTR pszFilePath;
					if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath)))
					{
						folderPath = pszFilePath;
						CoTaskMemFree(pszFilePath);
					}
					pItem->Release();
				}
			}
			pFileOpen->Release();
		}
		return folderPath;
	}

	std::wstring EditorUtils::OpenFileDialog(const wchar_t* filter, HWND owner)
	{
		WCHAR szFile[260] = { 0 };
		OPENFILENAMEW ofn = { 0 };
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = owner; // <--- Uses the passed HWND
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile) / sizeof(WCHAR);
		ofn.lpstrFilter = filter;
		ofn.nFilterIndex = 1;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

		if (GetOpenFileNameW(&ofn) == TRUE) return std::wstring(szFile);

		return L"";
	}
	std::wstring EditorUtils::MakeRelativePath(const std::wstring& absolutePath)
	{
		if (absolutePath.empty()) return L"";

		std::wstring result = absolutePath;
		size_t pos = result.find(L"Resources");
		if (pos == std::wstring::npos) pos = result.find(L"resources");

		if (pos != std::wstring::npos)
		{
			result = result.substr(pos);
		}
		else
		{
			try
			{
				std::filesystem::path fullPath(absolutePath);
				std::filesystem::path currentDir = std::filesystem::current_path();
				result = std::filesystem::relative(fullPath, currentDir).wstring();
			}
			catch (...) { result = absolutePath; }
		}

		for (wchar_t& c : result) if (c == L'\\') c = L'/';
		return result;
	}
}