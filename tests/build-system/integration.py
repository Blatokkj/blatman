#!/usr/bin/env python3
"""Offline integration coverage for declared Cargo and Go executable selection."""
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
TEST = Path(__file__).resolve().parent
CASES = TEST / 'cases'
CASES.mkdir(exist_ok=True)
LOG = TEST / 'integration.log'
ENV = dict(os.environ, CARGO_NET_OFFLINE='true', GOPROXY='off', GOSUMDB='off', GOCACHE=str(CASES / 'go-cache'), GOFLAGS='-buildvcs=false')
ENV.pop('BLATMAN_GO_PACKAGES', None)
ENV.pop('BLATMAN_BUILD_SYSTEM', None)

def write(path, content):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content)


def run(command, env=ENV):
    result = subprocess.run([str(item) for item in command], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=env)
    with LOG.open('a') as handle:
        handle.write('$ ' + ' '.join(map(str, command)) + '\n' + result.stdout + '\n')
    return result


def build(name, system, expected, selection=None, diagnostic=None):
    project = CASES / name
    output = project / 'isolated output'
    output.mkdir(exist_ok=True)
    write(output / 'unrelated-existing-file', 'retain\n')
    env = dict(ENV)
    if selection is not None:
        env['BLATMAN_GO_PACKAGES'] = selection
    result = run([TEST / 'driver', system, project, output], env)
    data = json.loads(next(line[7:] for line in reversed(result.stdout.splitlines()) if line.startswith('RESULT ')))
    if expected is None:
        assert not data['success'] and result.returncode != 0, data
        if diagnostic:
            assert diagnostic in data['output'], data
    else:
        assert data['success'] and result.returncode == 0, data
        binaries = [Path(path) for path in data['executables']]
        assert {path.name for path in binaries} == set(expected), data
        assert all(path.is_absolute() and path.parent == output and path.is_file() for path in binaries), data
        for binary in binaries:
            assert run([binary]).returncode == 0, binary
    assert (output / 'unrelated-existing-file').read_text() == 'retain\n'
    print('PASS', name, selection or '')


def rust_package(base, name, binary=None):
    manifest = f'[package]\nname = "{name}"\nversion = "0.1.0"\nedition = "2021"\n'
    if binary:
        manifest += f'[[bin]]\nname = "{binary}"\npath = "src/main.rs"\n'
    write(base / 'Cargo.toml', manifest)
    write(base / 'src/main.rs', 'fn main() { println!("fixture"); }\n')


LOG.write_text('')
compiled = run(['g++', '-std=c++20', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-I' + str(ROOT / 'include'), TEST / 'driver.cpp', ROOT / 'src/operations/build.cpp', ROOT / 'src/shell.cpp', '-o', TEST / 'driver'])
assert compiled.returncode == 0, compiled.stdout

for case in ['cargo-multi', 'cargo-default-members', 'cargo-duplicate']:
    base = CASES / case
    default = 'default-members = ["one"]\n' if case == 'cargo-default-members' else ''
    write(base / 'Cargo.toml', '[workspace]\nresolver = "2"\nmembers = ["one", "two"]\n' + default)
    rust_package(base / 'one', 'first', 'same' if case == 'cargo-duplicate' else None)
    rust_package(base / 'two', 'second', 'same' if case == 'cargo-duplicate' else None)
build('cargo-multi', 'cargo', ['first', 'second'])
build('cargo-default-members', 'cargo', ['first'])
build('cargo-duplicate', 'cargo', None, diagnostic='duplicados')
# Re-run with Cargo.lock present to cover --locked and fresh artifacts.
build('cargo-multi', 'cargo', ['first', 'second'])
base = CASES / 'cargo-library'
write(base / 'Cargo.toml', '[package]\nname = "onlylib"\nversion = "0.1.0"\nedition = "2021"\n')
write(base / 'src/lib.rs', 'pub fn answer() -> u8 { 42 }\n')
build('cargo-library', 'cargo', [])

main = 'package main\nimport "fmt"\nfunc main() { fmt.Println("fixture") }\n'
for case in ['go-root-cmd', 'go-library', 'go-collision', 'go-errors']:
    base = CASES / case
    write(base / 'go.mod', 'module example.com/fixture\n\ngo 1.22\n')
    if case == 'go-library':
        write(base / 'library.go', 'package fixture\nfunc Answer() int { return 42 }\n')
    elif case == 'go-collision':
        write(base / 'cmd/one/tool/main.go', main)
        write(base / 'cmd/two/tool/main.go', main)
    elif case == 'go-errors':
        write(base / 'main.go', 'package main\nimport _ "example.invalid/missing"\nfunc main() {}\n')
    else:
        write(base / 'main.go', main)
        write(base / 'cmd/cli/main.go', main)
        write(base / 'examples/broken/main.go', 'not valid Go\n')
        write(base / 'cmd/examples/broken/main.go', 'not valid Go\n')
build('go-root-cmd', 'go', ['fixture', 'cli'])
build('go-library', 'go', [])
build('go-collision', 'go', None, diagnostic='mesmo nome')
build('go-collision', 'go', ['tool'], selection='./cmd/one/tool')
build('go-errors', 'go', None, diagnostic='Go invalid')
print('All offline integration cases passed. Logs:', LOG)
