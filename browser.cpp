#include "browser_core.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

using std::string;

int main(int argc, char** argv) {
    std::vector<string> history;
    int history_index = -1;

    string current_url;
    if (argc > 1) {
        current_url = argv[1];
    } else {
        std::cout << "Enter URL (http://...): ";
        std::getline(std::cin, current_url);
    }
    if (current_url.find("://") == string::npos) current_url = "http://" + current_url;

    bool running = true;
    while (running) {
        try {
            HttpResponse response = http_get(current_url);
            string plain;
            std::vector<std::pair<string, string>> links;
            extract_text_and_links(response.body, plain, links);

            if (history_index + 1 < static_cast<int>(history.size())) {
                history.resize(history_index + 1);
            }
            if (history.empty() || history.back() != current_url) {
                history.push_back(current_url);
                history_index = static_cast<int>(history.size()) - 1;
            }

            std::cout << "\n=== " << current_url << " ===\n";
            std::cout << response.status_line << "\n";
            for (const auto& [k, v] : response.headers) {
                std::cout << k << ": " << v << "\n";
            }
            std::cout << "\n" << plain << "\n";

            if (!links.empty()) {
                std::cout << "\nLinks:\n";
                for (size_t i = 0; i < links.size(); ++i) {
                    std::cout << "[" << (i + 1) << "] " << links[i].first << " -> " << links[i].second << "\n";
                }
            }

            std::cout << "\nCommand ([number] follow, url <url>, back, forward, reload, quit): ";
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
                if (current_url.find("://") == string::npos) current_url = "http://" + current_url;
                continue;
            }

            const bool digits_only = !cmd.empty() &&
                std::all_of(cmd.begin(), cmd.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)); });
            if (digits_only) {
                const int index = std::stoi(cmd);
                if (index >= 1 && index <= static_cast<int>(links.size())) {
                    const string next = resolve_url(current_url, links[index - 1].second);
                    if (!next.empty()) current_url = next;
                    else std::cout << "Blocked unsafe or malformed link target.\n";
                } else {
                    std::cout << "Invalid link number.\n";
                }
                continue;
            }

            std::cout << "Unknown command.\n";
        } catch (const std::exception& ex) {
            std::cerr << "Error: " << ex.what() << "\n";
            std::cout << "Type a new URL (or 'quit'): ";
            string fallback;
            if (!std::getline(std::cin, fallback) || fallback == "quit") {
                running = false;
            } else {
                current_url = (fallback.find("://") == string::npos) ? "http://" + fallback : fallback;
            }
        }
    }

    return 0;
}
