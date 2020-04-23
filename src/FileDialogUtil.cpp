#include "FileDialogUtil.h"
#include <Windows.h>
#define STRICT_TYPED_ITEMIDS
#include <shlobj.h>
#include <objbase.h>
#include <shobjidl.h>
#include <shlwapi.h>
#include <knownfolders.h>
#include <propvarutil.h>
#include <propkey.h>
#include <propidl.h>
#include <strsafe.h>
#include <shtypes.h>

namespace FileDialogUtil
{
	using namespace Comfy;

	namespace
	{
		class DialogEventHandler : public IFileDialogEvents, public IFileDialogControlEvents
		{
		public:
			DialogEventHandler() : refCount(1) {}
			~DialogEventHandler() = default;

		public:
#pragma region IUnknown
			IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv)
			{
				static const QITAB searchTable[] =
				{
					QITABENT(DialogEventHandler, IFileDialogEvents),
					QITABENT(DialogEventHandler, IFileDialogControlEvents),
					{ nullptr, 0 },
				};
				return QISearch(this, searchTable, riid, ppv);
			}

			IFACEMETHODIMP_(ULONG) AddRef()
			{
				return InterlockedIncrement(&refCount);
			}

			IFACEMETHODIMP_(ULONG) Release()
			{
				long cRef = InterlockedDecrement(&refCount);
				if (!cRef)
					delete this;
				return cRef;
			}
#pragma endregion IUnknown

#pragma region IFileDialogEvents
			IFACEMETHODIMP OnFileOk(IFileDialog*) { return S_OK; }
			IFACEMETHODIMP OnFolderChange(IFileDialog*) { return S_OK; }
			IFACEMETHODIMP OnFolderChanging(IFileDialog*, IShellItem*) { return S_OK; }
			IFACEMETHODIMP OnHelp(IFileDialog*) { return S_OK; }
			IFACEMETHODIMP OnSelectionChange(IFileDialog*) { return S_OK; }
			IFACEMETHODIMP OnShareViolation(IFileDialog*, IShellItem*, FDE_SHAREVIOLATION_RESPONSE*) { return S_OK; }
			IFACEMETHODIMP OnTypeChange(IFileDialog* pfd) { return S_OK; }
			IFACEMETHODIMP OnOverwrite(IFileDialog*, IShellItem*, FDE_OVERWRITE_RESPONSE*) { return S_OK; }

			// This method gets called when an dialog control item selection happens (radio-button selection. etc).
			// For sample sake, let's react to this event by changing the dialog title.
			IFACEMETHODIMP OnItemSelected(IFileDialogCustomize* pfdc, DWORD dwIDCtl, DWORD dwIDItem)
			{
				HRESULT result = S_OK;
				IFileDialog* fileDialog = nullptr;

				if (result = pfdc->QueryInterface(&fileDialog); !SUCCEEDED(result))
					return result;

#if 0 // NOTE: Add future item check logic here if needed
				switch (dwIDItem)
				{
				default:
					break;
				}
#endif

				fileDialog->Release();
				return result;
			}

			IFACEMETHODIMP OnButtonClicked(IFileDialogCustomize*, DWORD) { return S_OK; }
			IFACEMETHODIMP OnCheckButtonToggled(IFileDialogCustomize*, DWORD, BOOL) { return S_OK; }
			IFACEMETHODIMP OnControlActivating(IFileDialogCustomize*, DWORD) { return S_OK; }
#pragma endregion IFileDialogEvents

		private:
			long refCount;

		public:
			static HRESULT CreateInstance(REFIID riid, void** ppv)
			{
				*ppv = nullptr;
				if (DialogEventHandler* pDialogEventHandler = new DialogEventHandler(); pDialogEventHandler != nullptr)
				{
					HRESULT result = pDialogEventHandler->QueryInterface(riid, ppv);
					pDialogEventHandler->Release();
					return S_OK;
				}

				return E_OUTOFMEMORY;
			}
		};

		void PlaceCustomDialogItems(std::vector<Customize::Item>& customizeItems, IFileDialogCustomize& dialogCustomize)
		{
			HRESULT result = S_OK;

			for (size_t i = 0; i < customizeItems.size(); i++)
			{
				const auto& item = customizeItems[i];
				const auto itemID = static_cast<DWORD>(Customize::ItemBaseID + i);

				if (item.Type == Customize::ItemType::VisualGroupStart)
					result = dialogCustomize.StartVisualGroup(itemID, Utf8ToUtf16(item.Label).c_str());
				else if (item.Type == Customize::ItemType::VisualGroupEnd)
					result = dialogCustomize.EndVisualGroup();
				else if (item.Type == Customize::ItemType::Checkbox)
				{
					result = dialogCustomize.AddCheckButton(itemID, Utf8ToUtf16(item.Label).c_str(), (item.Data.CheckboxChecked != nullptr) ? *item.Data.CheckboxChecked : false);

					if (item.Data.CheckboxChecked == nullptr)
						result = dialogCustomize.SetControlState(itemID, CDCS_VISIBLE);
				}
			}
		}

