#!/usr/bin/env -S uv run --script
"""Dialog host policy, native notifications and interaction modifiers (#879)."""
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

ROOT=Path(__file__).resolve().parent.parent
DIALOGS=ROOT/'frontend/dialogs'
UTILITY=ROOT/'frontend/utility'
CLASSES=('OBSBasicFilters','OBSBasicInteraction','OBSBasicProperties')
FILES=('OBSAbout','OBSBasicFilters','OBSBasicInteraction','OBSBasicProperties','OBSYoutubeActions')
SIGNATURES=('const char *GetPlatformAuthorsFile(', 'Qt::Key GetPlatformFilterRenameKey(',
            'void HandlePlatformDialogNativeEvent(', 'int TranslatePlatformInteractionModifiers(',
            'const char *GetPlatformPropertiesFontFace(', 'const char *GetPlatformPropertiesTextSource(',
            'float GetPlatformPropertiesLabelHeight(', 'void AdjustPlatformYouTubeDialogSize(')


def policy(host):
    text=(UTILITY/f'platform-{host}.hpp').read_text()
    if SIGNATURES[0] in text:
        return '\n'.join(function(text,s) for s in SIGNATURES)
    # Characterize actual old branches before their extraction.
    about=(DIALOGS/'OBSAbout.cpp.hpp').read_text()
    filters=(DIALOGS/'OBSBasicFilters.cpp.hpp').read_text()
    props=(DIALOGS/'OBSBasicProperties.cpp.hpp').read_text()
    youtube=(DIALOGS/'OBSYoutubeActions.cpp.hpp').read_text()
    authors=re.search(r'#ifdef __APPLE__\n\s*if \(!GetDataFilePath.*?#endif',about,re.S)[0]
    authors=re.sub(r'if \(!GetDataFilePath\("([^"]+)", path\)\) \{',r'return "\1";',authors)
    rename=re.search(r'#ifdef __APPLE__\n\s*ui->actionRenameFilter->setShortcut.*?#endif',filters,re.S)[0]
    rename=re.sub(r'ui->actionRenameFilter->setShortcut\(\{([^}]+)\}\);',r'return \1;',rename)
    font=re.search(r'#if defined\(_WIN32\)\n\s*obs_data_set_string\(font.*?#endif',props,re.S)[0]
    font=re.sub(r'obs_data_set_string\(font, "face", "([^"]+)"\);',r'return "\1";',font)
    source=re.search(r'#ifdef _WIN32\n\s*const char \*text_source_id.*?#endif',props,re.S)[0]
    height=re.search(r'#ifdef _WIN32\n\s*obs_source_get_height.*?#endif',props,re.S)[0]
    height=re.sub(r'obs_source_get_height\(scene\)(.*?)\);',r'return height\1;',height)
    resize=re.search(r'#ifdef __APPLE__\n\s*// MacOS theming issues.*?#endif',youtube,re.S)[0].replace('this->','dialog->')
    return ('const char *GetPlatformAuthorsFile(){\n'+authors+'\n}\n'
            'Qt::Key GetPlatformFilterRenameKey(){\n'+rename+'\n}\n'
            'const char *GetPlatformPropertiesFontFace(){\n'+font+'\n}\n'
            'const char *GetPlatformPropertiesTextSource(){\n'+source+'\nreturn text_source_id;}\n'
            'float GetPlatformPropertiesLabelHeight(uint32_t height){\n'+height+'\n}\n'
            'void AdjustPlatformYouTubeDialogSize(QWidget *dialog){\n'+resize+'\n}\n')


def native_methods():
    return '\n'.join(function((DIALOGS/f'{name}.cpp.hpp').read_text(),f'bool {name}::nativeEvent(')
                     for name in CLASSES)


STUBS=r'''
#include <cstdint>
#include <string>
#include <vector>
#include <cassert>
#include <cstring>
#include <type_traits>
#include "libobs/obs-interaction.h"
namespace Qt {
enum Key {Key_Return=13,Key_F2=113};
constexpr int ShiftModifier=1,AltModifier=2,ControlModifier=4,MetaModifier=8,KeypadModifier=16;
struct KeyboardModifiers{int value;bool testFlag(int flag)const{return value&flag;}};
}
struct QInputEvent{Qt::KeyboardModifiers flags;Qt::KeyboardModifiers modifiers()const{return flags;}};
struct OBSQTDisplay{int moves=0,changes=0;void OnMove(){++moves;}void OnDisplayChange(){++changes;}};
struct QWidget {
 int w=640,h=480,resizes=0;std::vector<OBSQTDisplay*> displays;
 int width()const{return w;}int height()const{return h;}void resize(int x,int y){w=x;h=y;++resizes;}
 template<class T>std::vector<T> findChildren(){return {displays.begin(),displays.end()};}
};
struct QByteArray{};using qintptr=long;
struct MSG{int message;};constexpr int WM_MOVE=1,WM_DISPLAYCHANGE=2;
struct OBSBasicFilters:QWidget{bool nativeEvent(const QByteArray&,void*,qintptr*);};
struct OBSBasicInteraction:QWidget{bool nativeEvent(const QByteArray&,void*,qintptr*);};
struct OBSBasicProperties:QWidget{bool nativeEvent(const QByteArray&,void*,qintptr*);};
'''


