#include "AetPlugin.h"
#include "Graphics/Auth2D/Aet/AetSet.h"

namespace AetPlugin
{
	A_Err VerifyAetSetImportable(const std::wstring& filePath, bool& canImport);
	A_Err ImportAetSet(Comfy::Graphics::Aet::AetSet& aetSet, AE_FIM_ImportOptions importOptions, AE_FIM_SpecialAction action, AEGP_ItemH itemHandle);

	void SetWorkingAetImportDirectory(const std::wstring& directory);
}
