#!/usr/bin/env -S uv run --script
"""VST search/migration policy and private host declaration boundary (#854)."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parent))
import platform_ifdef_ratchet as ratchet
from test_settings_platform_facade import function
ROOT=Path(__file__).resolve().parent.parent
PLUGIN=ROOT/'plugins/obs-vst'
OPS=('QStringList VstPlatformSearchDirs(', 'QStringList VstPlatformFileFilters(',
     'QString VstPlatformStripExtension(', 'const char *VstPlatformMigratePath(')


def bodies(host):
    native=PLUGIN/f'hosts/{host}/vst-platform.{"mm" if host=="macos" else "cpp"}.hpp'
    if native.exists():
        return '\n'.join(function(native.read_text(),op) for op in OPS)
    source=(PLUGIN/'obs-vst.cpp.hpp').read_text()
    fill=function(source,'static void fill_out_plugins(')
    dirs=fill[fill.index('QStringList dir_list;'):fill.index('QStringList filters;')]
    filters=fill[fill.index('QStringList filters;'):fill.index('QStringList vst_list;')]
    strip=re.search(r'#ifdef __APPLE__\n\s*name.remove.*?#endif',fill,re.S)[0]
    migration=re.search(r'#ifdef __linux__\n.*?#endif',source,re.S)[0]
    return ('QStringList VstPlatformSearchDirs(){'+dirs+'return dir_list;}\n'
            'QStringList VstPlatformFileFilters(){'+filters+'return filters;}\n'
            'QString VstPlatformStripExtension(QString name){\n'+strip+'\nreturn name;}\n'
            'const char *VstPlatformMigratePath(obs_data_t *settings,const char *path){\n'+migration+'\nreturn path;}')


COMMON=r'''
#include <string>
#include <vector>
#include <cassert>
#include <cstdlib>
#include <algorithm>
#include <cctype>
using BOOL=int;using HANDLE=void*;int wow=0,queries=0;
HANDLE GetCurrentProcess(){return nullptr;}
int IsWow64Process(HANDLE h,BOOL *value){assert(h==nullptr);++queries;*value=wow;return 1;}
struct obs_data_t{std::string path;int sets=0,gets=0;};
void obs_data_set_string(obs_data_t *data,const char *key,const char *value){assert(std::string(key)=="plugin_path");data->path=value;++data->sets;}
const char *obs_data_get_string(obs_data_t *data,const char *key){assert(std::string(key)=="plugin_path");++data->gets;return data->path.c_str();}
bool flatpak=false;struct QFile{static bool exists(const char *path){assert(std::string(path)=="/.flatpak-info");return flatpak;}};
'''
STUBS=r'''
namespace Qt{enum CaseSensitivity{CaseInsensitive};}
struct QStringList;
struct QString{
 std::string value;bool null=true;
 QString()=default;QString(const char *s):value(s?s:""),null(!s){};QString(std::string s):value(s),null(false){}
 bool isNull()const{return null;}std::string toStdString()const{return value;}
 bool startsWith(const char *s)const{return value.rfind(s,0)==0;}
 QString &replace(const char *from,const char *to){size_t p=0;while((p=value.find(from,p))!=std::string::npos){value.replace(p,std::string(from).size(),to);p+=std::string(to).size();}return *this;}
 QString &remove(const char *part,Qt::CaseSensitivity){std::string lower=value,needle=part;auto fold=[](unsigned char c){return std::tolower(c);};std::transform(needle.begin(),needle.end(),needle.begin(),fold);size_t p;for(;;){lower=value;std::transform(lower.begin(),lower.end(),lower.begin(),fold);p=lower.find(needle);if(p==std::string::npos)break;value.erase(p,needle.size());}return *this;}
 QStringList split(const char *delimiter)const;
 friend QString operator+(const QString &a,const char *b){return a.value+b;}
};
struct QStringList:std::vector<QString>{QStringList &operator<<(const QString &s){push_back(s);return *this;}void append(const QStringList &v){insert(end(),v.begin(),v.end());}};
QStringList QString::split(const char *delimiter)const{assert(std::string(delimiter)==":");QStringList out;size_t pos=0;for(;;){size_t next=value.find(':',pos);out<<QString(value.substr(pos,next-pos));if(next==std::string::npos)break;pos=next+1;}return out;}
QString qEnvironmentVariable(const char *name){return QString(getenv(name));}
'''


def checks(host,win64,bsd):
    win=host=='windows';mac=host=='macos';linux=host=='linux' and not bsd
    expected=(['/Library/Audio/Plug-Ins/VST/','~/Library/Audio/Plug-ins/VST/'] if mac else
              ['/pf/Steinberg/VstPlugins/','/common/Steinberg/Shared Components/','/common/VST2','/common/Steinberg/VST2','/common/VSTPlugins/','/pf/VSTPlugins/'] if win else
              [f'{base}/{name}/' for base in ('/usr/lib','/usr/lib64','/usr/local/lib','/usr/local/lib64') for name in ('vst','lxvst','linux_vst')]+['/home/test/.vst/','/home/test/.lxvst/'] if linux else [])
    def strings(values):
        return '{'+','.join('"'+v+'"' for v in values)+'}'
    filters=['*.dll'] if win else ['*.vst'] if mac else ['*.so','*.o'] if linux else []
    ext='DLL' if win else 'VST' if mac else 'SO' if linux else 'other'
    wowdirs=['/pf86/Steinberg/VstPlugins/','/common86/Steinberg/Shared Components/','/common86/VST2','/common86/VSTPlugins/','/pf86/VSTPlugins/']
    return f'''
std::vector<std::string> values(const QStringList &list){{std::vector<std::string> out;for(const auto &v:list)out.push_back(v.toStdString());return out;}}
int main(){{
 setenv("ProgramFiles","/pf",1);setenv("CommonProgramFiles","/common",1);
 setenv("ProgramFiles(x86)","/pf86",1);setenv("CommonProgramFiles(x86)","/common86",1);
 setenv("HOME","/home/test",1);unsetenv("VST_PATH");
 assert(values(VstPlatformSearchDirs())==std::vector<std::string>({strings(expected)}));
 assert(values(VstPlatformFileFilters())==std::vector<std::string>({strings(filters)}));
 wow=1;
 assert(values(VstPlatformSearchDirs())==std::vector<std::string>({strings(wowdirs if win and not win64 else expected)}));
 assert(queries=={2 if win and not win64 else 0});
 setenv("VST_PATH","",1);auto empty=values(VstPlatformSearchDirs());
 assert(empty==std::vector<std::string>({strings([''] if linux else wowdirs if win and not win64 else expected)}));
 setenv("VST_PATH",":/x::/y:",1);auto split=values(VstPlatformSearchDirs());
 assert(split==std::vector<std::string>({strings(['','/x','','/y',''] if linux else wowdirs if win and not win64 else expected)}));
 assert(VstPlatformStripExtension(QString("A.{ext}.backup.{ext.lower()}")).toStdString()=="{'A.backup' if not bsd else 'A.other.backup.other'}");
 assert(VstPlatformStripExtension(QString("plain")).toStdString()=="plain");
 assert(VstPlatformStripExtension(QString("a.so.backup.o")).toStdString()=="{'a.backup' if linux else 'a.so.backup.o'}");
 for(bool enabled:{{false,true}})for(const char *path:{{"","/other/a.so","/app/extensions/Plugins/lxvst/a.so","/app/extensions/Plugins/lxvst-suffix/a.so"}}){{
  flatpak=enabled;obs_data_t settings;const char *result=VstPlatformMigratePath(&settings,path);
  bool migrated={str(linux).lower()}&&enabled&&std::string(path).rfind("/app/extensions/Plugins/lxvst",0)==0;
  std::string expected=path;if(migrated)expected.replace(0,29,"/app/extensions/Plugins/vst");
  assert(std::string(result)==expected);assert(settings.sets==int(migrated)&&settings.gets==int(migrated));
  if(!migrated)assert(result==path);
 }}
}}
'''


class VstPlatformFacadeTest(unittest.TestCase):
    def test_shared_boundaries(self):
        for file in ('obs-vst.cpp.hpp','headers/EditorWidget.h','headers/VSTPlugin.h'):
            with self.subTest(file=file):
                self.assertEqual(ratchet.findings('plugins/obs-vst/'+file,(PLUGIN/file).read_text()),[])

    def test_host_wiring(self):
        self.assertTrue((PLUGIN/'headers/vst-platform.hpp').is_file())
        for host in ('windows','macos','linux'):
            self.assertTrue((PLUGIN/f'hosts/{host}/vst-platform.{"mm" if host=="macos" else "cpp"}.hpp').is_file())
        cmake=(PLUGIN/'CMakeLists.txt').read_text()
        for host in ('windows','macos','linux'):
            self.assertIn('hosts/'+host,cmake)

    def test_real_qt_policy(self):
        prefix=os.environ.get('OBS_QT_TEST_PREFIX')
        if not prefix:
            self.skipTest('Real Qt policy test runs with the CI Qt prefix')
        compiler=shutil.which('c++')
        with tempfile.TemporaryDirectory(prefix='obs-vst-real-qt-') as tmp:
            cmake='cmake_minimum_required(VERSION 3.28)\nproject(vst_policy LANGUAGES CXX)\nfind_package(Qt6 REQUIRED COMPONENTS Core)\n'
            variants=[('windows',['-U__linux__','-DWIN32','-D_WIN32','-D_WIN64'],True,False),('windows',['-U__linux__','-DWIN32','-D_WIN32'],False,False),('macos',['-U__linux__','-D__APPLE__'],False,False),('linux',[],False,False),('linux',['-U__linux__'],False,True)]
            for i,(host,flags,win64,bsd) in enumerate(variants):
                selected=subprocess.run([compiler,'-E','-P','-x','c++',*flags,'-'],input=bodies(host),capture_output=True,text=True,check=True).stdout
                Path(tmp,f'policy{i}.cpp').write_text('#include <QString>\n#include <QStringList>\n'+COMMON+selected+checks(host,win64,bsd))
                cmake+=f'add_executable(policy{i} policy{i}.cpp)\ntarget_compile_features(policy{i} PRIVATE cxx_std_17)\ntarget_link_libraries(policy{i} PRIVATE Qt6::Core)\n'
            Path(tmp,'CMakeLists.txt').write_text(cmake)
            build=str(Path(tmp)/'build')
            subprocess.run(['cmake','-S',tmp,'-B',build,'-DCMAKE_PREFIX_PATH='+prefix],check=True)
            subprocess.run(['cmake','--build',build],check=True)
            for i in range(len(variants)):
                subprocess.run([str(Path(build)/f'policy{i}')],check=True,timeout=10)

    def test_cmake_selects_unchanged_member_declarations(self):
        compiler=shutil.which('c++')
        if not compiler:
            self.skipTest('Native compiler unavailable')
        cmake=(PLUGIN/'CMakeLists.txt').read_text()
        command=re.search(r'target_include_directories\(.*?\n\)',cmake,re.S)
        self.assertIsNotNone(command)
        for system,host,flags,editor,module in [
            ('Windows','windows',['-U__linux__','-D_WIN32'], 'HWND windowHandle = NULL;', 'HINSTANCE dllHandle = nullptr;'),
            ('Darwin','macos',['-U__linux__','-D__APPLE__'], 'QWidget *cocoaViewContainer = NULL;', 'CFBundleRef bundle = NULL;'),
            ('Linux','linux',[], '', 'void *soHandle = nullptr;'),
            ('FreeBSD','linux',['-U__linux__'], '', ''),
        ]:
            with self.subTest(system=system),tempfile.TemporaryDirectory(prefix='obs-vst-types-') as tmp:
                Path(tmp,'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.28)\nproject(vst_header NONE)\nadd_library(obs-vst INTERFACE)\n'+command[0].replace('PRIVATE','INTERFACE')+'\nfile(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/selection.txt" CONTENT "$<TARGET_GENEX_EVAL:obs-vst,$<TARGET_PROPERTY:obs-vst,INTERFACE_INCLUDE_DIRECTORIES>>")\n')
                build=Path(tmp)/'build'
                result=subprocess.run(['cmake','-S',tmp,'-B',str(build),'-DCMAKE_SYSTEM_NAME='+system],capture_output=True,text=True)
                self.assertEqual(result.returncode,0,result.stderr)
                selected=(build/'selection.txt').read_text().split(';')
                self.assertEqual(selected[-1],str(Path(tmp)/f'hosts/{host}'))
                for name,expected in [('vst-editor-members.inc',editor),('vst-module-members.inc',module)]:
                    file=PLUGIN/f'hosts/{host}/{name}'
                    self.assertTrue(file.is_file())
                    result=subprocess.run([compiler,'-E','-P','-x','c++',*flags,str(file)],capture_output=True,text=True,check=True)
                    self.assertEqual(re.sub(r'\s+','',result.stdout),re.sub(r'\s+','',expected))

    def test_alternate_build_consumers_select_host_headers(self):
        # Both compile the real VST groups independently from the plugin target.
        cmake=(ROOT/'test/cmocka/tests.d/test_vst_editor_close.cmake').read_text()
        self.assertRegex(cmake,r'target_include_directories\(\s*test_vst_editor_close_module\s+PRIVATE[^)]*\$\{_vst_dir\}/hosts/linux')
        cargo=(ROOT/'rust/obs-build/src/platform/mod.rs').read_text()
        configure=function(cargo,'pub fn configure_vst(')
        self.assertIn('root.join("plugins/obs-vst/hosts/linux")',configure)

    def test_original_policy(self):
        compiler=shutil.which('c++')
        if not compiler:
            self.skipTest('Native compiler unavailable')
        for host,flags,win64,bsd in [('windows',['-U__linux__','-DWIN32','-D_WIN32','-D_WIN64'],True,False),('windows',['-U__linux__','-DWIN32','-D_WIN32'],False,False),('macos',['-U__linux__','-D__APPLE__'],False,False),('linux',[],False,False),('linux',['-U__linux__'],False,True)]:
            with self.subTest(host=host,flags=flags),tempfile.TemporaryDirectory(prefix='obs-vst-policy-') as tmp:
                source=Path(tmp)/'policy.cpp';binary=Path(tmp)/'policy'
                source.write_text(COMMON+STUBS+bodies(host)+checks(host,win64,bsd))
                r=subprocess.run([compiler,'-std=c++17',*flags,str(source),'-o',str(binary)],capture_output=True,text=True)
                self.assertEqual(r.returncode,0,r.stderr)
                subprocess.run([str(binary)],check=True)


if __name__=='__main__':
    unittest.main()
