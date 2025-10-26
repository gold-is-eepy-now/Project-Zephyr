// console wrapper that uses browser_core
#include "browser_core.h"

#include <iostream>
#include <vector>
#include <functional>

using std::string;

int main(int argc, char **argv) {
    std::vector<string> history;
    int hist_index = -1;

    std::function<int(const string&)> open_url;
    open_url = [&](const string &url) -> int {
        try {
            HttpResponse r = http_get(url);
            string plain;
            std::vector<std::pair<string,string>> links;
            extract_text_and_links(r.body, plain, links);

            std::cout << "\n=== " << url << " ===\n";
            std::cout << r.status_line << "\n";
            for (auto &h : r.headers) std::cout << h.first << ": " << h.second << "\n";
            std::cout << "\n";
            std::cout << plain << "\n\n";

            if (!links.empty()) {
                std::cout << "Links:\n";
                for (size_t i = 0; i < links.size(); ++i) {
                    std::cout << "[" << (i+1) << "] " << links[i].first << " -> " << links[i].second << "\n";
                }
            } else {
                std::cout << "(no links found)\n";
            }

            if (hist_index + 1 < (int)history.size()) history.resize(hist_index+1);
            history.push_back(url);
            hist_index = (int)history.size() - 1;

            for (;;) {
                std::cout << "\nCommand ([number] follow, url <url>, back, forward, quit): ";
                string cmd;
                if (!std::getline(std::cin, cmd)) return 0;
                if (cmd.empty()) continue;
                if (cmd == "quit") return 0;
                if (cmd == "back") {
                    if (hist_index > 0) {
                        hist_index -= 1;
                        open_url(history[hist_index]);
                        return 0;
                    } else { std::cout << "No back history.\n"; continue; }
                }
                if (cmd == "forward") {
                    if (hist_index + 1 < (int)history.size()) {
                        hist_index += 1;
                        open_url(history[hist_index]);
                        return 0;
                    } else { std::cout << "No forward history.\n"; continue; }
                }
                if (cmd.rfind("url ", 0) == 0) {
                    string newurl = cmd.substr(4);
                    if (newurl.find("://") == string::npos) newurl = "http://" + newurl;
                    open_url(newurl);
                    return 0;
                }
                bool all_digits = std::all_of(cmd.begin(), cmd.end(), [](char c){ return std::isdigit((unsigned char)c); });
                if (all_digits) {
                    int n = stoi(cmd);
                    if (n >= 1 && n <= (int)links.size()) {
                        string href = links[n-1].second;
                        string next;
                        if (href.rfind("http://", 0) == 0) next = href;
                        else {
                            UrlParts parts;
                            parse_url(url, parts);
                            if (!href.empty() && href[0] == '/') {
                                next = parts.scheme + "://" + parts.host + href;
                            } else {
                                string base = parts.path;
                                auto p = base.rfind('/');
                                if (p == string::npos) base = "/"; else base = base.substr(0, p+1);
                                next = parts.scheme + "://" + parts.host + base + href;
                            }
                        }
                        open_url(next);
                        return 0;
                    } else { std::cout << "Invalid link number.\n"; }
                }
                std::cout << "Unknown command.\n";
            }

        } catch (const std::exception &ex) {
            std::cerr << "Error: " << ex.what() << "\n";
        }
        return 0;
    };

    string start;
    if (argc > 1) start = argv[1];
    else {
        std::cout << "Enter URL (http://...): ";
        std::getline(std::cin, start);
    }
    if (start.find("://") == string::npos) start = "http://" + start;
    open_url(start);
    return 0;
}
