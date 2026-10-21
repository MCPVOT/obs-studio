#!/usr/bin/env -S uv run --script
"""OBSApp theme policy and POSIX signal lifecycle characterization (#885)."""
from pathlib import Path
import re
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import platform_ifdef_ratchet as ratchet
from test_settings_platform_facade import function

ROOT = Path(__file__).resolve().parent.parent
APP = ROOT / 'frontend/OBSApp.cpp.hpp'
THEMES = ROOT / 'frontend/OBSApp_Themes.cpp.hpp'
UTILITY = ROOT / 'frontend/utility'
SIGNALS = UTILITY / 'platform-signal-posix.cpp.hpp'
KINDS = ('Int', 'Term', 'Abrt', 'Quit')
THEME_SIGS = ('const char *GetPlatformThemePrefix(', 'QStyle *CreatePlatformThemeStyle(',
              'void ApplyPlatformThemeDarkMode(', 'const char *GetPlatformFallbackTheme(')
ENUM = 'enum class PlatformAppSignal { Interrupt, Terminate, Abort, Quit };\n'


def compile_run(source, args=()):
    compiler = shutil.which('c++')
    if not compiler:
        raise unittest.SkipTest('C++ compiler unavailable; native builds remain required')
    with tempfile.TemporaryDirectory(prefix='obsapp-platform-') as tmp:
        p = Path(tmp) / 'policy.cpp'; p.write_text(source)
        binary = Path(tmp) / 'policy'
        result = subprocess.run([compiler, '-std=c++17', *args, str(p), '-o', str(binary)],
                                capture_output=True, text=True)
        if result.returncode:
            raise AssertionError(result.stderr)
        subprocess.run([str(binary)], check=True, timeout=15)


def theme_bodies(host):
    text = (UTILITY / f'platform-{host}.hpp').read_text()
    if THEME_SIGS[0] in text:
        return '\n'.join(function(text, sig) for sig in THEME_SIGS)
    original = THEMES.read_text()
    prefix = re.search(r'#ifdef _WIN32\n\s*const QString osPrefix.*?#endif', original, re.S)[0]
    style = re.search(r'#if !defined\(_WIN32\).*?setStyle.*?#endif', original, re.S)[0]
    dark = re.search(r'#ifdef __APPLE__\n\s*SetMacOSDarkMode.*?#endif', original, re.S)[0]
    fallback = re.search(r'#ifdef _WIN32\n\s*themeName = HighContrastEnabled.*?#endif', original, re.S)[0]
    return ('const char *GetPlatformThemePrefix() {\n' + prefix.replace('const QString', 'const char *') + '\nreturn osPrefix;}\n'
            'QStyle *CreatePlatformThemeStyle() { QStyle *result=nullptr; auto setStyle=[&](QStyle *p){result=p;};\n' + style + '\nreturn result;}\n'
            'void ApplyPlatformThemeDarkMode(bool dark) {\n' + dark.replace('theme->isDark', 'dark') + '\n}\n'
            'const char *GetPlatformFallbackTheme(const char *DEFAULT_THEME) {const char *themeName;\n' + fallback + '\nreturn themeName;}\n')


