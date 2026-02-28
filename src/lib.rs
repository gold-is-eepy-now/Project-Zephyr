use std::collections::HashMap;

use anyhow::{anyhow, Result};
use scraper::{Html, Selector};
use url::Url;

#[derive(Debug, Clone)]
pub struct HttpResponse {
    pub status_line: String,
    pub headers: HashMap<String, String>,
    pub body: String,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct UrlParts {
    pub scheme: String,
    pub host: String,
    pub port: u16,
    pub path: String,
}

pub fn parse_url(raw: &str) -> Option<UrlParts> {
    let url = Url::parse(raw).ok()?;
    match url.scheme() {
        "http" | "https" => {}
        _ => return None,
    }
    Some(UrlParts {
        scheme: url.scheme().to_string(),
        host: url.host_str()?.to_string(),
        port: url.port_or_known_default()?,
        path: if url.path().is_empty() {
            "/".to_string()
        } else {
            url.path().to_string()
        },
    })
}

pub fn is_safe_navigation_target(href: &str) -> bool {
    let lower = href.trim().to_ascii_lowercase();
    !(lower.starts_with("javascript:")
        || lower.starts_with("data:")
        || lower.starts_with("file:")
        || lower.starts_with("vbscript:"))
}

pub fn resolve_url(base_url: &str, href: &str) -> Option<String> {
    let clean = href.trim();
    if clean.is_empty() || !is_safe_navigation_target(clean) {
        return None;
    }

    if clean.starts_with("http://") || clean.starts_with("https://") {
        return Some(clean.to_string());
    }

    let base = Url::parse(base_url).ok()?;
    if clean.starts_with('#') {
        return Some(base_url.to_string());
    }

    Some(base.join(clean).ok()?.to_string())
}

pub fn http_get(url: &str, timeout_seconds: u64, redirect_limit: usize) -> Result<HttpResponse> {
    let _ = parse_url(url).ok_or_else(|| anyhow!("Only http:// and https:// URLs are supported"))?;

    let client = reqwest::blocking::Client::builder()
        .timeout(std::time::Duration::from_secs(timeout_seconds))
        .redirect(reqwest::redirect::Policy::limited(redirect_limit))
        .user_agent("Zephyr/0.1 (Rust Rewrite)")
        .build()?;

    let resp = client.get(url).send()?;
    let status = resp.status();
    let mut headers = HashMap::new();
    for (k, v) in resp.headers() {
        headers.insert(k.as_str().to_string(), v.to_str().unwrap_or_default().to_string());
    }
    let body = resp.text()?;

    Ok(HttpResponse {
        status_line: format!("HTTP/{} {}", if status == reqwest::StatusCode::HTTP_VERSION_NOT_SUPPORTED {"1.1"} else {"2"}, status.as_u16()),
        headers,
        body,
    })
}

pub fn extract_style_blocks(html: &str) -> String {
    let doc = Html::parse_document(html);
    let style_sel = Selector::parse("style").expect("valid selector");
    doc.select(&style_sel)
        .map(|n| n.text().collect::<String>())
        .collect::<Vec<_>>()
        .join("\n")
}

#[derive(Debug, Clone, Default)]
pub struct SourceBundle {
    pub html: String,
    pub css: String,
    pub javascript: String,
    pub typescript: String,
}

pub fn extract_source_bundle(html: &str) -> SourceBundle {
    let doc = Html::parse_document(html);
    let script_sel = Selector::parse("script").expect("valid selector");
    let mut js = String::new();
    let mut ts = String::new();

    for script in doc.select(&script_sel) {
        let value = script.value();
        let kind = value.attr("type").unwrap_or_default().to_ascii_lowercase();
        let src = value.attr("src").unwrap_or_default();
        let body = script.text().collect::<String>();

        let mut block = String::new();
        if !src.is_empty() {
            block.push_str(&format!("// external script src={}\n", src));
        }
        if !body.trim().is_empty() {
            block.push_str(body.trim());
            block.push('\n');
        }

        if block.trim().is_empty() {
            continue;
        }

        if kind.contains("typescript") || kind.contains("text/ts") {
            ts.push_str(&block);
            ts.push('\n');
        } else {
            js.push_str(&block);
            js.push('\n');
        }
    }

    SourceBundle {
        html: html.to_string(),
        css: extract_style_blocks(html),
        javascript: js,
        typescript: ts,
    }
}

pub fn render_page_text(html: &str, wrap_width: usize) -> String {
    let doc = Html::parse_document(html);
    let body_sel = Selector::parse("body").expect("valid selector");
    let root = doc.select(&body_sel).next();

    let mut text = if let Some(body) = root {
        body.text().collect::<Vec<_>>().join(" ")
    } else {
        doc.root_element().text().collect::<Vec<_>>().join(" ")
    };

    text = text
        .split_whitespace()
        .collect::<Vec<_>>()
        .join(" ")
        .trim()
        .to_string();

    let a_sel = Selector::parse("a[href]").expect("valid selector");
    let links = doc
        .select(&a_sel)
        .filter_map(|a| {
            let href = a.value().attr("href")?;
            if is_safe_navigation_target(href) {
                Some(href.to_string())
            } else {
                None
            }
        })
        .take(12)
        .collect::<Vec<_>>();

    if !links.is_empty() {
        text.push_str("\n\nLinks:\n");
        for href in links {
            text.push_str("- ");
            text.push_str(&href);
            text.push('\n');
        }
    }

    wrap_text(&text, wrap_width)
}

fn wrap_text(input: &str, width: usize) -> String {
    let mut out = String::new();
    let mut line_len = 0usize;

    for word in input.split_whitespace() {
        if line_len == 0 {
            out.push_str(word);
            line_len = word.len();
            continue;
        }

        if line_len + 1 + word.len() > width {
            out.push('\n');
            out.push_str(word);
            line_len = word.len();
        } else {
            out.push(' ');
            out.push_str(word);
            line_len += 1 + word.len();
        }
    }

    out.trim().to_string()
}

pub fn extract_text_and_links(html: &str) -> (String, Vec<(String, String)>) {
    let doc = Html::parse_document(html);
    let text = render_page_text(html, 120);

    let a_sel = Selector::parse("a[href]").expect("valid selector");
    let mut links = Vec::new();
    for a in doc.select(&a_sel) {
        let href = a.value().attr("href").unwrap_or_default().to_string();
        if !is_safe_navigation_target(&href) {
            continue;
        }
        let label = a.text().collect::<Vec<_>>().join(" ").trim().to_string();
        if !label.is_empty() {
            links.push((label, href));
        }
    }

    (text, links)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn url_helpers_work() {
        let parts = parse_url("https://example.com/a/b").unwrap();
        assert_eq!(parts.scheme, "https");
        assert_eq!(parts.port, 443);
        assert_eq!(resolve_url("https://example.com/a/b", "../c").unwrap(), "https://example.com/c");
        assert!(resolve_url("https://example.com", "javascript:alert(1)").is_none());
    }

    #[test]
    fn extraction_and_rendering_work() {
        let sample = "<html><head><style>body{color:red}</style></head><body><h1>Hello</h1><a href='https://x'>X</a><script>console.log(1)</script></body></html>";
        let src = extract_source_bundle(sample);
        assert!(src.css.contains("color:red"));
        assert!(src.javascript.contains("console.log"));

        let rendered = render_page_text(sample, 80);
        assert!(rendered.contains("Hello"));
        assert!(rendered.contains("Links:"));
    }
}