class DialogPlatformFacadeTest(unittest.TestCase):
    def test_shared_dialogs_have_no_host_decisions(self):
        for name in FILES:
            with self.subTest(file=name):
                self.assertEqual(ratchet.count_platform_conditionals((DIALOGS/f'{name}.cpp.hpp').read_text()),[])
        for name in CLASSES:
            p=f'frontend/build/frontend-dialogs-{name.lower()}.pch.hpp'
            with self.subTest(pch=p):
                self.assertEqual(ratchet.findings(p,(ROOT/p).read_text()),[])

    def test_every_host_declares_and_defines_the_operations(self):
        header=(UTILITY/'platform.hpp').read_text()
        for signature in SIGNATURES:
            self.assertTrue(signature in header, 'missing declaration: ' + signature)
            for host in ('windows.cpp','osx.mm','x11.cpp'):
                self.assertTrue(signature in (UTILITY/f'platform-{host}.hpp').read_text(), host + ': ' + signature)

    def test_real_qt_display_notifications(self):
        prefix=os.environ.get('OBS_QT_TEST_PREFIX')
        if not prefix:
            self.skipTest('Real Qt hierarchy check runs with the Qt CI prefix')
        helper=function((UTILITY/'platform-windows.cpp.hpp').read_text(),
                        'void HandlePlatformDialogNativeEvent(')
        code=r'''
#include <QApplication>
#include <QDialog>
#include <QWidget>
#include <QByteArray>
#include <cassert>
class OBSQTDisplay:public QWidget {
 Q_OBJECT
public:
 explicit OBSQTDisplay(QWidget *parent=nullptr):QWidget(parent){}
 int moves=0,changes=0;void OnMove(){++moves;}void OnDisplayChange(){++changes;}
};
struct MSG{int message;};constexpr int WM_MOVE=1,WM_DISPLAYCHANGE=2;
class OBSBasicFilters:public QDialog {public:bool nativeEvent(const QByteArray&,void*,qintptr*);};
class OBSBasicInteraction:public QDialog {public:bool nativeEvent(const QByteArray&,void*,qintptr*);};
class OBSBasicProperties:public QDialog {public:bool nativeEvent(const QByteArray&,void*,qintptr*);};
'''+helper+native_methods()+r'''
template<class T>void check(){
 T dialog;QWidget container(&dialog),foreign(&dialog);OBSQTDisplay direct(&dialog),nested(&container);
 QDialog other;OBSQTDisplay outside(&other);qintptr result=99;
 for(int event:{WM_MOVE,WM_DISPLAYCHANGE,999}){MSG message{event};assert(!dialog.nativeEvent("mocked-message",&message,&result));assert(result==99);}
 assert(direct.moves==1&&direct.changes==1&&nested.moves==1&&nested.changes==1);
 assert(outside.moves==0&&outside.changes==0);
 T empty;MSG move{WM_MOVE};assert(!empty.nativeEvent("mocked-message",&move,&result));
}
int main(int argc,char **argv){QApplication app(argc,argv);check<OBSBasicFilters>();check<OBSBasicInteraction>();check<OBSBasicProperties>();}
#include "dialog.moc"
'''
        with tempfile.TemporaryDirectory(prefix='obs-dialog-real-qt-') as tmp:
            Path(tmp,'dialog.cpp').write_text(code)
            Path(tmp,'CMakeLists.txt').write_text(
                'cmake_minimum_required(VERSION 3.28)\nproject(dialog_policy LANGUAGES CXX)\n'
                'find_package(Qt6 REQUIRED COMPONENTS Widgets)\nset(CMAKE_AUTOMOC ON)\n'
                'add_executable(dialog_test dialog.cpp)\ntarget_compile_features(dialog_test PRIVATE cxx_std_17)\n'
                'target_link_libraries(dialog_test PRIVATE Qt6::Widgets)\n')
            build=str(Path(tmp)/'build')
            subprocess.run(['cmake','-S',tmp,'-B',build,'-DCMAKE_PREFIX_PATH='+prefix],check=True)
            subprocess.run(['cmake','--build',build],check=True)
            subprocess.run([str(Path(build)/'dialog_test')],check=True,
                           env={**os.environ,'QT_QPA_PLATFORM':'offscreen'},timeout=10)

    def test_original_policies_and_native_notifications(self):
        compiler=shutil.which('c++')
        if not compiler:
            self.skipTest('C++ compiler unavailable; native frontend validation remains required')
        wrapper=function((DIALOGS/'OBSBasicInteraction.cpp.hpp').read_text(),
                         'static int TranslateQtKeyboardEventModifiers(')
        for host,flags,windows,mac in (
            ('windows.cpp',['-D_WIN32'],True,False),
            ('osx.mm',['-D__APPLE__'],False,True),
            ('x11.cpp',[],False,False),
        ):
            with self.subTest(host=host):
                code=STUBS+policy(host)+native_methods()+wrapper
                code+=f'''
template<class T>void checkEvents(){{
 T dialog;OBSQTDisplay a,b,c;dialog.displays={{&a,&b,&c}};QByteArray eventType;qintptr result=99;
 for(int event:{{WM_MOVE,WM_DISPLAYCHANGE,999}}){{MSG message{{event}};assert(!dialog.nativeEvent(eventType,&message,&result));assert(result==99);}}
 assert(a.moves=={int(windows)}&&b.moves=={int(windows)}&&c.moves=={int(windows)});
 assert(a.changes=={int(windows)}&&b.changes=={int(windows)}&&c.changes=={int(windows)});
 T empty;MSG message{{WM_MOVE}};assert(!empty.nativeEvent(eventType,&message,&result));
}}
int main(){{
 assert(std::string(GetPlatformAuthorsFile())=="{'AUTHORS' if mac else 'authors/AUTHORS'}");
 static_assert(std::is_same_v<decltype(GetPlatformFilterRenameKey()),Qt::Key>);
 assert(GetPlatformFilterRenameKey()==Qt::{'Key_Return' if mac else 'Key_F2'});
 assert(std::string(GetPlatformPropertiesFontFace())=="{'Arial' if windows else 'Helvetica' if mac else 'Monospace'}");
 assert(std::string(GetPlatformPropertiesTextSource())=="{'text_gdiplus' if windows else 'text_ft2_source'}");
 for(uint32_t height:{{0u,1u,2u,5u,100u,250u,300u,16777217u,2147483647u,4294967295u}}){{
  float expected={'height' if windows else 'height*0.8'};float actual=GetPlatformPropertiesLabelHeight(height);
  assert(std::memcmp(&expected,&actual,sizeof(float))==0);
 }}
 QWidget dialog;AdjustPlatformYouTubeDialogSize(&dialog);
 assert(dialog.w=={840 if mac else 640}&&dialog.h=={600 if mac else 480}&&dialog.resizes=={int(mac)});
 for(int bits=0;bits<64;++bits)for(bool mouse:{{false,true}}){{
  QInputEvent event{{{{bits}}}};
  int expected=((bits&1)?INTERACT_SHIFT_KEY:0)|((bits&2)?INTERACT_ALT_KEY:0)|
    ((bits&4)?{'INTERACT_COMMAND_KEY' if mac else 'INTERACT_CONTROL_KEY'}:0)|
    {'((bits&8)?INTERACT_CONTROL_KEY:0)|' if mac else ''}
    ((!mouse&&(bits&16))?INTERACT_IS_KEY_PAD:0);
  assert(TranslateQtKeyboardEventModifiers(&event,mouse)==expected);
 }}
 checkEvents<OBSBasicFilters>();checkEvents<OBSBasicInteraction>();checkEvents<OBSBasicProperties>();
}}
'''
                with tempfile.TemporaryDirectory(prefix='obs-dialog-policy-') as tmp:
                    p=Path(tmp)/'policy.cpp';p.write_text(code);binary=Path(tmp)/'policy'
                    result=subprocess.run([compiler,'-std=c++17','-I',str(ROOT),*flags,str(p),'-o',str(binary)],capture_output=True,text=True)
                    self.assertEqual(result.returncode,0,result.stderr)
                    subprocess.run([str(binary)],check=True)


if __name__=='__main__':
    unittest.main()