def bridge_code():
    if SIGNALS.exists():
        return '\n'.join(SIGNALS.read_text().splitlines()[1:])
    # Compile the original setup, async handlers, reads and shutdown bodies.
    text = APP.read_text()
    definitions = text[text.index('std::array<int, 2> OBSApp::sigIntFileDescriptor'):]
    definitions = definitions[:definitions.index('#endif')]
    handlers = '\n'.join(function(text, f'void OBSApp::sig{k}SignalHandler(') for k in KINDS)
    constructor = text[text.index('// Add POSIX signal handlers:'):]
    constructor = constructor[:constructor.index('#endif')]
    shutdown = function(text, 'void OBSApp::applicationShutdown()')
    shutdown = shutdown[shutdown.index('auto disconnectSignal'):shutdown.index('#endif')]
    consumers = []
    for index, kind in enumerate(KINDS):
        body = function(text, f'void OBSApp::processSig{kind}(')
        part = body[body.index('{') + 1:body.index('->setEnabled(true);') + len('->setEnabled(true);')]
        consumers.append(f'case {index}: {{' + part.replace('return;', 'return false;') + '\nreturn true;}')
    main = (ROOT / 'frontend/obs-main.cpp.hpp').read_text()
    install = main[main.index('using SignalHandlerCallback = decltype(OBSApp::sigIntSignalHandler)'):]
    install = install[:install.index('#endif')]
    return (definitions + handlers + '\nvoid OBSApp::connectLegacy() {\n' + constructor + '\n}\n'
            'void OBSApp::disconnectLegacy() {\n' + shutdown + '\n}\n'
            'bool OBSApp::consumeLegacy(int kind) {switch(kind) {' + '\n'.join(consumers) + '} return false;}\n'
            'static OBSApp *bridgeApp;\nvoid InstallPlatformAppSignalHandlers() {\n' + install + '\n}\n'
            'void ConnectPlatformAppSignals(OBSApp *app) {bridgeApp=app;app->connectLegacy();}\n'
            'bool ConsumePlatformAppSignal(PlatformAppSignal kind) {return bridgeApp->consumeLegacy(int(kind));}\n'
            'void DisconnectPlatformAppSignals() {bridgeApp->disconnectLegacy();}\n')


