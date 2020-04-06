#include "Win32Crypto.h"
#include "Core/Win32/ComfyWindows.h"
#include "Core/Logger.h"
#include <bcrypt.h>

#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)

namespace Comfy::FileSystem::Crypto
{
	namespace
	{
		enum class BlockCipherMode
		{
			CBC,
			ECB,
		};

		struct AlgorithmProvider
		{
			AlgorithmProvider(BlockCipherMode cipherMode)
			{
				NTSTATUS status;

				status = BCryptOpenAlgorithmProvider(&Handle, BCRYPT_AES_ALGORITHM, NULL, 0);
				if (NT_SUCCESS(status))
				{
					switch (cipherMode)
					{
					case BlockCipherMode::CBC:
						status = BCryptSetProperty(Handle, BCRYPT_CHAINING_MODE, (PBYTE)BCRYPT_CHAIN_MODE_CBC, sizeof(BCRYPT_CHAIN_MODE_CBC), 0);
						break;

					case BlockCipherMode::ECB:
						status = BCryptSetProperty(Handle, BCRYPT_CHAINING_MODE, (PBYTE)BCRYPT_CHAIN_MODE_ECB, sizeof(BCRYPT_CHAIN_MODE_ECB), 0);
						break;
					
					default:
						assert(false);
						return;
					}
				
					if (!NT_SUCCESS(status))
						Logger::LogErrorLine(__FUNCTION__"(): BCryptSetProperty() failed with 0x%X", status);
				}
				else
				{
					Logger::LogErrorLine(__FUNCTION__"(): BCryptOpenAlgorithmProvider() failed with 0x%X", status);
				}
			}

			~AlgorithmProvider()
			{
				if (Handle)
					BCryptCloseAlgorithmProvider(Handle, 0);
			}

			BCRYPT_ALG_HANDLE Handle = NULL;
		};

		struct SymmetricKey
		{
			SymmetricKey(AlgorithmProvider& algorithm, std::array<uint8_t, KeySize>& key)
			{
				if (!algorithm.Handle)
				{
					Logger::LogErrorLine(__FUNCTION__"(): Invalid AlgorithmProvider::Handle");
					return;
				}

				NTSTATUS status;
				
				ULONG keyObjectSize;
				DWORD copiedDataSize;

				status = BCryptGetProperty(algorithm.Handle, BCRYPT_OBJECT_LENGTH, (PBYTE)&keyObjectSize, sizeof(DWORD), &copiedDataSize, 0);
				if (NT_SUCCESS(status))
				{
					KeyObject.resize(keyObjectSize);
					status = BCryptGenerateSymmetricKey(algorithm.Handle, &Handle, KeyObject.data(), keyObjectSize, key.data(), static_cast<ULONG>(key.size()), 0);

					if (!NT_SUCCESS(status))
						Logger::LogErrorLine(__FUNCTION__"(): BCryptGenerateSymmetricKey() failed with 0x%X", status);
				}
				else
				{
					Logger::LogErrorLine(__FUNCTION__"(): BCryptGetProperty() failed with 0x%X", status);
				}
			}

			~SymmetricKey()
			{
				if (Handle)
					BCryptDestroyKey(Handle);
			}

			BCRYPT_KEY_HANDLE Handle = NULL;
			std::vector<uint8_t> KeyObject;
		};

		bool Win32DecryptInternal(SymmetricKey& key, uint8_t* encryptedData, uint8_t* decryptedData, size_t dataSize, std::array<uint8_t, IVSize>* iv)
		{
			if (!key.Handle)
			{
				Logger::LogErrorLine(__FUNCTION__"(): Invalid SymmetricKey::Handle");
				return false;
			}

			ULONG copiedDataSize;
			NTSTATUS status = (iv == nullptr) ?
				BCryptDecrypt(key.Handle, encryptedData, static_cast<ULONG>(dataSize), NULL, nullptr, 0, decryptedData, static_cast<ULONG>(dataSize), &copiedDataSize, 0) :
				BCryptDecrypt(key.Handle, encryptedData, static_cast<ULONG>(dataSize), NULL, iv->data(), static_cast<ULONG>(iv->size()), decryptedData, static_cast<ULONG>(dataSize), &copiedDataSize, 0);

			if (!NT_SUCCESS(status))
			{
				Logger::LogErrorLine(__FUNCTION__"(): BCryptDecrypt() failed with 0x%X", status);
				return false;
			}

			return true;
		}
	}

	bool Win32DecryptAesEcb(const uint8_t* encryptedData, uint8_t* decryptedData, size_t dataSize, std::array<uint8_t, KeySize> key)
	{
		AlgorithmProvider algorithmProvider = { BlockCipherMode::ECB };
		SymmetricKey symmetricKey = { algorithmProvider, key };

		return Win32DecryptInternal(symmetricKey, const_cast<uint8_t*>(encryptedData), decryptedData, dataSize, nullptr);
	}

	bool Win32DecryptAesCbc(const uint8_t* encryptedData, uint8_t* decryptedData, size_t dataSize, std::array<uint8_t, KeySize> key, std::array<uint8_t, IVSize> iv)
	{
		AlgorithmProvider algorithmProvider = { BlockCipherMode::CBC };
		SymmetricKey symmetricKey = { algorithmProvider, key };

		return Win32DecryptInternal(symmetricKey, const_cast<uint8_t*>(encryptedData), decryptedData, dataSize, &iv);
	}
}
