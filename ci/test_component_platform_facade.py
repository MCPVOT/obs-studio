#!/usr/bin/env -S uv run --script
"""Characterize component host policies before and after facade extraction (#877)."""
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import platform_ifdef_ratchet as ratchet
from test_settings_platform_facade import function

ROOT = Path(__file__).resolve().parent.parent
COMP = ROOT / 'frontend/components'
FILES = ('AudioCaptureToolbar', 'DisplayCaptureToolbar', 'WindowCaptureToolbar',
         'ColorSourceToolbar', 'TextSourceToolbar', 'Multiview', 'SourceTree',
         'SourceTreeItem', 'UIValidation', 'VisibilityItemDelegate')
OPS = ('const char *GetPlatformCaptureModuleId(', 'const char *GetPlatformCaptureLabelKey(',
       'const char *GetPlatformCaptureProperty(', 'bool PlatformNeedsNonNativeFontDialog(',
       'void ConfigurePlatformSourceItemWidget(', 'bool PlatformEndsSourceEdit(',
       'bool PlatformNeedsDeferredSourceProperties(',
       'QMessageBox::ButtonRole GetPlatformStreamWarningButtonRole(',
       'QPalette::ColorGroup GetPlatformVisibilityPaletteGroup(',
       'QPalette::ColorRole GetPlatformVisibilityHighlightRole(')


def text(name):
    return (COMP / (name + '.cpp.hpp')).read_text()


def blocks(name):
    return re.findall(r'^#if[^\n]*\n.*?^#endif', text(name), re.M | re.S)


def bodies(host):
    native = (ROOT / f'frontend/utility/platform-{host}.hpp').read_text()
    if OPS[0] in native:
        return '\n'.join(function(native, op) for op in OPS)
    # Use actual old preprocessor branches, not a second table of policies.
    result = []
    for suffix, index in [('ModuleId', 0), ('LabelKey', 1)]:
        name = 'get_os_module' if index == 0 else 'get_os_text'
        args = []
        for file in FILES[:3]:
            call = re.search(r' = ' + name + r'\((.*?)\);', text(file))[1]
            args.append(call if index == 0 else call.removeprefix('mod, '))
        macro = blocks('AudioCaptureToolbar')[0]
        macro = re.sub(r'obs_get_module\((\w+)\)', r'\1', macro)
        macro = re.sub(r'obs_module_get_locale_text\(mod, (\w+)\)', r'\1', macro)
        result.append(f'const char *GetPlatformCapture{suffix}(PlatformCaptureKind kind){{\n' + macro + '\nswitch(kind){\n' +
                      '\n'.join(f'case PlatformCaptureKind::{kind}:return {name}({"nullptr, " if index else ""}{arg});'
                                for kind, arg in zip(('Audio', 'Display', 'Window'), args)) +
                      '\n}return nullptr;\n}\n#undef get_os_module\n#undef get_os_text\n')
    result.append('const char *GetPlatformCaptureProperty(PlatformCaptureKind kind,bool &is_int){const char *prop_name="device_id";'
                  'if(kind==PlatformCaptureKind::Display){\n' + blocks('DisplayCaptureToolbar')[1] +
                  '\n}else if(kind==PlatformCaptureKind::Window){\n' + '\n'.join(blocks('WindowCaptureToolbar')[1:]) +
                  '\n}return prop_name;}')
    result.append('bool PlatformNeedsNonNativeFontDialog(){int options=0;\n' + blocks('TextSourceToolbar')[0] + '\nreturn options!=0;}')
    widget = blocks('SourceTreeItem')[0].replace('vis->', 'widget->')
    widget = re.sub(r'^\s*lock->.*$', '', widget, flags=re.M)
    result.append('void ConfigurePlatformSourceItemWidget(QWidget *widget){\n' + widget + '\n}')
    end = blocks('SourceTree')[0].replace('itemWidget->ExitEditMode(true);', 'return true;')
    result.append('bool PlatformEndsSourceEdit(){\n' + end + '\nreturn false;}')
    deferred = blocks('SourceTreeItem')[1]
    deferred = re.sub(r'if \(snapEnabled\).*?#endif', 'return snapEnabled;\n#else\nreturn false;\n#endif', deferred, flags=re.S)
    result.append('bool PlatformNeedsDeferredSourceProperties(){\n' + deferred + '\n}')
    roles = blocks('UIValidation')[0].replace('#define ACCEPT_BUTTON', 'accept =').replace('#define REJECT_BUTTON', 'reject =')
    roles = re.sub(r'(accept|reject) = (QMessageBox::\w+)', r'\1 = \2;', roles)
    result.append('QMessageBox::ButtonRole GetPlatformStreamWarningButtonRole(bool settings){QMessageBox::ButtonRole accept,reject;\n' + roles + '\nreturn settings?accept:reject;}')
    group, highlight = blocks('VisibilityItemDelegate')
    result.append('QPalette::ColorGroup GetPlatformVisibilityPaletteGroup(bool active){\n' + group + '\nreturn group;}')
    result.append('QPalette::ColorRole GetPlatformVisibilityHighlightRole(){\n' + highlight + '\nreturn highlightRole;}')
    return '\n'.join(result)


