#include "app/CeliaApp.h"

#include <exception>
#include <iostream>

int main(int argc, char* argv[]) {
    try {
        celia::CeliaApp app(argc > 0 ? argv[0] : "");
        return app.run();
    } catch (const std::exception& error) {
        std::cerr << "Voice Embedded Verification failed: " << error.what() << '\n';
        return 1;
    }
}
