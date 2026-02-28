use std::collections::HashMap;

use anyhow::{anyhow, Result};
use boa_engine::{Context as JsContext, Source};
use cssparser::{Parser, ParserInput, Token};
use html5ever::{parse_document, tendril::TendrilSink};
use markup5ever_rcdom::{Handle, NodeData, RcDom};
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

#[derive(Debug, Clone, Default)]
pub struct SourceBundle {
    pub html: String,
    pub css: String,
    pub javascript: String,
    pub typescript: String,
}

#[derive(Debug, Clone)]
pub struct EngineNode {
    pub id: usize,
    pub parent: Option<usize>,
    pub tag: Option<String>,
    pub text: String,
    pub attrs: HashMap<String, String>,
    pub children: Vec<usize>,
}

#[derive(Debug, Clone, Default)]
pub struct EngineDocument {
    pub nodes: Vec<EngineNode>,
    pub root_id: usize,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct CssSelector {
    pub parts: Vec<SimpleSelector>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SimpleSelector {
    pub tag: Option<String>,
    pub id: Option<String>,
    pub classes: Vec<String>,
}

#[derive(Debug, Clone, Default)]
pub struct CssDeclaration {
    pub property: String,
    pub value: String,
}

#[derive(Debug, Clone, Default)]
pub struct CssRule {
    pub selectors: Vec<CssSelector>,
    pub declarations: Vec<CssDeclaration>,
    pub source_order: usize,
}

#[derive(Debug, Clone, Default)]
pub struct EngineStyleSheet {
    pub rules: Vec<CssRule>,
}

#[derive(Debug, Clone, Default)]
pub struct LayoutBox {
    pub node_id: usize,
    pub x: f32,
    pub y: f32,
    pub width: f32,
    pub height: f32,
    pub children: Vec<LayoutBox>,
}

#[derive(Debug, Clone, Default)]
pub struct LayoutTree {
    pub root: Option<LayoutBox>,
    pub content_height: f32,
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
        .user_agent("Zephyr/0.2 (Engine Hardening)")
        .build()?;

    let resp = client.get(url).send()?;
    let status = resp.status();

    let mut headers = HashMap::new();
    for (k, v) in resp.headers() {
        headers.insert(k.as_str().to_string(), v.to_str().unwrap_or_default().to_string());
    }

    Ok(HttpResponse {
        status_line: format!("HTTP {}", status.as_u16()),
        headers,
        body: resp.text()?,
    })
}

pub fn parse_html_spec(html: &str) -> EngineDocument {
    let dom: RcDom = parse_document(RcDom::default(), Default::default()).one(html);
    let mut out = EngineDocument::default();

    fn walk(handle: &Handle, parent: Option<usize>, out: &mut EngineDocument) -> usize {
        let mut node = EngineNode {
            id: out.nodes.len(),
            parent,
            tag: None,
            text: String::new(),
            attrs: HashMap::new(),
            children: Vec::new(),
        };

        match &handle.data {
            NodeData::Document => {
                node.tag = Some("#document".to_string());
            }
            NodeData::Element { name, attrs, .. } => {
                node.tag = Some(name.local.to_string());
                let attrs_ref = attrs.borrow();
                for a in &*attrs_ref {
                    node.attrs.insert(a.name.local.to_string(), a.value.to_string());
                }
            }
            NodeData::Text { contents } => {
                node.text = contents.borrow().to_string();
            }
            NodeData::Comment { contents } => {
                node.tag = Some("#comment".to_string());
                node.text = contents.to_string();
            }
            _ => {}
        }

        let id = node.id;
        out.nodes.push(node);

        let children = handle.children.borrow();
        for child in &*children {
            let child_id = walk(child, Some(id), out);
            out.nodes[id].children.push(child_id);
        }

        id
    }

    out.root_id = walk(&dom.document, None, &mut out);
    out
}

pub fn extract_style_blocks(html: &str) -> String {
    let doc = Html::parse_document(html);
    let style_sel = Selector::parse("style").expect("valid selector");
    doc.select(&style_sel)
        .map(|n| n.text().collect::<String>())
        .collect::<Vec<_>>()
        .join("\n")
}

fn parse_selector_text(text: &str) -> CssSelector {
    let mut parts = Vec::new();
    for chunk in text.split_whitespace() {
        let mut selector = SimpleSelector {
            tag: None,
            id: None,
            classes: Vec::new(),
        };

        let mut current = String::new();
        let mut mode = 't';
        for c in chunk.chars().chain(std::iter::once('\0')) {
            if c == '#' || c == '.' || c == '\0' {
                if !current.is_empty() {
                    match mode {
                        't' => selector.tag = Some(current.to_ascii_lowercase()),
                        '#' => selector.id = Some(current.to_string()),
                        '.' => selector.classes.push(current.to_string()),
                        _ => {}
                    }
                    current.clear();
                }
                mode = c;
            } else {
                current.push(c);
            }
        }

        parts.push(selector);
    }

    CssSelector { parts }
}

pub fn parse_css_stylesheet(css: &str) -> EngineStyleSheet {
    let mut sheet = EngineStyleSheet::default();
    let mut source_order = 0usize;

    for block in css.split('}') {
        let Some((selectors_src, body_src)) = block.split_once('{') else { continue };

        let selectors = selectors_src
            .split(',')
            .map(|s| parse_selector_text(s.trim()))
            .filter(|s| !s.parts.is_empty())
            .collect::<Vec<_>>();
        if selectors.is_empty() {
            continue;
        }

        let mut declarations = Vec::new();
        for decl in body_src.split(';') {
            let Some((name, value)) = decl.split_once(':') else { continue };
            let name = name.trim();
            let value = value.trim();
            if name.is_empty() || value.is_empty() {
                continue;
            }

            let mut input = ParserInput::new(value);
            let mut parser = Parser::new(&mut input);
            let mut normalized = String::new();
            while let Ok(token) = parser.next_including_whitespace_and_comments() {
                match token {
                    Token::Ident(v) => normalized.push_str(v),
                    Token::Hash(v) => {
                        normalized.push('#');
                        normalized.push_str(v);
                    }
                    Token::Dimension { value, unit, .. } => normalized.push_str(&format!("{}{}", value, unit)),
                    Token::Number { value, .. } => normalized.push_str(&value.to_string()),
                    Token::Percentage { unit_value, .. } => normalized.push_str(&format!("{}%", unit_value * 100.0)),
                    Token::WhiteSpace(_) => normalized.push(' '),
                    _ => {}
                }
            }
            if normalized.trim().is_empty() {
                normalized = value.to_string();
            }

            declarations.push(CssDeclaration {
                property: name.to_ascii_lowercase(),
                value: normalized.trim().to_string(),
            });
        }

        sheet.rules.push(CssRule {
            selectors,
            declarations,
            source_order,
        });
        source_order += 1;
    }

    sheet
}

fn node_matches_simple(node: &EngineNode, simple: &SimpleSelector) -> bool {
    if let Some(tag) = &simple.tag {
        if node.tag.as_deref().unwrap_or_default() != tag {
            return false;
        }
    }

    if let Some(id) = &simple.id {
        if node.attrs.get("id") != Some(id) {
            return false;
        }
    }

    if !simple.classes.is_empty() {
        let classes = node.attrs.get("class").cloned().unwrap_or_default();
        for class in &simple.classes {
            if !classes.split_whitespace().any(|c| c == class) {
                return false;
            }
        }
    }

    true
}

fn selector_matches(doc: &EngineDocument, node_id: usize, selector: &CssSelector) -> bool {
    if selector.parts.is_empty() {
        return false;
    }

    let mut cursor = Some(node_id);
    for simple in selector.parts.iter().rev() {
        let mut found = None;
        while let Some(idx) = cursor {
            let node = &doc.nodes[idx];
            if node_matches_simple(node, simple) {
                found = Some(idx);
                break;
            }
            cursor = node.parent;
        }

        let Some(idx) = found else { return false };
        cursor = doc.nodes[idx].parent;
    }

    true
}

fn selector_specificity(selector: &CssSelector) -> usize {
    let mut score = 0usize;
    for p in &selector.parts {
        if p.id.is_some() {
            score += 100;
        }
        score += p.classes.len() * 10;
        if p.tag.is_some() {
            score += 1;
        }
    }
    score
}

pub fn compute_layout(doc: &EngineDocument, css: &EngineStyleSheet, viewport_width: f32) -> LayoutTree {
    fn node_text_content(doc: &EngineDocument, node_id: usize, out: &mut String) {
        let node = &doc.nodes[node_id];
        if node.tag.is_none() && !node.text.trim().is_empty() {
            out.push_str(node.text.trim());
            out.push(' ');
        }
        for &child in &node.children {
            node_text_content(doc, child, out);
        }
    }

    fn build(doc: &EngineDocument, css: &EngineStyleSheet, node_id: usize, x: f32, y: &mut f32, width: f32) -> Option<LayoutBox> {
        let node = &doc.nodes[node_id];
        let tag = node.tag.clone().unwrap_or_default();

        if ["#comment", "script", "style", "head", "meta", "link"].contains(&tag.as_str()) {
            return None;
        }

        let mut computed: HashMap<String, (usize, usize, String)> = HashMap::new();
        for rule in &css.rules {
            for selector in &rule.selectors {
                if selector_matches(doc, node_id, selector) {
                    let spec = selector_specificity(selector);
                    for decl in &rule.declarations {
                        let entry = computed.entry(decl.property.clone()).or_insert((0, 0, String::new()));
                        if spec > entry.0 || (spec == entry.0 && rule.source_order >= entry.1) {
                            *entry = (spec, rule.source_order, decl.value.clone());
                        }
                    }
                }
            }
        }

        if computed.get("display").map(|v| v.2.as_str()) == Some("none") {
            return None;
        }

        let mut layout = LayoutBox {
            node_id,
            x,
            y: *y,
            width,
            height: 0.0,
            children: Vec::new(),
        };

        let padding = computed
            .get("padding")
            .and_then(|v| v.2.trim_end_matches("px").parse::<f32>().ok())
            .unwrap_or(0.0);

        let line_height = 20.0;
        let mut content_text = String::new();
        if tag == "p" || tag.starts_with('h') || tag == "li" || node.tag.is_none() {
            node_text_content(doc, node_id, &mut content_text);
        }

        if !content_text.trim().is_empty() {
            let approx_chars_per_line = ((width - (padding * 2.0)).max(80.0) / 8.0).max(10.0) as usize;
            let words = content_text.split_whitespace().collect::<Vec<_>>();
            let mut line_len = 0usize;
            let mut lines = 1usize;
            for w in words {
                if line_len == 0 {
                    line_len = w.len();
                } else if line_len + 1 + w.len() > approx_chars_per_line {
                    lines += 1;
                    line_len = w.len();
                } else {
                    line_len += 1 + w.len();
                }
            }
            layout.height += (lines as f32) * line_height;
        }

        let mut child_y = *y + padding + if layout.height > 0.0 { 4.0 } else { 0.0 };
        for &child in &node.children {
            if let Some(child_box) = build(doc, css, child, x + padding, &mut child_y, width - (padding * 2.0)) {
                child_y = child_box.y + child_box.height + 6.0;
                layout.children.push(child_box);
            }
        }

        if !layout.children.is_empty() {
            let bottom = layout
                .children
                .iter()
                .map(|c| c.y + c.height)
                .fold(layout.y, f32::max);
            layout.height = (bottom - layout.y) + padding;
        }

        if layout.height <= 0.0 {
            layout.height = if tag == "#document" || tag == "body" || tag == "html" {
                0.0
            } else {
                22.0 + padding
            };
        }

        *y = layout.y + layout.height + 6.0;
        Some(layout)
    }

    let mut y = 0.0;
    let root = build(doc, css, doc.root_id, 0.0, &mut y, viewport_width);
    LayoutTree {
        root,
        content_height: y,
    }
}

pub fn execute_javascript(source: &str) -> Result<String> {
    let mut ctx = JsContext::default();
    let result = ctx
        .eval(Source::from_bytes(source))
        .map_err(|e| anyhow!(e.to_string()))?;
    Ok(result.display().to_string())
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
        assert_eq!(
            resolve_url("https://example.com/a/b", "../c").unwrap(),
            "https://example.com/c"
        );
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

    #[test]
    fn spec_html_css_and_layout_start_work() {
        let html = "<html><body><h1 id='hero' class='title'>A</h1><p class='title'>B C D E F</p></body></html>";
        let css = "#hero { padding: 8px; } .title { display: block; } p { padding: 4px; }";

        let doc = parse_html_spec(html);
        assert!(!doc.nodes.is_empty());

        let stylesheet = parse_css_stylesheet(css);
        assert!(!stylesheet.rules.is_empty());

        let layout = compute_layout(&doc, &stylesheet, 900.0);
        assert!(layout.content_height >= 0.0);
    }

    #[test]
    fn javascript_runtime_executes_basic_code() {
        let value = execute_javascript("1 + 2 + 3").unwrap();
        assert_eq!(value, "6");
    }
}
