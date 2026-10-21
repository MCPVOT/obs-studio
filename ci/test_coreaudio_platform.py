#!/usr/bin/env -S uv run --script
"""CoreAudio status/loader ownership and same-TU import-table contract (#856)."""
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import platform_ifdef_ratchet as ratchet
from ci_scope import scope
from test_settings_platform_facade import function

ROOT = Path(__file__).resolve().parent.parent
PLUGIN = ROOT / 'plugins/coreaudio-encoder'
SOURCE = PLUGIN / 'encoder.cpp.hpp'


class CoreAudioPlatformTest(unittest.TestCase):
    def test_shared_host_boundary_with_compiler_exceptions(self):
        for p in (SOURCE, PLUGIN / 'build/coreaudio-encoder.pch.hpp'):
            decisions = ratchet.count_platform_conditionals(p.read_text())
            # The issue explicitly permits the existing compiler-only format
            # attribute and warning push/pop. It permits no shared OS branch.
            self.assertEqual([d for d in decisions if '_MSC_VER' not in d[1]], [])
            self.assertEqual(len(decisions), 3 if p == SOURCE else 0)

    def test_host_groups_keep_the_import_table_with_the_encoder(self):
        cmake = (PLUGIN / 'CMakeLists.txt').read_text()
        for host in ('windows', 'macos'):
            group = PLUGIN / f'build/coreaudio-encoder.{host}+.hpp.cpp'
            self.assertTrue(group.exists(), str(group))
            text = group.read_text()
            self.assertIn('plugins/coreaudio-encoder/encoder.cpp.hpp', text)
            self.assertIn(f'plugins/coreaudio-encoder/hosts/{host}/coreaudio-platform.cpp.hpp', text)
            self.assertIn(group.name, cmake)
        self.assertNotIn('build/coreaudio-encoder.core+.hpp.cpp', cmake)
        windows = (PLUGIN / 'hosts/windows/coreaudio-platform.cpp.hpp').read_text()
        macos = (PLUGIN / 'hosts/macos/coreaudio-platform.cpp.hpp').read_text()
        self.assertIn('void obs_module_unload(', windows)
        self.assertNotIn('obs_module_unload(', macos)

    def test_host_and_shared_paths_select_the_right_ci(self):
        for path, windows, macos in (
            ('build/coreaudio-encoder.windows+.hpp.cpp', True, False),
            ('hosts/windows/coreaudio-platform.cpp.hpp', True, False),
            ('hosts/windows/coreaudio-platform-types.hpp', True, False),
            ('build/coreaudio-encoder.macos+.hpp.cpp', False, True),
            ('hosts/macos/coreaudio-platform.cpp.hpp', False, True),
            ('hosts/macos/coreaudio-platform-types.hpp', False, True),
            ('coreaudio-platform.hpp', True, True),
            ('build/coreaudio-encoder.pch.hpp', True, True),
            ('encoder.cpp.hpp', True, True),
        ):
            with self.subTest(path=path):
                result = scope(['plugins/coreaudio-encoder/' + path])
                self.assertEqual((result.windows, result.macos), (windows, macos))

    def test_status_load_registration_and_resource_ownership(self):
        compiler = shutil.which('c++')
        if not compiler:
            self.skipTest('C++ compiler unavailable; native plugin validation remains required')
        text = SOURCE.read_text()
        status = function(text, 'static DStr osstatus_to_dstr(')
        code_names = function(text, 'static const char *code_to_str(')
        codes = re.findall(r'HANDLE_CODE\((\w+)\);', code_names)
        module_load = function(text, 'bool obs_module_load(')
        assignments = re.findall(r'aac_info\.(\w+)\s*=\s*([^;]+);', module_load)
        callbacks = {value for _, value in assignments if value.startswith('aac_')}
        info_fields = '\n'.join('void (*'+name+')();' for name, value in assignments if value in callbacks)
        info_fields += '\nconst char *id;const char *codec;int type;\n'
        declarations = '\n'.join(f'void {name}() {{}}' for name in sorted(callbacks))
        shared_deleter = function(text, 'template<> struct default_delete<remove_pointer<AudioConverterRef>::type>') + ';'
        for host, flags, windows in (('windows', ['-D_WIN32'], True), ('macos', [], False)):
            with self.subTest(host=host):
                host_source = PLUGIN / f'hosts/{host}/coreaudio-platform.cpp.hpp'
                types = PLUGIN / f'hosts/{host}/coreaudio-platform-types.hpp'
                if host_source.exists():
                    helpers = '\n'.join(function(host_source.read_text(), signature) for signature in (
                        'bool ca_platform_load(', 'void ca_platform_unload(', 'bool ca_platform_describe_status('))
                    deleters = '' if windows else types.read_text()[types.read_text().index('namespace std'):]
                    unload = function(host_source.read_text(), 'void obs_module_unload(') if windows else ''
                else:
                    helpers = ''
                    match = re.search(r'#ifndef _WIN32\n(template<> struct default_delete.*?)#endif', text, re.S)
                    deleters = 'namespace std {\n' + match[1] + '\n}' if not windows else ''
                    unload = function(text, 'void obs_module_unload(') if windows else ''
                preamble = r'''
#include <memory>
#include <type_traits>
#include <string>
#include <vector>
#include <cassert>
#include <cstdio>
using namespace std;
using OSStatus=int;
struct Converter{};using AudioConverterRef=Converter*;
struct Error{int code;};using CFErrorRef=const Error*;
struct String{std::string value;};using CFStringRef=const String*;
static int errorsFreed=0,stringsFreed=0,disposals=0,loads=0,unloads=0,registered=0;
static bool copyWorks=true,loadWorks=true;
static vector<string> logs;static vector<int> levels;
static OSStatus dispose(AudioConverterRef p){++disposals;delete p;return 0;}
static OSStatus (*AudioConverterDispose)(AudioConverterRef)=nullptr;
static bool load_core_audio(){++loads;if(!loadWorks)return false;AudioConverterDispose=dispose;return true;}
static void unload_core_audio(){++unloads;AudioConverterDispose=nullptr;}
static void record_log(int level,const char *message){levels.push_back(level);logs.emplace_back(message);}
#define CA_LOG(level,...) record_log(level,__VA_ARGS__)
constexpr int LOG_WARNING=1,LOG_INFO=2,OBS_ENCODER_AUDIO=3;
constexpr int kCFAllocatorDefault=0,kCFErrorDomainOSStatus=1,kCFStringEncodingUTF8=2;
CFErrorRef CFErrorCreate(int,int,int code,void*){return new Error{code};}
CFStringRef CFErrorCopyDescription(CFErrorRef e){return new String{"native:"+to_string(e->code)};}
void CFRelease(CFErrorRef p){++errorsFreed;delete p;}
void CFRelease(CFStringRef p){++stringsFreed;delete p;}
struct DStr{string value;};
bool cfstr_copy_dstr(CFStringRef s,int encoding,DStr &out){assert(encoding==2);if(!copyWorks)return false;out.value=s->value;return true;}
void dstr_printf(DStr &out,const char *fmt,const char *name,const char *left,int code,const char *right){char buf[512];snprintf(buf,sizeof(buf),fmt,name,left,code,right);out.value=buf;}
''' + deleters + '\nnamespace std {' + shared_deleter + '}\n'
                preamble += 'template<typename T> using cf_ptr=unique_ptr<typename remove_pointer<T>::type>;\n'
                preamble += '\n'.join(f'constexpr OSStatus {name}={-1000-i};' for i,name in enumerate(codes))
                preamble += '\nstruct obs_encoder_info {' + info_fields + '};\n' + declarations
                preamble += '\nvoid obs_register_encoder(obs_encoder_info *info){++registered;assert(string(info->id)=="CoreAudio_AAC");assert(string(info->codec)=="aac");}\n'
                checks = f'''
int main() {{
 for(bool copy:{{false,true}})for(OSStatus code:{{kAudio_ParamError,42,-1234}}) {{
  errorsFreed=stringsFreed=0;copyWorks=copy;
  DStr out=osstatus_to_dstr(code);
  string fallback=code==kAudio_ParamError?"kAudio_ParamError ("+to_string(code)+")":to_string(code);
  assert(out.value==({'fallback' if windows else 'copy?"native:"+to_string(code):fallback'}));
  assert(errorsFreed=={0 if windows else 1}&&stringsFreed=={0 if windows else 1});
 }}
 for(bool available:{{false,true}}) {{
  loads=unloads=registered=disposals=0;logs.clear();levels.clear();loadWorks=available;AudioConverterDispose={'nullptr' if windows else 'dispose'};
  assert(obs_module_load());
  assert(loads=={int(windows)});assert(registered==({'int(available)' if windows else '1'}));
  assert(logs.size()=={int(windows)});
  {'assert(levels[0]==(available?LOG_INFO:LOG_WARNING));assert(logs[0]==(available?"Adding CoreAudio AAC encoder":"CoreAudio AAC encoder not installed on the system or couldn\'t be loaded"));' if windows else ''}
  if({'available' if windows else 'true'}) {{cf_ptr<AudioConverterRef> converter{{new Converter}};}}
  assert(disposals==({'int(available)' if windows else '1'}));
  {'obs_module_unload();assert(unloads==1);assert(AudioConverterDispose==nullptr);' if windows else 'assert(unloads==0);'}
 }}
}}
'''
                with tempfile.TemporaryDirectory(prefix='obs-coreaudio-policy-') as tmp:
                    p=Path(tmp)/'policy.cpp';p.write_text(preamble+helpers+code_names+status+module_load+unload+checks)
                    binary=Path(tmp)/'policy'
                    result=subprocess.run([compiler,'-std=c++17',*flags,str(p),'-o',str(binary)],capture_output=True,text=True)
                    self.assertEqual(result.returncode,0,result.stderr)
                    subprocess.run([str(binary)],check=True)


if __name__=='__main__':
    unittest.main()
