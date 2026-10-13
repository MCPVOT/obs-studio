#pragma once

template<typename Handler> void StopVirtualCamForShutdown(Handler *handler)
{
	if (handler->VirtualCamActive()) {
		handler->StopVirtualCam();
	}
}
