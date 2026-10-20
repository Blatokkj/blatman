#!/usr/bin/env python3
"""Offline integration checks. Only creates files beneath the selected test directory."""
from pathlib import Path
import json,os,stat,subprocess,sys,tempfile
B=Path(sys.argv[1]);P=Path(sys.argv[2]);area=Path(sys.argv[3]);area.mkdir(parents=True,exist_ok=True)
R=Path(tempfile.mkdtemp(prefix='run with space-',dir=area));checks=[]
def write(p,data='content',mode=0o644):
 p.parent.mkdir(parents=True,exist_ok=True);p.write_text(data);p.chmod(mode)
def publish(name,setup,expected,verify=lambda s,d:None):
 s=R/name/'stage';d=R/name/'dest';s.mkdir(parents=True);d.mkdir();setup(s,d)
 p=subprocess.run([str(P),str(s),str(d)],capture_output=True,text=True)
 (s.parent/'log').write_text(p.stdout+p.stderr)
 assert p.returncode==expected,(name,p.stderr)
 verify(s,d);checks.append(name);print('PASS',name,flush=True)
def links(s,d):
 write(s/'usr/lib/a.so.1');(s/'usr/lib/a.so').symlink_to('a.so.1');(s/'usr/lib/absolute').symlink_to('/usr/lib/a.so');(s/'usr/lib/directory').symlink_to('.')
def modes(s,d):assert stat.S_IMODE((d/'usr/bin/tool').stat().st_mode)==0o755
publish('mode-normalized',lambda s,d:write(s/'usr/bin/tool','ok',0o777),0,modes)
publish('valid-links',links,0)
publish('empty-stage',lambda s,d:None,1)
publish('outside-prefix',lambda s,d:write(s/'etc/config'),1)
publish('dangling-link',lambda s,d:((s/'usr').mkdir(),(s/'usr/a').symlink_to('missing')),1)
publish('escaping-link',lambda s,d:((s/'usr').mkdir(),(s/'usr/a').symlink_to('../../etc/passwd')),1)
publish('link-cycle',lambda s,d:((s/'usr').mkdir(),(s/'usr/a').symlink_to('b'),(s/'usr/b').symlink_to('a')),1)
publish('special-permissions',lambda s,d:write(s/'usr/bin/bad','x',0o4755),1)
publish('fifo',lambda s,d:((s/'usr').mkdir(),os.mkfifo(s/'usr/fifo')),1)
def conflict(s,d):write(s/'usr/bin/a');write(s/'usr/bin/z');write(d/'usr/bin/z','keep')
def kept(s,d):assert not (d/'usr/bin/a').exists();assert (d/'usr/bin/z').read_text()=='keep'
publish('preflight-conflict',conflict,1,kept)
def parentlink(s,d):
 write(s/'usr/bin/app');(d/'usr').mkdir();(d/'outside').mkdir();(d/'usr/bin').symlink_to('../outside')
publish('destination-parent-link',parentlink,1)
def partial(s,d):
 write(s/'usr/a/app');write(s/'usr/z/app');(d/'usr/z').mkdir(parents=True);(d/'usr/z').chmod(0o555)
def reverted(s,d):assert not (d/'usr/a').exists();(d/'usr/z').chmod(0o755)
if os.geteuid()!=0:publish('rollback-copy-failure',partial,1,reverted)

def repo(name,files):
 p=R/'blatman-cache/example.com/probe'/name;p.mkdir(parents=True)
 for name,data in files.items():write(p/name,data)
 for command in [['git','init','-q'],['git','add','.'],['git','-c','user.name=test','-c','user.email=test@example.com','commit','-qm','fixture'],['git','remote','add','origin','https://example.com/probe/'+p.name+'.git']]:subprocess.run(command,cwd=p,check=True,capture_output=True)
 return p
root=R/'installed';root.mkdir();tmp=R/'tmp';tmp.mkdir()
env=os.environ.copy();env.update(BLATMAN_DESTDIR=str(root),TMPDIR=str(tmp))
def invoke(name,args,expected=0,answers='',extra={}):
 p=subprocess.run([str(B)]+args,cwd=R,env={**env,**extra},input=answers,capture_output=True,text=True,timeout=90)
 (R/(name+'.log')).write_text(p.stdout+p.stderr)
 assert p.returncode==expected,(name,p.stdout,p.stderr)
 assert not list(tmp.glob('.blatman-*')),(name,'temporary directory leak')
 checks.append(name);print('PASS',name,flush=True);return p
