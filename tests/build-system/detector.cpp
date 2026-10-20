#include "builderDetector.hpp"
#include <iostream>
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    std::cout << buildSystemName(BuildDetector{}.detect(argv[1])) << '\n';
}
