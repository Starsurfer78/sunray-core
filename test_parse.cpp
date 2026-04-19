#include <iostream>
#include <string>

int main() {
    std::string output = "update_available:a5a6cf6e";
    if (output == "already_up_to_date") {
        std::cout << "status: up_to_date" << std::endl;
    } else if (output.rfind("update_available:", 0) == 0) {
        std::cout << "status: update_available" << std::endl;
        std::cout << "hash: " << output.substr(std::string("update_available:").size()) << std::endl;
    } else if (output.rfind("error:", 0) == 0) {
        std::cout << "status: error" << std::endl;
    } else {
        std::cout << "status: unknown" << std::endl;
    }
    return 0;
}
