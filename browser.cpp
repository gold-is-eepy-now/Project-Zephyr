#include "browser_core.h"

#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    std::vector<std::string> history;
    int history_index = -1;

    std::string current;
    if (argc > 1) current = argv[1];
    else {
        std::cout << "Enter URL: ";
        std::getline(std::cin, current);
    }
    if (current.find("://") == std::string::npos) current = "https://" + current;

    while (true) {
        try {
            const HttpResponse r = http_get(current);
            const std::string page = render_page_text(r.body, 100);

            if (history_index + 1 < static_cast<int>(history.size())) history.resize(history_index + 1);
            if (history.empty() || history.back() != current) {
                history.push_back(current);
                history_index = static_cast<int>(history.size()) - 1;
            }

            std::cout << "\n=== " << current << " ===\n\n";
            std::cout << (page.empty() ? "(No renderable content)" : page) << "\n\n";
            std::cout << "Command (url <url>, back, forward, reload, quit): ";

            std::string cmd;
            if (!std::getline(std::cin, cmd)) break;
            if (cmd == "quit") break;
            if (cmd == "reload") continue;
            if (cmd == "back") {
                if (history_index > 0) current = history[--history_index];
                else std::cout << "No back history.\n";
                continue;
            }
            if (cmd == "forward") {
                if (history_index + 1 < static_cast<int>(history.size())) current = history[++history_index];
                else std::cout << "No forward history.\n";
                continue;
            }
            if (cmd.rfind("url ", 0) == 0) {
                current = cmd.substr(4);
                if (current.find("://") == std::string::npos) current = "https://" + current;
                continue;
            }
            std::cout << "Unknown command.\n";
        } catch (const std::exception& ex) {
            std::cerr << "Error: " << ex.what() << "\n";
            std::cout << "New URL (or quit): ";
            std::string next;
            if (!std::getline(std::cin, next) || next == "quit") break;
            current = (next.find("://") == std::string::npos) ? ("https://" + next) : next;
        }
    }

    return 0;
}
