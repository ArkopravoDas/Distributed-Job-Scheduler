$ErrorActionPreference = "Stop"

cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
.\build\scheduler_app.exe
