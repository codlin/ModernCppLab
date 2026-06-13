#include "ConsoleApp.h"

#include <filesystem>
#include <iostream>

int main() {
    try {
        ConsoleApp app(std::filesystem::path("data/tasks.json"));
        app.Run();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << "\n";
        return 1;
    }
}
