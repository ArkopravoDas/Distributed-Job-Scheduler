#include <chrono>
#include <iostream>
#include <string>

#include "scheduler/distributed/CoordinatorServer.hpp"

int main() {
    scheduler::distributed::CoordinatorServer coordinator;

    std::cout << "Coordinator server started.\n";
    std::cout << "Enter text messages like: REGISTER worker_id=worker-1\n";
    std::cout << "Type 'tick' to mark dead workers after a 5s timeout, or 'quit' to exit.\n";

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "quit") {
            break;
        }

        if (line == "tick") {
            const auto deadWorkers = coordinator.markDeadWorkers(std::chrono::seconds{5});
            if (deadWorkers.empty()) {
                std::cout << "No dead workers detected.\n";
            } else {
                for (const auto& workerId : deadWorkers) {
                    std::cout << "Marked dead: " << workerId << '\n';
                }
            }
            continue;
        }

        std::cout << coordinator.handleRawMessage(line) << '\n';
    }

    return 0;
}
