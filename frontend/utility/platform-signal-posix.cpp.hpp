#include "frontend/build/frontend-signal-posix.pch.hpp"

namespace {
std::array<int, 2> sigIntFileDescriptor{0, 0};
std::array<int, 2> sigTermFileDescriptor{0, 0};
std::array<int, 2> sigAbrtFileDescriptor{0, 0};
std::array<int, 2> sigQuitFileDescriptor{0, 0};

QPointer<QSocketNotifier> sigIntNotifier{};
QPointer<QSocketNotifier> sigTermNotifier{};
QPointer<QSocketNotifier> sigAbrtNotifier{};
QPointer<QSocketNotifier> sigQuitNotifier{};

void sigIntSignalHandler(int)
{
	char tmp = 1;
	::send(sigIntFileDescriptor[0], &tmp, sizeof(tmp), 0);
}

void sigTermSignalHandler(int)
{
	char tmp = 1;
	::send(sigTermFileDescriptor[0], &tmp, sizeof(tmp), 0);
}

void sigAbrtSignalHandler(int)
{
	char tmp = 1;
	::send(sigAbrtFileDescriptor[0], &tmp, sizeof(tmp), 0);
}

void sigQuitSignalHandler(int)
{
	char tmp = 1;
	::send(sigQuitFileDescriptor[0], &tmp, sizeof(tmp), 0);
}
} // namespace

void InstallPlatformAppSignalHandlers()
{
	using SignalHandlerCallback = decltype(sigIntSignalHandler);

	auto setupSignalHandler = [](SignalHandlerCallback &callback, int signal) {
		struct sigaction signalHandler{};
		signalHandler.sa_handler = callback;
		sigemptyset(&signalHandler.sa_mask);
		signalHandler.sa_flags = 0;

		sigaction(signal, &signalHandler, nullptr);
	};

	setupSignalHandler(sigIntSignalHandler, SIGINT);
	setupSignalHandler(sigTermSignalHandler, SIGTERM);
	setupSignalHandler(sigTermSignalHandler, SIGHUP);
	setupSignalHandler(sigAbrtSignalHandler, SIGABRT);
	setupSignalHandler(sigQuitSignalHandler, SIGQUIT);

	// Block SIGPIPE in all threads, this can happen if a thread calls write on a closed pipe.
	// Supposedly this can happen when OBS tries to write to a closed RTMP socket.
	sigset_t sigpipeMask{};
	sigemptyset(&sigpipeMask);
	sigaddset(&sigpipeMask, SIGPIPE);
	sigset_t savedMask{};

	// pthread_sigmask returns 0 on success
	bool signalMaskChanged = pthread_sigmask(SIG_BLOCK, &sigpipeMask, &savedMask) == 0;

	if (!signalMaskChanged) {
		perror("Unexpected failure to add 'SIG_BLOCK' to 'SIGPIPE' signal mask.");
		exit(1);
	}
}

void ConnectPlatformAppSignals(OBSApp *app)
{
	// Add POSIX signal handlers:
	// * SIGINT
	// * SIGTERM
	// * SIGABRT
	// * SIGQUIT

	using SignalCallback = decltype(&OBSApp::processSigInt);

	auto connectSignal = [app](std::array<int, 2> &fileDescriptor, QPointer<QSocketNotifier> &notifier,
				   SignalCallback callback) -> void {
		socketpair(AF_UNIX, SOCK_STREAM, 0, fileDescriptor.data());
		notifier = new QSocketNotifier(fileDescriptor[1], QSocketNotifier::Read, app);
		QObject::connect(notifier, &QSocketNotifier::activated, app, callback);
	};

	connectSignal(sigIntFileDescriptor, sigIntNotifier, &OBSApp::processSigInt);
	connectSignal(sigTermFileDescriptor, sigTermNotifier, &OBSApp::processSigTerm);
	connectSignal(sigAbrtFileDescriptor, sigAbrtNotifier, &OBSApp::processSigAbrt);
	connectSignal(sigQuitFileDescriptor, sigQuitNotifier, &OBSApp::processSigQuit);
}

bool ConsumePlatformAppSignal(PlatformAppSignal signal)
{
	auto consumeSignal = [](std::array<int, 2> &fileDescriptor, QPointer<QSocketNotifier> &notifier) {
		if (!notifier->isEnabled()) {
			return false;
		}
		notifier->setEnabled(false);
		char tmp;
		::recv(fileDescriptor[1], &tmp, sizeof(tmp), 0);
		notifier->setEnabled(true);
		return true;
	};
	switch (signal) {
	case PlatformAppSignal::Interrupt:
		return consumeSignal(sigIntFileDescriptor, sigIntNotifier);
	case PlatformAppSignal::Terminate:
		return consumeSignal(sigTermFileDescriptor, sigTermNotifier);
	case PlatformAppSignal::Abort:
		return consumeSignal(sigAbrtFileDescriptor, sigAbrtNotifier);
	case PlatformAppSignal::Quit:
		return consumeSignal(sigQuitFileDescriptor, sigQuitNotifier);
	}
	return false;
}

void DisconnectPlatformAppSignals()
{
	auto disconnectSignal = [](std::array<int, 2> &fileDescriptor, QPointer<QSocketNotifier> &notifier) -> void {
		notifier->setEnabled(false);

		std::array<int, 2> tempFileDescriptor = std::exchange(fileDescriptor, {0, 0});
		::close(tempFileDescriptor[0]);
		::close(tempFileDescriptor[1]);
	};

	disconnectSignal(sigIntFileDescriptor, sigIntNotifier);
	disconnectSignal(sigTermFileDescriptor, sigTermNotifier);
	disconnectSignal(sigAbrtFileDescriptor, sigAbrtNotifier);
	disconnectSignal(sigQuitFileDescriptor, sigQuitNotifier);
}
