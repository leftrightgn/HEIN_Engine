#include "pch.h"
#include "EditorUtils.h"
#include <windows.h>
#include <commdlg.h>
#include <shobjidl.h>
#include <filesystem>
#include <vector>

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

	bool EditorUtils::DoesFontFileExistOnDisk(const std::wstring& path)
	{
		if (path.empty()) return false;

		std::filesystem::path p(path);
		std::wstring filename = p.filename().wstring();

		std::vector<std::wstring> candidates = {
			path,
			std::wstring(L"Dual/") + path,
			std::wstring(L"../Dual/") + path,
			std::wstring(L"Resources/Font/") + filename,
			std::wstring(L"Dual/Resources/Font/") + filename,
			std::wstring(L"../Dual/Resources/Font/") + filename
		};

		for (const auto& candidate : candidates)
		{
			try
			{
				if (std::filesystem::exists(candidate) && std::filesystem::is_regular_file(candidate))
				{
					return true;
				}
			}
			catch (...)
			{
			}
		}
		return false;
	}

	std::wstring EditorUtils::ImportFontFileToResources(const std::wstring& sourceFilePath)
	{
		if (sourceFilePath.empty()) return L"";

		try
		{
			std::filesystem::path src(sourceFilePath);
			if (!std::filesystem::exists(src) || !std::filesystem::is_regular_file(src))
			{
				return L"";
			}

			std::wstring filename = src.filename().wstring();
			std::wstring relResult = std::wstring(L"Resources/Font/") + filename;

			// Destination candidate folders for Font assets
			std::vector<std::string> destDirs = {
				"Resources/Font",
				"Dual/Resources/Font",
				"../Dual/Resources/Font"
			};

			for (const auto& dir : destDirs)
			{
				if (std::filesystem::exists(dir) && std::filesystem::is_directory(dir))
				{
					std::filesystem::path dest = std::filesystem::path(dir) / filename;
					if (std::filesystem::absolute(src) != std::filesystem::absolute(dest))
					{
						std::filesystem::copy_file(src, dest, std::filesystem::copy_options::overwrite_existing);
					}
				}
			}

			// Mirror to desktop project if available
			const std::string desktopFontDir = "C:/Users/it252195/Desktop/Dual/\xe3\x83\x97\xe3\x83\xad\xe3\x82\xb8\xe3\x82\xa7\xe3\x82\xaf\xe3\x83\x88/Dual/Resources/Font";
			if (std::filesystem::exists(desktopFontDir))
			{
				std::filesystem::path dest = std::filesystem::path(desktopFontDir) / filename;
				if (std::filesystem::absolute(src) != std::filesystem::absolute(dest))
				{
					std::filesystem::copy_file(src, dest, std::filesystem::copy_options::overwrite_existing);
				}
			}

			return relResult;
		}
		catch (...)
		{
			return MakeRelativePath(sourceFilePath);
		}
	}
}
