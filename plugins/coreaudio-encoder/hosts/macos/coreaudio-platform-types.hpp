#pragma once

#include <AudioToolbox/AudioToolbox.h>
#include <util/apple/cfstring-utils.h>

namespace std {
template<> struct default_delete<remove_pointer<CFErrorRef>::type> {
	void operator()(remove_pointer<CFErrorRef>::type *err) { CFRelease(err); }
};

template<> struct default_delete<remove_pointer<CFStringRef>::type> {
	void operator()(remove_pointer<CFStringRef>::type *str) { CFRelease(str); }
};

} // namespace std
