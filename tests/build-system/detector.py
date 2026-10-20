#!/usr/bin/env python3
import os
from pathlib import Path
import subprocess
ROOT = Path(__file__).resolve().parents[2]
TEST = Path(__file__).resolve().parent
base_env = dict(os.environ)
base_env.pop('BLATMAN_BUILD_SYSTEM', None)
subprocess.run(['g++', '-std=c++20', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-I' + str(ROOT / 'include'), str(TEST / 'detector.cpp'), str(ROOT / 'src/builderDetector.cpp'), '-o', str(TEST / 'detector')], check=True)
cases = [
    ('gradle-wrapper-stale-pom', ['pom.xml', 'build.gradle', 'gradlew'], None, 'Gradle'),
    ('maven-wrapper-stale-gradle', ['pom.xml', 'build.gradle', 'mvnw'], None, 'Maven'),
    ('java-ambiguous', ['pom.xml', 'build.gradle'], None, 'Unknown'),
    ('mixed-ambiguous', ['CMakeLists.txt', 'Cargo.toml'], None, 'Unknown'),
    ('mixed-override', ['CMakeLists.txt', 'Cargo.toml'], 'cargo', 'Cargo'),
    ('bad-override', ['Cargo.toml'], 'cmake', 'Unknown'),
    ('native-generated-make', ['CMakeLists.txt', 'Makefile'], None, 'CMake'),
    ('make-override', ['CMakeLists.txt', 'Makefile'], 'make', 'Make'),
    ('pure-make', ['Makefile'], None, 'Make'),
]
with (TEST / 'detector.log').open('w') as log:
    for name, files, override, expected in cases:
        project = TEST / 'cases' / name
        project.mkdir(parents=True, exist_ok=True)
        for filename in files:
            (project / filename).touch()
        env = dict(base_env)
        if override:
            env['BLATMAN_BUILD_SYSTEM'] = override
        result = subprocess.run([str(TEST / 'detector'), str(project)], text=True, capture_output=True, env=env, check=True)
        log.write(f'{name}: {result.stdout.strip()}\n{result.stderr}')
        assert result.stdout.strip() == expected, (name, result.stdout, result.stderr)
        print('PASS', name)
