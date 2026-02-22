#include "browser_core.h"

#include <iostream>
#include <string>
#include <vector>

using std::string;

int main(int argc, char** argv) {
    std::vector<string> history;
    int history_index = -1;

    string current_url;
    if (argc > 1) current_url = argv[1];
    else {
        std::cout << "Enter URL (http:// or https://): ";
        std::getline(std::cin, current_url);
    }
    if (current_url.find("://") == string::npos) current_url = "https://" + current_url;

    bool running = true;
    while (running) {
        try {
            HttpResponse response = http_get(current_url);
            const std::string rendered = render_page_text(response.body, 100);

            if (history_index + 1 < static_cast<int>(history.size())) history.resize(history_index + 1);
            if (history.empty() || history.back() != current_url) {
                history.push_back(current_url);
                history_index = static_cast<int>(history.size()) - 1;
            }

            std::cout << "\n=== " << current_url << " ===\n\n";
            if (rendered.empty()) std::cout << "(No renderable body content)\n\n";
            else std::cout << rendered << "\n\n";

            std::cout << "Command (url <url>, back, forward, reload, quit): ";
            string cmd;
            if (!std::getline(std::cin, cmd)) break;
            if (cmd == "quit") break;
            if (cmd == "reload") continue;

            if (cmd == "back") {
                if (history_index > 0) current_url = history[--history_index];
                else std::cout << "No back history.\n";
                continue;
            }

            if (cmd == "forward") {
                if (history_index + 1 < static_cast<int>(history.size())) current_url = history[++history_index];
                else std::cout << "No forward history.\n";
                continue;
            }

            if (cmd.rfind("url ", 0) == 0) {
                current_url = cmd.substr(4);
                if (current_url.find("://") == string::npos) current_url = "https://" + current_url;
                continue;
            }

            std::cout << "Unknown command.\n";
        } catch (const std::exception& ex) {
            std::cerr << "Error: " << ex.what() << "\n";
            std::cout << "Type a new URL (or 'quit'): ";
            string fallback;
            if (!std::getline(std::cin, fallback) || fallback == "quit") running = false;
            else current_url = (fallback.find("://") == string::npos) ? "https://" + fallback : fallback;
        }
    }

    return 0;
}
