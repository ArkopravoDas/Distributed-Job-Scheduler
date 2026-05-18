#!/usr/bin/env bash
set -euo pipefail

cmake -S . -B build
cmake --build build
./build/example_simple_job
./build/example_dependent_tasks
./build/example_failure_handling
