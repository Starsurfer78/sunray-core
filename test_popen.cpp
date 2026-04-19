#include <iostream>
#include <string>
#include <stdio.h>

int main() {
    FILE* pipe = popen("./scripts/ota_update.sh --check-only 2>/dev/null", "r");
    char buf[256];
    std::string output;
    while (fgets(buf, sizeof(buf), pipe)) output += buf;
    pclose(pipe);

    std::cout << "RAW OUTPUT: [" << output << "]" << std::endl;
    
    while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) {
        output.pop_back();
    }
    
    std::cout << "TRIMMED: [" << output << "]" << std::endl;
    return 0;
}
