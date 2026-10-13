#pragma once

template<typename Handler> void StopVirtualCamForShutdown(Handler *handler)
{
	if (handler && handler->VirtualCamActive()) {
		handler->StopVirtualCam();
	}
}
