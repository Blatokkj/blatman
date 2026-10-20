#include "installTransaction.hpp"
int main(int argc, char** argv) {
    return argc == 3 && publishTree(argv[1], argv[2]) ? 0 : 1;
}
