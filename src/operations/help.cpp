#include "operations/help.hpp"
#include <iostream>
bool helpCommand(const std::string &)
{
    std::cout << R"(Blatman
Uso: blatman <comando> <argumento>
 install <URL HTTPS>  Compila e instala um repositorio.
 build <diretorio>    Compila um projeto local.
 help                Mostra esta ajuda.
 remove / update     Ainda nao implementados.
Sistemas: CMake, Make, Meson, Cargo, npm, Maven, Gradle, Autotools e Go.
Executaveis: /usr/bin. Bibliotecas, headers e dados preservam seus destinos.
BLATMAN_DESTDIR=/caminho instala nessa raiz alternativa, sem sudo.
BLATMAN_BUILD_SYSTEM=cmake|make|meson|cargo|npm|maven|gradle|autotools|go
seleciona um sistema presente no projeto.
BLATMAN_JAVA_HOME=/jdk seleciona um JDK compativel.
BLATMAN_GO_PACKAGES seleciona pacotes main em layouts ambiguos.
)";
    return true;
}
