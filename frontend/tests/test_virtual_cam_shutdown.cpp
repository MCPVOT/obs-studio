#include "../widgets/VirtualCamShutdown.hpp"

#include <cassert>

struct FakeHandler {
	bool active = false;
	int stops = 0;

	bool VirtualCamActive() const { return active; }
	void StopVirtualCam() { ++stops; }
};

int main()
{
	StopVirtualCamForShutdown(static_cast<FakeHandler *>(nullptr));

	FakeHandler inactive;
	StopVirtualCamForShutdown(&inactive);
	assert(inactive.stops == 0);

	FakeHandler active{true, 0};
	StopVirtualCamForShutdown(&active);
	assert(active.stops == 1);
}
