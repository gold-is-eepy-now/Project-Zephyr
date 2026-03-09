use std::io::{self, Write};

use anyhow::Result;

fn normalize_url(input: &str) -> String {
    if input.contains("://") {
        input.to_string()
    } else {
        format!("https://{}", input)
    }
}

fn main() -> Result<()> {
    let mut history: Vec<String> = Vec::new();
    let mut history_index: isize = -1;

    let mut current = std::env::args().nth(1).unwrap_or_else(|| "https://duckduckgo.com".to_string());
    current = normalize_url(&current);

    loop {
        match zephyr::http_get(&current, 15, 5) {
            Ok(response) => {
                let rendered = zephyr::render_page_text(&response.body, 100);
                if history_index >= 0 && (history_index as usize) + 1 < history.len() {
                    history.truncate((history_index as usize) + 1);
                }
                if history.last().map(|x| x != &current).unwrap_or(true) {
                    history.push(current.clone());
                    history_index = history.len() as isize - 1;
                }

                println!("\n=== {} ===\n", current);
                if rendered.is_empty() {
                    println!("(No renderable content)\n");
                } else {
                    println!("{}\n", rendered);
                }
            }
            Err(err) => {
                eprintln!("Error loading {}: {}", current, err);
            }
        }

        print!("Command (url <url>, back, forward, reload, quit): ");
        io::stdout().flush()?;
        let mut cmd = String::new();
        if io::stdin().read_line(&mut cmd).is_err() {
            break;
        }
        let cmd = cmd.trim();

        if cmd.eq_ignore_ascii_case("quit") {
            break;
        }
        if cmd.eq_ignore_ascii_case("reload") {
            continue;
        }
        if cmd.eq_ignore_ascii_case("back") {
            if history_index > 0 {
                history_index -= 1;
                current = history[history_index as usize].clone();
            } else {
                println!("No back history.");
            }
            continue;
        }
        if cmd.eq_ignore_ascii_case("forward") {
            if (history_index as usize) + 1 < history.len() {
                history_index += 1;
                current = history[history_index as usize].clone();
            } else {
                println!("No forward history.");
            }
            continue;
        }
        if let Some(url) = cmd.strip_prefix("url ") {
            current = normalize_url(url.trim());
            continue;
        }
    }

    Ok(())
}
