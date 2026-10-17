#include "frontend/updater/build/updater.pch.hpp"

void FreeProvider(HCRYPTPROV prov)
{
	CryptReleaseContext(prov, 0);
}
