#include "frontend/build/frontend-whatsnew.pch.hpp"

#ifdef BROWSER_AVAILABLE
struct QCef;
extern QCef *cef;
#endif

void WhatsNewBrowserInitThread::run()
{
#ifdef BROWSER_AVAILABLE
	cef->wait_for_browser_init();
#endif
	emit Result(url);
}
