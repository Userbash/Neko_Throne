#include <chrono>
#include <iostream>
int main() {
    std::chrono::milliseconds ms(100);
    std::cout << ms.count() << std::endl;
    return 0;
}
