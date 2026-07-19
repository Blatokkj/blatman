cmake_minimum_required(VERSION 3.10)
project(Blatman VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Diretórios
include_directories(include)

# Arquivos fonte
file(GLOB SOURCES "src/*.cpp")
# Adiciona Installer e DependencyResolver automaticamente

# Executável
add_executable(blatman main.cpp ${SOURCES})

# Permissões para instalar sem ./ (Linux)
set_target_properties(blatman PROPERTIES
    SKIP_BUILD_RPATH OFF
    BUILD_WITH_INSTALL_RPATH OFF
    INSTALL_RPATH "$ORIGIN"
    INSTALL_RPATH_USE_LINK_PATH ON
)

# Instalação
install(TARGETS blatman DESTINATION bin)