class OBSAppThemePlatformTest(unittest.TestCase):
    def test_shared_boundaries(self):
        for path in (ROOT / 'frontend/OBSApp.hpp', THEMES):
            with self.subTest(path=path):
                self.assertEqual(ratchet.count_platform_conditionals(path.read_text()), [])
        for path in (APP, ROOT / 'frontend/build/frontend-app-obsapp.pch.hpp'):
            self.assertFalse('OBS_PLATFORM_POSIX_SIGNALS' in path.read_text(), str(path))

    def test_operations_and_build_wiring(self):
        self.assertTrue(SIGNALS.exists(), 'missing POSIX signal bridge')
        header = (UTILITY / 'platform.hpp').read_text()
        for name in ('InstallPlatformAppSignalHandlers', 'ConnectPlatformAppSignals',
                     'ConsumePlatformAppSignal', 'DisconnectPlatformAppSignals',
                     'InstallPlatformDllBlocklist', 'LogPlatformBlockedDlls'):
            self.assertIn(name + '(', header)
        for host in ('windows.cpp', 'osx.mm', 'x11.cpp'):
            text = (UTILITY / f'platform-{host}.hpp').read_text()
            for signature in THEME_SIGS:
                self.assertIn(signature, text)
        for host in ('macos', 'linux', 'freebsd'):
            self.assertIn('build/frontend.signal-posix+.hpp.cpp',
                          (ROOT / f'frontend/cmake/os-{host}.cmake').read_text())

    def test_legacy_theme_policy(self):
        for host, flags, prefix, style, mac in (
            ('windows.cpp', ['-D_WIN32'], 'os_win_', 'windowsvista', False),
            ('osx.mm', ['-D__APPLE__'], 'os_mac_', 'default', True),
            ('x11.cpp', [], 'os_lin_', 'Fusion', False),
        ):
            with self.subTest(host=host):
                source = r'''
#include <string>
#include <cassert>
struct QStyle {std::string name; explicit QStyle(const char *n):name(n){} virtual ~QStyle()=default;};
struct OBSProxyStyle:QStyle {OBSProxyStyle():QStyle("default"){} explicit OBSProxyStyle(const char *n):QStyle(n){}};
static bool contrast=false, darkValue=false; static int darkCalls=0;
bool HighContrastEnabled() {return contrast;}
void SetMacOSDarkMode(bool value) {darkValue=value;++darkCalls;}
''' + theme_bodies(host) + f'''
int main() {{
 assert(std::string(GetPlatformThemePrefix())=="{prefix}");
 QStyle *style=CreatePlatformThemeStyle(); assert(style->name=="{style}");delete style;
 ApplyPlatformThemeDarkMode(true);ApplyPlatformThemeDarkMode(false);
 assert(darkCalls=={2 if mac else 0});assert(!darkValue);
 for(bool enabled:{{false,true}}) {{contrast=enabled;
  assert(std::string(GetPlatformFallbackTheme("fallback"))==
    ({'enabled ? "com.obsproject.System" : "fallback"' if host=='windows.cpp' else '"fallback"'}));
 }}
}}
'''
                compile_run(source, flags)

    def test_signal_application_actions(self):
        text = APP.read_text()
        methods = '\n'.join(function(text, f'void OBSApp::processSig{k}(') for k in KINDS)
        source = r'''
#include <array>
#include <deque>
#include <functional>
#include <memory>
#include <vector>
#include <cassert>
''' + ENUM + r'''
static bool saveBefore=false, haveMain=true, consumeEnabled=true;
bool PlatformSavesBeforeSignalQuit(){return saveBefore;}
bool ConsumePlatformAppSignal(PlatformAppSignal){return consumeEnabled;}
struct QWidget;
static std::deque<QWidget *> modals;
struct QWidget {virtual ~QWidget()=default;virtual void close(){modals.pop_front();}};
struct QDialog:QWidget {void reject(){close();}};
template<class T> T qobject_cast(QWidget *p){return dynamic_cast<T>(p);}
static std::function<void()> queued;
struct QTimer {template<class R,class C>static void singleShot(int,R *r,C cb){queued=[r,cb]{(r->*cb)();};}};
struct OBSBasic {
 int saves=0,closes=0,noPrompts=0;
 static OBSBasic *Get();void saveAll(){++saves;}void close(){++closes;}void closeWithoutPrompt(){++noPrompts;}
};
static OBSBasic mainWindow;
OBSBasic *OBSBasic::Get(){return haveMain?&mainWindow:nullptr;}
struct OBSApp {
 bool quitRequested_=false;int quits=0;
 QWidget *activeModalWidget(){return modals.empty()?nullptr:modals.front();}
 void quit(){++quits;}
 void processSigInt();void processSigTerm();void processSigAbrt();void processSigQuit();
};
''' + methods + r'''
int main(){
 using Fn=void(OBSApp::*)();std::array<Fn,4> methods={&OBSApp::processSigInt,&OBSApp::processSigTerm,&OBSApp::processSigAbrt,&OBSApp::processSigQuit};
 for(bool save:{false,true})for(bool main:{false,true})for(int modalCount:{0,1,20})for(int signal=0;signal<4;++signal){
  OBSApp app;mainWindow={};saveBefore=save;haveMain=main;queued={};modals.clear();
  std::vector<std::unique_ptr<QWidget>> owned;
  for(int i=0;i<modalCount;++i){if(i%2)owned.emplace_back(new QWidget);else owned.emplace_back(new QDialog);modals.push_back(owned.back().get());}
  consumeEnabled=false;(app.*methods[signal])();assert(app.quits==0&&mainWindow.saves==0&&modals.size()==size_t(modalCount));
  consumeEnabled=true;(app.*methods[signal])();
  assert(mainWindow.saves==int(save&&main));
  assert(mainWindow.closes==int(signal==0&&save&&main));
  assert(mainWindow.noPrompts==int(signal==1&&main));
  if(signal==1&&modalCount){assert(app.quitRequested_);assert(app.quits==0);assert(queued);assert(modals.size()==size_t(modalCount-std::min(16,modalCount)));queued();assert(app.quits==1);}
  else {assert(!app.quitRequested_);assert(!queued);assert(app.quits==int(signal!=0||!save));assert(modals.size()==size_t(modalCount));}
 }
}
'''
        compile_run(source)

    def test_windows_signal_and_dll_noops(self):
        for host in ('windows.cpp', 'osx.mm', 'x11.cpp'):
            text = (UTILITY / f'platform-{host}.hpp').read_text()
            signatures = ['void InstallPlatformDllBlocklist(', 'void LogPlatformBlockedDlls(']
            if host == 'windows.cpp':
                signatures += ['void InstallPlatformAppSignalHandlers(', 'void ConnectPlatformAppSignals(',
                               'bool ConsumePlatformAppSignal(', 'void DisconnectPlatformAppSignals(']
            code = '\n'.join(function(text, sig) for sig in signatures)
            preamble = ENUM + 'class OBSApp; int installs=0,logs=0; extern "C" void install_dll_blocklist_hook(){++installs;} extern "C" void log_blocked_dlls(){++logs;}\n'
            calls = 'InstallPlatformAppSignalHandlers();ConnectPlatformAppSignals(nullptr);DisconnectPlatformAppSignals();for(int k=0;k<4;++k)if(ConsumePlatformAppSignal(PlatformAppSignal(k)))return 1;' if host == 'windows.cpp' else ''
            main = 'int main(){InstallPlatformDllBlocklist();LogPlatformBlockedDlls();' + calls + f'return installs!={int(host=="windows.cpp")} || logs!={int(host=="windows.cpp")};}}'
            compile_run(preamble + code + main)

    def test_real_qt_signal_delivery(self):
        prefix = os.environ.get('OBS_QT_TEST_PREFIX')
        if not prefix:
            self.skipTest('Real Qt signal test runs in Qt CI with its pinned prefix')
        code = r'''
#include <QCoreApplication>
#include <QSocketNotifier>
#include <QPointer>
#include <QTimer>
#include <array>
#include <utility>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <cassert>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
''' + ENUM + r'''
class OBSApp;bool ConsumePlatformAppSignal(PlatformAppSignal);
class OBSApp:public QCoreApplication {
 void consume(PlatformAppSignal signal){if(ConsumePlatformAppSignal(signal)){++calls[int(signal)];if(++total==5)quit();}}
public:
 std::array<int,4> calls{};int total=0;
 OBSApp(int &argc,char **argv):QCoreApplication(argc,argv){}
 void processSigInt(){consume(PlatformAppSignal::Interrupt);}void processSigTerm(){consume(PlatformAppSignal::Terminate);}
 void processSigAbrt(){consume(PlatformAppSignal::Abort);}void processSigQuit(){consume(PlatformAppSignal::Quit);}
};
''' + bridge_code() + r'''
int main(int argc,char **argv){
 InstallPlatformAppSignalHandlers();OBSApp app(argc,argv);ConnectPlatformAppSignals(&app);
 QTimer::singleShot(0,[]{raise(SIGINT);raise(SIGTERM);raise(SIGHUP);raise(SIGABRT);raise(SIGQUIT);});
 QTimer::singleShot(3000,&app,[&]{app.exit(99);});
 int result=app.exec();assert(result==0);assert((app.calls==std::array<int,4>{1,2,1,1}));
 DisconnectPlatformAppSignals();
}
'''
        with tempfile.TemporaryDirectory(prefix='obsapp-real-qt-signals-') as tmp:
            Path(tmp, 'signal.cpp').write_text(code)
            Path(tmp, 'CMakeLists.txt').write_text(
                'cmake_minimum_required(VERSION 3.28)\nproject(signal_bridge LANGUAGES CXX)\n'
                'find_package(Qt6 REQUIRED COMPONENTS Core)\nadd_executable(signal_test signal.cpp)\n'
                'target_compile_features(signal_test PRIVATE cxx_std_17)\n'
                'target_link_libraries(signal_test PRIVATE Qt6::Core)\n')
            build = str(Path(tmp) / 'build')
            subprocess.run(['cmake', '-S', tmp, '-B', build, '-DCMAKE_PREFIX_PATH=' + prefix], check=True)
            subprocess.run(['cmake', '--build', build], check=True)
            subprocess.run([str(Path(build) / 'signal_test')], check=True, timeout=10)

    def test_posix_signal_lifecycle(self):
        source = r'''
#include <array>
#include <utility>
#include <vector>
#include <functional>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <csignal>
#include <sys/socket.h>
#include <sys/poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
''' + ENUM + r'''
class OBSApp;
void InstallPlatformAppSignalHandlers();void ConnectPlatformAppSignals(OBSApp *);
bool ConsumePlatformAppSignal(PlatformAppSignal);void DisconnectPlatformAppSignals();
struct QSocketNotifier;
static std::vector<QSocketNotifier *> notifiers;
struct QSocketNotifier {
 enum {Read};int descriptor; bool enabled=true;std::function<void()> callback;
 QSocketNotifier(int fd,int,OBSApp *):descriptor(fd){notifiers.push_back(this);}
 bool isEnabled() const {return enabled;} void setEnabled(bool value){enabled=value;}
 void activated(){}
};
template<class T> struct QPointer {T *ptr=nullptr;QPointer&operator=(T *p){ptr=p;return *this;}T *operator->()const{return ptr;}operator T*()const{return ptr;}};
struct QObject {template<class S,class E,class R,class C>static void connect(S sender,E,R *receiver,C cb){sender->callback=[receiver,cb]{(receiver->*cb)();};}};
class OBSApp:public QObject {
public:
 static std::array<int,2> sigIntFileDescriptor,sigTermFileDescriptor,sigAbrtFileDescriptor,sigQuitFileDescriptor;
 QPointer<QSocketNotifier> sigIntNotifier,sigTermNotifier,sigAbrtNotifier,sigQuitNotifier;
 static void sigIntSignalHandler(int);static void sigTermSignalHandler(int);static void sigAbrtSignalHandler(int);static void sigQuitSignalHandler(int);
 void connectLegacy();void disconnectLegacy();bool consumeLegacy(int);
 std::array<int,4> calls{};
 void processSigInt(){if(ConsumePlatformAppSignal(PlatformAppSignal::Interrupt))++calls[0];}
 void processSigTerm(){if(ConsumePlatformAppSignal(PlatformAppSignal::Terminate))++calls[1];}
 void processSigAbrt(){if(ConsumePlatformAppSignal(PlatformAppSignal::Abort))++calls[2];}
 void processSigQuit(){if(ConsumePlatformAppSignal(PlatformAppSignal::Quit))++calls[3];}
};
''' + bridge_code() + r'''
int main() {
 InstallPlatformAppSignalHandlers();
 sigset_t mask;pthread_sigmask(SIG_BLOCK,nullptr,&mask);assert(sigismember(&mask,SIGPIPE)==1);
 int signals[]={SIGINT,SIGTERM,SIGHUP,SIGABRT,SIGQUIT};int kinds[]={0,1,1,2,3};
 for(int round=0;round<3;++round) {
  OBSApp app;notifiers.clear();ConnectPlatformAppSignals(&app);assert(notifiers.size()==4);
  for(int k=0;k<4;++k){notifiers[k]->setEnabled(false);assert(!ConsumePlatformAppSignal(PlatformAppSignal(k)));notifiers[k]->setEnabled(true);}
  for(int i=0;i<5;++i) {
   struct sigaction action{};sigaction(signals[i],nullptr,&action);assert(action.sa_handler!=SIG_DFL);assert(!(action.sa_flags&SA_RESTART));
   raise(signals[i]);int kind=kinds[i];pollfd fd{notifiers[kind]->descriptor,POLLIN,0};
   assert(poll(&fd,1,0)==1);notifiers[kind]->callback();assert(notifiers[kind]->isEnabled());
   fd.revents=0;assert(poll(&fd,1,0)==0);
  }
  assert((app.calls==std::array<int,4>{1,2,1,1}));
  std::array<int,4> descriptors;for(int k=0;k<4;++k)descriptors[k]=notifiers[k]->descriptor;
  DisconnectPlatformAppSignals();
  for(int k=0;k<4;++k){assert(!notifiers[k]->isEnabled());assert(fcntl(descriptors[k],F_GETFD)==-1&&errno==EBADF);delete notifiers[k];}
 }
}
'''
        compile_run(source, ['-pthread'])


if __name__ == '__main__':
    unittest.main()
