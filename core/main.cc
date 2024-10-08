#include <iostream>
#include <v8.h>
#include "runtime.cc"

int main(int argc, char* argv[]) {
    const char* flags = "--max-old-space-size=4096";
    v8::V8::InitializeICU();
    v8::V8::SetFlagsFromString(flags);
    v8::V8::InitializeICUDefaultLocation(argv[0]);
    v8::V8::InitializeExternalStartupData(argv[0]);

    if (argc < 2) {
        std::cerr << "error: No script specified" << std::endl;
        return 1;
    }

    char *filename = argv[1];
    runtime::Runtime app;
    app.start(filename, argc, argv);
    
    return 0;
}
