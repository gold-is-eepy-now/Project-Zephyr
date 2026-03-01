use eframe::egui;

fn normalize_url(input: &str) -> String {
    if input.contains("://") {
        input.to_string()
    } else {
        format!("https://{}", input)
    }
}

struct ZephyrGuiApp {
    address: String,
    page_text: String,
    status: String,
    history: Vec<String>,
    history_index: isize,
}

impl Default for ZephyrGuiApp {
    fn default() -> Self {
        Self {
            address: "https://duckduckgo.com".to_string(),
            page_text: "Welcome to Zephyr Rust GUI".to_string(),
            status: "Ready".to_string(),
            history: Vec::new(),
            history_index: -1,
        }
    }
}

impl ZephyrGuiApp {
    fn load(&mut self, url: &str, push_history: bool) {
        let normalized = normalize_url(url);
        self.status = format!("Loading {} ...", normalized);

        match zephyr::http_get(&normalized, 15, 5) {
            Ok(resp) => {
                self.page_text = zephyr::render_page_text(&resp.body, 120);
                self.address = normalized.clone();
                self.status = "Done".to_string();

                if push_history {
                    if self.history_index >= 0 && (self.history_index as usize) + 1 < self.history.len() {
                        self.history.truncate((self.history_index as usize) + 1);
                    }
                    if self.history.last().map(|h| h != &normalized).unwrap_or(true) {
                        self.history.push(normalized);
                        self.history_index = self.history.len() as isize - 1;
                    }
                }
            }
            Err(err) => {
                self.page_text = format!("Load error: {err}");
                self.status = "Load error".to_string();
            }
        }
    }

    fn can_back(&self) -> bool {
        self.history_index > 0
    }

    fn can_forward(&self) -> bool {
        self.history_index >= 0 && (self.history_index as usize) + 1 < self.history.len()
    }
}

impl eframe::App for ZephyrGuiApp {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        egui::TopBottomPanel::top("tabs").show(ctx, |ui| {
            ui.horizontal_wrapped(|ui| {
                ui.spacing_mut().item_spacing.x = 8.0;
                let _ = ui.selectable_label(true, "🌐 Explore Destinations");
                let _ = ui.selectable_label(false, "✈ Book Flights");
                let _ = ui.selectable_label(false, "⚙ Settings");
                ui.separator();
                ui.label("＋");
            });
        });

        egui::TopBottomPanel::top("toolbar").show(ctx, |ui| {
            ui.horizontal(|ui| {
                if ui.add_enabled(self.can_back(), egui::Button::new("←")).clicked() {
                    self.history_index -= 1;
                    let url = self.history[self.history_index as usize].clone();
                    self.load(&url, false);
                }
                if ui
                    .add_enabled(self.can_forward(), egui::Button::new("→"))
                    .clicked()
                {
                    self.history_index += 1;
                    let url = self.history[self.history_index as usize].clone();
                    self.load(&url, false);
                }
                if ui.button("↻").clicked() {
                    let url = self.address.clone();
                    self.load(&url, false);
                }
                if ui.button("⌂").clicked() {
                    self.load("https://duckduckgo.com", true);
                }

                let response = ui.add_sized(
                    [ui.available_width() - 110.0, 24.0],
                    egui::TextEdit::singleline(&mut self.address),
                );
                if response.lost_focus() && ui.input(|i| i.key_pressed(egui::Key::Enter)) {
                    let url = self.address.clone();
                    self.load(&url, true);
                }
                if ui.button("Go").clicked() {
                    let url = self.address.clone();
                    self.load(&url, true);
                }
            });
        });

        egui::CentralPanel::default().show(ctx, |ui| {
            egui::Frame::group(ui.style())
                .fill(egui::Color32::from_rgb(26, 40, 76))
                .show(ui, |ui| {
                    ui.add_space(8.0);
                    ui.heading("EXPLORE THE OPEN WEB");
                    ui.label("Privacy-first, open-source browsing with a modern Rust desktop shell.");
                    ui.add_space(8.0);
                    ui.horizontal(|ui| {
                        ui.group(|ui| {
                            ui.strong("1. Open Source Core");
                            ui.label("Learn More");
                        });
                        ui.group(|ui| {
                            ui.strong("2. Secure by Default");
                            ui.label("Learn More");
                        });
                        ui.group(|ui| {
                            ui.strong("3. Rust Engine");
                            ui.label("Learn More");
                        });
                    });
                    ui.add_space(8.0);
                });

            ui.add_space(8.0);
            ui.label(egui::RichText::new(&self.status).italics());
            ui.separator();

            egui::ScrollArea::vertical().show(ui, |ui| {
                ui.add(
                    egui::TextEdit::multiline(&mut self.page_text)
                        .desired_rows(26)
                        .desired_width(f32::INFINITY),
                );
            });
        });
    }
}

fn main() -> eframe::Result<()> {
    let native_options = eframe::NativeOptions {
        viewport: egui::ViewportBuilder::default()
            .with_title("Zephyr Browser (Rust GUI)")
            .with_inner_size([1280.0, 860.0]),
        ..Default::default()
    };

    eframe::run_native(
        "zephyr_gui",
        native_options,
        Box::new(|_cc| {
            let mut app = ZephyrGuiApp::default();
            app.load("https://duckduckgo.com", true);
            Ok(Box::new(app))
        }),
    )
}
