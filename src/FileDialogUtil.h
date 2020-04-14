#pragma once
#include "Types.h"
#include "CoreTypes.h"
#include "Comfy/ComfyCore/Misc/StringHelper.h"

namespace FileDialogUtil
{
	namespace Customize
	{
		constexpr uint32_t ItemBaseID = 0x666;

		enum class ItemType
		{
			VisualGroupStart,
			VisualGroupEnd,
			Checkbox,
		};

		struct Item
		{
			ItemType Type;
			std::string_view Label;
			union DataUnion
			{
				bool* CheckboxChecked;
			} Data;
		};
	}

	struct FileFilter
	{
		std::string Name, Spec;
	};

	struct SaveFileDialogInput
	{
		std::string FileName;
		std::string DefaultExtension;
		std::vector<FileFilter> Filters;
		uint32_t FilterIndex;
		void* ParentWindowHandle;
		std::vector<Customize::Item> CustomizeItems;
		std::wstring OutFilePath;
	};

	bool OpenDialog(SaveFileDialogInput& dialog);
}