url=lambda n:'https://example.com/probe/'+n+'.git'
repo('dep',{'Makefile':'all: dep\ndep: dep.c\n\tcc dep.c -o dep\n','dep.c':'int main(void){return 0;}\n'})
repo('main',{'Makefile':'all: main\nmain: main.c\n\t@test -f "$$BLATMAN_DESTDIR/usr/bin/dep" || { echo "fatal error: dependency.h: No such file or directory"; exit 1; }\n\tcc main.c -o main\n','main.c':'int main(void){return 0;}\n'})
invoke('recursive-dependency',['install',url('main')],answers='y\nn\n'+url('dep')+'\n')
assert (root/'usr/bin/dep').exists() and (root/'usr/bin/main').exists()
previous=(root/'usr/bin/main').read_bytes();invoke('reinstall-conflict',['install',url('main')],1);assert (root/'usr/bin/main').read_bytes()==previous
for name in ['cycle-a','cycle-b']:repo(name,{'Makefile':'all:\n\t@echo "fatal error: cycle.h: No such file or directory"\n\t@exit 1\n'})
p=invoke('dependency-cycle',['install',url('cycle-a')],1,'y\nn\n'+url('cycle-b')+'\ny\nn\n'+url('cycle-a')+'\n');assert 'Ciclo' in p.stderr
(R/'outside-cache').mkdir();(R/'blatman-cache/redirect.test').symlink_to(R/'outside-cache');invoke('cache-parent-link',['install','https://redirect.test/owner/repo.git'],1);assert not list((R/'outside-cache').iterdir())
repo('empty-cmake',{'CMakeLists.txt':'cmake_minimum_required(VERSION 3.20)\nproject(empty NONE)\n'});invoke('empty-install-recipe',['install',url('empty-cmake')],1)
repo('library',{'CMakeLists.txt':'cmake_minimum_required(VERSION 3.20)\nproject(hello C)\nadd_library(hello SHARED hello.c)\nset_target_properties(hello PROPERTIES VERSION 1.0 SOVERSION 1)\ninstall(TARGETS hello LIBRARY DESTINATION lib)\ninstall(FILES hello.h DESTINATION include)\n','hello.c':'int hello(void){return 42;}\n','hello.h':'int hello(void);\n'})
invoke('shared-library',['install',url('library')]);assert (root/'usr/lib/libhello.so').is_symlink();assert (root/'usr/include/hello.h').is_file()
repo('cmake-dep',{'CMakeLists.txt':'cmake_minimum_required(VERSION 3.20)\nproject(dep NONE)\ninstall(FILES marker DESTINATION share/probe)\n','marker':'yes'})
repo('override-main',{'Makefile':'all: override-main\noverride-main: main.c\n\t@test -f "$$BLATMAN_DESTDIR/usr/share/probe/marker" || { echo "fatal error: marker.h: No such file or directory"; exit 1; }\n\tcc main.c -o override-main\n','main.c':'int main(void){return 0;}\n'})
invoke('dependency-independent-detector',['install',url('override-main')],answers='y\nn\n'+url('cmake-dep')+'\n',extra={'BLATMAN_BUILD_SYSTEM':'make'})
for name,args,code in [('help',['help'],0),('unknown',['wat'],1),('missing-url',['install'],1),('missing-project',['build','/does/not/exist'],1),('remove',['remove','x'],1),('update',['update'],1),('excess-arguments',['build','.','extra'],1)]:invoke(name,args,code)
shim=R/'shim';shim.mkdir();write(shim/'git','#!/bin/sh\nif [ "$1" = clone ]; then echo "simulated clone failure" >&2; exit 128; fi\nexec /usr/bin/git "$@"\n',0o755)
invoke('failed-clone-cleanup',['install','https://clone-failure.test/owner/project.git'],1,extra={'PATH':str(shim)+':'+os.environ['PATH']})
parent=R/'blatman-cache/clone-failure.test/owner';assert not (parent/'project').exists();assert not list(parent.glob('.blatman-*'))
(area/'results.json').write_text(json.dumps({'run':str(R),'passed':checks},indent=2))
print(str(len(checks))+' checks passed')
