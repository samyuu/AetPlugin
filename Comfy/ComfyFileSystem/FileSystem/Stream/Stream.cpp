#include "Stream.h"

namespace Comfy::FileSystem
{
	FileAddr StreamBase::RemainingBytes() const
	{
		return GetLength() - GetPosition();
	}

	bool StreamBase::EndOfFile() const
	{
		return GetPosition() >= GetLength();
	}

	void StreamBase::Skip(FileAddr amount)
	{
		Seek(GetPosition() + amount);
	}

	void StreamBase::Rewind()
	{
		Seek(FileAddr(0));
	}
}
