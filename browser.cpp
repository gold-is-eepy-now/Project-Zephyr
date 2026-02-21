#include "browser_core.h"

#include <iostream>
#include <string>
#include <vector>

using std::string;

static std::string format_source_view(const SourceBundle& src) {
    std::string out;
    out += "\n================ HTML ================\n";
    out += src.html + "\n";
    out += "\n================ CSS ================\n";
    out += src.css.empty() ? "(none)\n" : src.css + "\n";
    out += "\n============= JavaScript =============\n";
    out += src.javascript.empty() ? "(none)\n" : src.javascript + "\n";
    out += "\n============= TypeScript =============\n";
    out += src.typescript.empty() ? "(none)\n" : src.typescript + "\n";
    return out;
}

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
            SourceBundle src = extract_source_bundle(response.body);

            if (history_index + 1 < static_cast<int>(history.size())) history.resize(history_index + 1);
            if (history.empty() || history.back() != current_url) {
                history.push_back(current_url);
                history_index = static_cast<int>(history.size()) - 1;
            }

            std::cout << "\n=== " << current_url << " ===\n";
            std::cout << response.status_line << "\n";
            for (const auto& [k, v] : response.headers) std::cout << k << ": " << v << "\n";
            std::cout << format_source_view(src) << "\n";

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
