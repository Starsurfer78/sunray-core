#include <iostream>
#include <string>

int main() {
    std::string output = "[ota] Fetching remote refs...\nupdate_available:c5505dd3";
    
    // simulated parsing
    std::string line;
    size_t pos = output.find_last_of('\n');
    if (pos != std::string::npos) {
        line = output.substr(pos + 1);
    } else {
        line = output;
    }

    if (line == "already_up_to_date") {
        std::cout << "status: up_to_date" << std::endl;
    } else if (line.rfind("update_available:", 0) == 0) {
        std::cout << "status: update_available" << std::endl;
        std::cout << "hash: " << line.substr(std::string("update_available:").size()) << std::endl;
    } else if (line.rfind("error:", 0) == 0) {
        std::cout << "status: error" << std::endl;
    } else {
        std::cout << "status: unknown" << std::endl;
    }
    return 0;
}