		void ReadCustomDialogItems(std::vector<Customize::Item>& customizeItems, IFileDialogCustomize& dialogCustomize)
		{
			HRESULT result = S_OK;

			for (size_t i = 0; i < customizeItems.size(); i++)
			{
				auto& item = customizeItems[i];
				const auto itemID = static_cast<DWORD>(Customize::ItemBaseID + i);

				if (item.Type == Customize::ItemType::Checkbox)
				{
					BOOL checked;
					result = dialogCustomize.GetCheckButtonState(itemID, &checked);
					if (item.Data.CheckboxChecked != nullptr)
						*item.Data.CheckboxChecked = checked;
				}
			}
		}
	}

	bool OpenDialog(SaveFileDialogInput& dialog)
	{
		HRESULT result = S_OK;
		IFileDialog* fileDialog = nullptr;

		if (result = ::CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&fileDialog)); !SUCCEEDED(result))
			return false;

		DWORD eventCookie = 0;
		IFileDialogEvents* dialogEvents = nullptr;
		IFileDialogCustomize* dialogCustomize = nullptr;

		if (result = DialogEventHandler::CreateInstance(IID_PPV_ARGS(&dialogEvents)); SUCCEEDED(result))
		{
			if (result = fileDialog->Advise(dialogEvents, &eventCookie); SUCCEEDED(result))
			{
				if (result = fileDialog->QueryInterface(IID_PPV_ARGS(&dialogCustomize)); SUCCEEDED(result))
					PlaceCustomDialogItems(dialog.CustomizeItems, *dialogCustomize);

				DWORD existingOptionsFlags = 0;
				result = fileDialog->GetOptions(&existingOptionsFlags);
				result = fileDialog->SetOptions(existingOptionsFlags | FOS_FORCEFILESYSTEM);

				if (!dialog.DefaultExtension.empty())
					result = fileDialog->SetDefaultExtension(Utf8ToUtf16(dialog.DefaultExtension).c_str());

				if (!dialog.FileName.empty())
					result = fileDialog->SetFileName(Utf8ToUtf16(dialog.FileName).c_str());

				if (!dialog.Filters.empty())
				{
					std::vector<std::wstring> owningWideStrings;
					owningWideStrings.reserve(dialog.Filters.size() * 2);

					std::vector<COMDLG_FILTERSPEC> convertedFilters;
					convertedFilters.reserve(dialog.Filters.size());

					for (const auto& inputFilter : dialog.Filters)
					{
						convertedFilters.push_back(COMDLG_FILTERSPEC
							{
								owningWideStrings.emplace_back(Utf8ToUtf16(inputFilter.Name)).c_str(),
								owningWideStrings.emplace_back(Utf8ToUtf16(inputFilter.Spec)).c_str(),
							});
					}

					result = fileDialog->SetFileTypes(static_cast<UINT>(convertedFilters.size()), convertedFilters.data());
					result = fileDialog->SetFileTypeIndex(dialog.FilterIndex);
				}
			}

			dialogEvents->Release();
		}

		if (SUCCEEDED(result))
		{
			if (result = fileDialog->Show(reinterpret_cast<HWND>(dialog.ParentWindowHandle)); SUCCEEDED(result))
			{
				IShellItem* itemResult = nullptr;
				if (result = fileDialog->GetResult(&itemResult); SUCCEEDED(result))
				{
					wchar_t* filePath = nullptr;
					if (result = itemResult->GetDisplayName(SIGDN_FILESYSPATH, &filePath); SUCCEEDED(result))
					{
						result = fileDialog->GetFileTypeIndex(&dialog.FilterIndex);
						dialog.OutFilePath = filePath;

						if (dialogCustomize != nullptr)
							ReadCustomDialogItems(dialog.CustomizeItems, *dialogCustomize);

						::CoTaskMemFree(filePath);
					}

					itemResult->Release();
				}
			}
		}

		if (dialogCustomize != nullptr)
			dialogCustomize->Release();

		if (fileDialog != nullptr)
		{
			fileDialog->Unadvise(eventCookie);
			fileDialog->Release();
		}

		return SUCCEEDED(result) && !dialog.OutFilePath.empty();
	}
}