STUBS = r'''
#include <cassert>
#include <string>
enum class PlatformCaptureKind{Audio,Display,Window};
namespace Qt{constexpr int WA_LayoutUsesWidgetRect=7;}
struct QWidget{int attributes=0;void setAttribute(int value){assert(value==7);++attributes;}};
struct QFontDialog{enum{DontUseNativeDialog=1};};
struct QMessageBox{enum ButtonRole{AcceptRole,ResetRole,NoRole};};
struct QPalette{enum ColorGroup{Active,Inactive};enum ColorRole{WindowText,HighlightedText};};
using BOOL=int;constexpr int FALSE=0,SPI_GETSNAPTODEFBUTTON=9;
int snap=0,queries=0;
int SystemParametersInfo(int action,int zero,BOOL *result,int flags){assert(action==9&&zero==0&&flags==0);++queries;*result=snap;return 0;}
'''


class ComponentPlatformFacadeTest(unittest.TestCase):
    def test_shared_components_have_no_host_decisions(self):
        for name in FILES:
            with self.subTest(file=name):
                self.assertEqual(ratchet.count_platform_conditionals(text(name)), [])
        p='frontend/build/frontend-components-sourcetreeitem.pch.hpp'
        self.assertEqual(ratchet.findings(p,(ROOT/p).read_text()), [])

    def test_each_host_declares_and_defines_operations(self):
        for op in OPS:
            self.assertIn(op,(ROOT/'frontend/utility/platform.hpp').read_text())
            for host in ('windows.cpp','osx.mm','x11.cpp'):
                self.assertIn(op,(ROOT/f'frontend/utility/platform-{host}.hpp').read_text())

    def test_capture_callers_and_missing_modules(self):
        compiler=shutil.which('c++')
        if not compiler:
            self.skipTest('Native compiler unavailable')
        for host,flags,win,mac in [('windows.cpp',['-D_WIN32'],True,False),('osx.mm',['-D__APPLE__'],False,True),('x11.cpp',[],False,False)]:
            with self.subTest(host=host):
                code=STUBS+bodies(host)+r'''
struct Button{};
struct Label{std::string value;void setText(const char *v){value=v;}};
struct UI{Button *activateButton=new Button;Label *deviceLabel=new Label;~UI(){delete activateButton;delete deviceLabel;}};
struct ComboSelectToolbar{UI *ui=new UI;const char *prop_name=nullptr;bool is_int=false;int initialized=0;void Init(){++initialized;}~ComboSelectToolbar(){delete ui;}};
struct AudioCaptureToolbar:ComboSelectToolbar{void Init();};
struct DisplayCaptureToolbar:ComboSelectToolbar{void Init();};
struct WindowCaptureToolbar:ComboSelectToolbar{void Init();};
struct obs_module_t{};obs_module_t module;bool available=true;int moduleCalls=0,localeCalls=0;std::string moduleId;
obs_module_t *obs_get_module(const char *id){++moduleCalls;moduleId=id;return available?&module:nullptr;}
const char *obs_module_get_locale_text(obs_module_t *mod,const char *key){assert(mod==&module);++localeCalls;return key;}
'''
                for name in FILES[:3]:
                    original=text(name)
                    if '#define get_os_module' in original:
                        code+=blocks(name)[0]+'\n'
                    code+=function(original,f'void {name}::Init(')+'\n#undef get_os_module\n#undef get_os_text\n'
                code+=r'''
template<class T>void check(PlatformCaptureKind kind){
 for(available=false;;available=true){
  moduleCalls=localeCalls=0;T toolbar;toolbar.Init();
  assert(toolbar.ui->activateButton==nullptr&&moduleCalls==1);
  assert(moduleId==GetPlatformCaptureModuleId(kind));
  if(available){
   assert(localeCalls==1&&toolbar.initialized==1);
   assert(toolbar.ui->deviceLabel->value==GetPlatformCaptureLabelKey(kind));
   bool expected=false;const char *property=GetPlatformCaptureProperty(kind,expected);
   assert(std::string(toolbar.prop_name)==property&&toolbar.is_int==expected);break;
  }
  assert(localeCalls==0&&toolbar.initialized==0&&toolbar.prop_name==nullptr&&!toolbar.is_int);
 }
}
int main(){check<AudioCaptureToolbar>(PlatformCaptureKind::Audio);check<DisplayCaptureToolbar>(PlatformCaptureKind::Display);check<WindowCaptureToolbar>(PlatformCaptureKind::Window);}
'''
                with tempfile.TemporaryDirectory(prefix='obs-capture-callers-') as tmp:
                    source=Path(tmp)/'capture.cpp';binary=Path(tmp)/'capture';source.write_text(code)
                    result=subprocess.run([compiler,'-std=c++17',*flags,str(source),'-o',str(binary)],capture_output=True,text=True)
                    self.assertEqual(result.returncode,0,result.stderr)
                    subprocess.run([str(binary)],check=True)

    def test_original_host_policy(self):
        compiler=shutil.which('c++')
        if not compiler:
            self.skipTest('Native compiler unavailable')
        for host,flags,win,mac in [('windows.cpp',['-D_WIN32'],True,False),('osx.mm',['-D__APPLE__'],False,True),('x11.cpp',[],False,False)]:
            with self.subTest(host=host):
                checks=f'''
int main(){{
 const PlatformCaptureKind kinds[]={{PlatformCaptureKind::Audio,PlatformCaptureKind::Display,PlatformCaptureKind::Window}};
 const char *modules[]={{{'"win-wasapi","win-capture","win-capture"' if win else '"mac-capture","mac-capture","mac-capture"' if mac else '"linux-pulseaudio","linux-capture","linux-capture"'}}};
 const char *labels[]={{{'"Device","Monitor","WindowCapture.Window"' if win else '"CoreAudio.Device","DisplayCapture.Display","WindowUtils.Window"' if mac else '"Device","Screen","Window"'}}};
 const char *props[]={{"device_id","{'monitor_id' if win else 'display_uuid' if mac else 'screen'}","{'window' if win or mac else 'capture_window'}"}};
 for(int i=0;i<3;++i){{
  assert(std::string(GetPlatformCaptureModuleId(kinds[i]))==modules[i]);
  assert(std::string(GetPlatformCaptureLabelKey(kinds[i]))==labels[i]);
  for(bool initial:{{false,true}}){{bool is_int=initial;assert(std::string(GetPlatformCaptureProperty(kinds[i],is_int))==props[i]);assert(is_int==(initial||(i==1&&{str(not win and not mac).lower()})||(i==2&&{str(mac).lower()})));}}
 }}
 assert(PlatformNeedsNonNativeFontDialog()=={str(not win).lower()});
 QWidget widget;ConfigurePlatformSourceItemWidget(&widget);assert(widget.attributes=={int(mac)});
 assert(PlatformEndsSourceEdit()=={str(mac).lower()});
 for(snap=0;snap<2;++snap){{assert(PlatformNeedsDeferredSourceProperties()==({str(win).lower()}&&snap));}}
 assert(queries=={2 if win else 0});
 assert(GetPlatformStreamWarningButtonRole(true)==QMessageBox::{'AcceptRole' if mac else 'NoRole'});
 assert(GetPlatformStreamWarningButtonRole(false)==QMessageBox::{'ResetRole' if mac else 'NoRole'});
 assert(GetPlatformVisibilityPaletteGroup(true)==QPalette::Active);
 assert(GetPlatformVisibilityPaletteGroup(false)==QPalette::{'Inactive' if win or mac else 'Active'});
 assert(GetPlatformVisibilityHighlightRole()==QPalette::{'WindowText' if win else 'HighlightedText'});
}}
'''
                with tempfile.TemporaryDirectory(prefix='obs-component-policy-') as tmp:
                    source=Path(tmp)/'policy.cpp';binary=Path(tmp)/'policy'
                    source.write_text(STUBS+bodies(host)+checks)
                    r=subprocess.run([compiler,'-std=c++17',*flags,str(source),'-o',str(binary)],capture_output=True,text=True)
                    self.assertEqual(r.returncode,0,r.stderr)
                    subprocess.run([str(binary)],check=True)


if __name__=='__main__':
    unittest.main()
