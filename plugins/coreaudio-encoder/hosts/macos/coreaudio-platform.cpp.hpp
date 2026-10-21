#include "plugins/coreaudio-encoder/build/coreaudio-encoder.pch.hpp"

bool ca_platform_load()
{
	return true;
}

void ca_platform_unload() {}

bool ca_platform_describe_status(OSStatus code, DStr &result)
{
	cf_ptr<CFErrorRef> err{CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainOSStatus, code, NULL)};
	cf_ptr<CFStringRef> str{CFErrorCopyDescription(err.get())};

	if (cfstr_copy_dstr(str.get(), kCFStringEncodingUTF8, result)) {
		return true;
	}

	return false;
}
