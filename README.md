# Blatman

**Universal GitHub Project Builder** - Clone, compile e instale qualquer projeto do GitHub com um único comando.

## 🚀 Funcionalidades

- **Universal**: Suporta 9 sistemas de build (Cargo, CMake, Make, Meson, Npm, Gradle, Autotools, Go, Maven)
- **Detecção automática**: Identifica o sistema de build pelo arquivo de configuração do projeto
- **Baixa memória**: Modo conservador automático para PCs fracos (RAM < 4GB ou swap < 2GB)
- **Seguro**: Proteção contra injeção de comando, path traversal e validação estrita de URLs
- **Gerenciamento de pacotes**: `install`, `list`, `upgrade`, `packs`, `help`
- **Atualizações**: Atualiza pacotes instalados com `git pull` + rebuild
- **Multi-distro**: Resolve dependências via pacman, apt, dnf, zypper, brew
- **Manifests**: Rastreia pacotes instalados com metadados JSON

## 📦 Instalação

### Pré-requisitos
- Git
- Compilador C++ (g++ ou clang++)
- CMake 3.10+
- Sistema de build alvo (cargo, cmake, make, go, node, gradle, maven, etc.)

### Baixar e compilar

```bash
# Clone o repositório
git clone https://github.com/Blatokkj/blatman.git
cd blatman

# Compile
mkdir build && cd build
cmake ..
make -j$(nproc)

# Instale (opcional - adiciona ao PATH)
sudo cmake --install .  # ou copie para ~/.local/bin/
```

## 🎯 Uso

### Comandos principais

```bash
# Instala um projeto do GitHub
blatman install https://github.com/usuario/repositorio

# Lista pacotes instalados
blatman list

# Atualiza todos os pacotes
blatman upgrade

# Atualiza um pacote específico
blatman upgrade nome-do-pacote

# Conta pacotes instalados
blatman packs

# Ajuda
blatman help
```

### Exemplos

```bash
# Projetos Rust (Cargo)
blatman install https://github.com/jesseduffield/lazygit
blatman install https://github.com/rust-lang/mdBook

# Projetos C/C++ (CMake)
blatman install https://github.com/neovim/neovim
blatman install https://github.com/redis/hiredis

# Projetos Go
blatman install https://github.com/jesseduffield/lazygit
blatman install https://github.com/ClementTsang/bottom

# Projetos Make/Autotools
blatman install https://github.com/aristocratos/btop
blatman install https://github.com/cava/cava

# Projetos Node.js
blatman install https://github.com/vercel/next.js

# Projetos Java/Kotlin (Gradle/Maven)
blatman install https://github.com/spring-projects/spring-boot
```

## 🏗️ Sistemas de Build Suportados

| Sistema | Arquivo de Detecção | Comando de Build |
|---------|---------------------|------------------|
| **Cargo (Rust)** | `Cargo.toml` | `cargo build --release` |
| **CMake** | `CMakeLists.txt` | `cmake -B build && cmake --build build` |
| **Make** | `Makefile` | `make` |
| **Meson** | `meson.build` | `meson setup build && ninja -C build` |
| **Npm** | `package.json` | `npm ci && npm run build` |
| **Gradle** | `build.gradle*` | `./gradlew build` |
| **Maven** | `pom.xml` | `mvn package` + wrapper script |
| **Autotools** | `configure*` | `./configure && make` |
| **Go** | `go.mod` | `go build -trimpath -ldflags="-s -w"` |

## ⚒️ Testes prévios

| Projeto | Requisito | Conclusão |
|---------|---------------------|------------------|
| **Catch2** | `CMake` | `Teste bem sucedido.` |
| **GoAnime** | `Go` | `Teste bem sucedido.` |
| **Cava** | `Cmake` | `Teste bem sucedido.` |
| **Hiredis** | `Cmake` | `Teste bem sucedido.` |
| **Bottom/btm** | `Cargo` | `Teste bem sucedido.` |
| **Lazygit** | `Go` | `Teste bem sucedido.` |
| **Tty-clock** | `Make` | `Teste bem sucedido.` |

## ⚙️ Modo Baixa Memória

Ativado automaticamente quando:
- RAM disponível < 4GB **OU**
- Swap < 2GB

**O que faz:**
- Limita jobs paralelos a 1 (`-j1`)
- Desabilita LTO no Rust (`CARGO_PROFILE_RELEASE_LTO=false`)
- Limita heap do Node.js (`--max-old-space-size=1024`)
- Configura Maven/Gradle para usar menos memória
- Usa `-p 1` no Go (single-threaded compilation)

## 🔒 Segurança

- **Validação de URL**: Aceita apenas `https://github.com/usuario/repositorio`
- **Escape de shell**: Todos os argumentos são escapados antes de `system()`
- **Path traversal**: Nomes de repositório sanitizados (apenas `[\w.-]`)
- **Sem sudo no build**: Apenas dependências de sistema usam sudo (via package manager)

## 📁 Estrutura de Diretórios

```
~/.blatman/
├── bin/           # Binários instalados (adicionado ao PATH)
├── cache/         # Repositórios clonados e builds
├── logs/          # Logs diários (blatman_YYYYMMDD.log)
└── manifests/     # Metadados JSON dos pacotes instalados
```

## 🔄 Atualizações

```bash
# Atualiza todos
blatman upgrade

# Atualiza apenas um
blatman upgrade lazygit

# O que faz:
# 1. git pull no cache do projeto
# 2. Re-detecta sistema de build
# 3. Recompila
# 4. Substitui binários em ~/.blatman/bin/
```

## 🐛 Troubleshooting

### "Sistema de build desconhecido"
O projeto não tem arquivo de configuração reconhecido. Verifique se tem `Cargo.toml`, `CMakeLists.txt`, `Makefile`, `go.mod`, `package.json`, `pom.xml`, `build.gradle`, `meson.build` ou `configure`.

### "Cache do pacote não encontrado"
```bash
# Reinstale
blatman install https://github.com/usuario/repo
```

### Falha de memória (OOM)
O modo baixa memória ativa automaticamente. Se ainda falhar:
```bash
# Aumente swap
sudo fallocate -l 4G /swapfile
sudo chmod 600 /swapfile
sudo mkswap /swapfile
sudo swapon /swapfile
```

### Dependências faltando
O Blatman tenta instalar automaticamente via package manager (pacman/apt/dnf/zypper/brew). Se falhar, instale manualmente.

## 🤝 Contribuindo

1. Fork o projeto
2. Crie branch: `git checkout -b feature/nova-funcionalidade`
3. Commit: `git commit -m 'Adiciona nova funcionalidade'`
4. Push: `git push origin feature/nova-funcionalidade`
5. Abra Pull Request

## 📄 Licença

MIT License - veja [LICENSE](LICENSE) para detalhes.

---

**Blatman** - Compile qualquer projeto GitHub. 🛠️
