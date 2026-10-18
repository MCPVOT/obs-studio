//! Test-only module that exercises the actual frontend Tools actions/dialogs.
use obs_build::{NativeBuild, SharedLibrary, plugins};
use std::path::Path;

pub fn build(build: &NativeBuild, root: &Path, core: &Path, api: &Path) -> plugins::Module {
    let source = build.output.join("frontend-tools-probe.cpp");
    if std::fs::read_to_string(&source).ok().as_deref() != Some(SOURCE) {
        std::fs::write(&source, SOURCE).unwrap();
    }
    let mut target = SharedLibrary {
        name: "native-frontend-probe".into(),
        sources: vec![source],
        file_name: Some("native-frontend-probe.so".into()),
        include_dirs: vec![
            root.into(),
            root.join("libobs"),
            root.join("frontend/api"),
            build.output.join("config"),
        ],
        dependencies: vec![core.into(), api.into()],
        cpp_compile_options: vec![
            "-std=gnu++17".into(),
            "-fvisibility=hidden".into(),
            "-fvisibility-inlines-hidden".into(),
        ],
        ..SharedLibrary::default()
    };
    for package in ["simde", "Qt6Widgets", "Qt6Test"] {
        let dependency = pkg_config::Config::new()
            .cargo_metadata(false)
            .probe(package)
            .unwrap();
        target.include_dirs.extend(dependency.include_paths);
        let mut definitions: Vec<_> = dependency.defines.into_iter().collect();
        definitions.sort();
        target.definitions.extend(definitions);
        for path in dependency.link_paths {
            target.link_options.extend([
                format!("-L{}", path.display()),
                format!("-Wl,-rpath,{}", path.display()),
            ]);
        }
        target
            .link_options
            .extend(dependency.libs.iter().map(|name| format!("-l{name}")));
    }
    let data = build.output.join("frontend-tools-probe-data");
    std::fs::create_dir_all(&data).unwrap();
    plugins::Module {
        library: build.shared_library(&target).unwrap(),
        data,
    }
}

const SOURCE: &str = r#"
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QDialog>
#include <QListWidget>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QImage>
#include <QTimer>
#include <QEventLoop>
#include <QPointer>
#include <QPixmap>
#include <QTableView>
#include <QLineEdit>
#include <QDockWidget>
#include <QTest>
#include <QSignalSpy>
#include <QElapsedTimer>
#include <QUrl>
#include <QScreen>
#include <cstdlib>

OBS_DECLARE_MODULE()

static bool open_dialog(QMenu *menu, const char *label, const char *type, const char *image, int scripts = -1, bool websocket = false)
{
    QAction *selected = nullptr;
    for (QAction *action : menu->actions()) {
        QString text = action->text(); text.remove('&');
        if (text == label) { selected = action; break; }
    }
    if (!selected) { blog(LOG_ERROR, "cc::Build missing Tools action %s", label); return false; }
    // SceneSwitcher runs a nested modal event loop and is destroyed before
    // trigger() returns. OutputTimer instead queues its show() after 250 ms.
    // Inspect and close both while their event loops are running.
    QObject context;
    QEventLoop loop;
    bool completed = false;
    bool passed = false;
    QTimer::singleShot(400, &context, [&]() {
        QPointer<QWidget> dialog;
        for (QWidget *widget : QApplication::topLevelWidgets())
            if (widget->inherits(type) && widget->isVisible()) { dialog = widget; break; }
        if (!dialog) {
            blog(LOG_ERROR, "cc::Build Tools dialog %s did not open", type);
        } else {
            bool valid = true;
            if (scripts >= 0) {
                QListWidget *list = dialog->findChild<QListWidget *>("scripts");
                valid = list && list->count() == scripts;
                if (!valid) blog(LOG_ERROR, "cc::Build incorrect saved script count");
                if (valid) {
                    for (int row = 0; row < scripts; ++row) {
                        list->setCurrentRow(row);
                        // OBSPropertiesView queues its initial property reload.
                        QApplication::processEvents();
                        QLabel *description = dialog->findChild<QLabel *>("description");
                        const auto fields = dialog->findChildren<QSpinBox *>();
                        bool seed = false;
                        for (QSpinBox *field : fields) seed = seed || field->value() == 41;
                        if (!description || !description->text().startsWith("Native GUI ") || !seed) {
                            blog(LOG_ERROR, "cc::Build saved script properties were not displayed for row %d", row);
                            valid = false;
                        }
                    }
                    if (scripts) {
                        list->setCurrentRow(0);
                        QApplication::processEvents();
                    }
                }
            }
            if (websocket) {
                QPushButton *button = dialog->findChild<QPushButton *>("showConnectInfoButton");
                if (!button || obs_video_active()) {
                    blog(LOG_ERROR, "cc::Build websocket connection dialog requires idle output and its button");
                    valid = false;
                } else {
                    button->click();
                    QApplication::processEvents();
                    QPointer<QWidget> info;
                    for (QWidget *widget : QApplication::topLevelWidgets())
                        if (widget->inherits("ConnectInfo") && widget->isVisible()) { info = widget; break; }
                    QLabel *code = info ? info->findChild<QLabel *>("qrCodeLabel") : nullptr;
                    const QImage qr = code ? code->pixmap().toImage() : QImage();
                    bool black = false, white = false;
                    for (int y = 0; y < qr.height(); ++y)
                        for (int x = 0; x < qr.width(); ++x) {
                            int value = qr.pixelColor(x, y).lightness();
                            black = black || value < 32; white = white || value > 223;
                        }
                    const char *directory = std::getenv("OBS_BUILD_GUI_EVIDENCE");
                    valid = valid && info && black && white && directory &&
                        info->grab().save(QString::fromUtf8(directory) + "/websocket-connect-info.png");
                    if (info) info->close();
                    if (!valid) blog(LOG_ERROR, "cc::Build websocket connection QR dialog failed");
                }
            }
            const char *directory = std::getenv("OBS_BUILD_GUI_EVIDENCE");
            bool saved = directory && dialog->grab().save(QString::fromUtf8(directory) + "/" + image);
            bool closed = dialog->close();
            passed = valid && saved && closed && (!dialog || !dialog->isVisible());
            if (!passed) blog(LOG_ERROR, "cc::Build Tools inspection/screenshot/close failed");
        }
        completed = true;
        loop.quit();
    });
    selected->trigger();
    if (!completed) loop.exec();
    return passed;
}

static bool verify_browser_panel(QMainWindow *main)
{
    QAction *action = nullptr;
    for (QAction *candidate : main->findChildren<QAction *>()) {
        QString text = candidate->text(); text.remove('&');
        if (text == "Custom Browser Docks") { action = candidate; break; }
    }
    const char *directory = std::getenv("OBS_BUILD_GUI_EVIDENCE");
    if (!action || !directory) { blog(LOG_ERROR, "cc::Build browser dock action missing"); return false; }
    blog(LOG_INFO, "cc::Build opening browser dock editor");
    action->trigger();
    QPointer<QWidget> dialog;
    for (QWidget *widget : QApplication::topLevelWidgets())
        if (widget->inherits("OBSExtraBrowsers") && widget->isVisible()) { dialog = widget; break; }
    auto *table = dialog ? dialog->findChild<QTableView *>("table") : nullptr;
    auto *apply = dialog ? dialog->findChild<QPushButton *>("apply") : nullptr;
    if (!table || !table->model() || !apply) { blog(LOG_ERROR, "cc::Build browser dock editor missing"); return false; }
    auto *model = table->model();
    // This model commits through its line-edit delegate, not setData(). Drive
    // the actual editor so validation and new-row insertion also run.
    auto edit_cell = [&](int column, const QString &text) {
        const auto index = model->index(0, column);
        table->setCurrentIndex(index);
        table->openPersistentEditor(index);
        QApplication::processEvents();
        QLineEdit *editor = nullptr;
        const auto center = table->viewport()->mapToGlobal(table->visualRect(index).center());
        for (auto *candidate : table->findChildren<QLineEdit *>())
            if (candidate->isVisible() && candidate->rect().contains(candidate->mapFromGlobal(center))) editor = candidate;
        if (!editor) { blog(LOG_ERROR, "cc::Build browser dock cell editor missing"); return false; }
        editor->selectAll(); QTest::keyClicks(editor, text); QTest::keyClick(editor, Qt::Key_Return);
        QApplication::processEvents();
        const bool committed = model->index(0, column).data().toString() == text;
        table->closePersistentEditor(index);
        if (!committed) blog(LOG_ERROR, "cc::Build browser dock cell %d edit was not committed", column);
        return committed;
    };
    if (!edit_cell(0, "Native CEF Panel") || !edit_cell(1, "about:blank")) return false;
    blog(LOG_INFO, "cc::Build creating browser dock");
    apply->click();
    QDockWidget *dock = nullptr;
    for (auto *candidate : main->findChildren<QDockWidget *>())
        if (candidate->windowTitle() == "Native CEF Panel") { dock = candidate; break; }
    if (!dock || !dock->widget()) { blog(LOG_ERROR, "cc::Build browser dock was not created"); return false; }
    QSignalSpy titles(dock->widget(), SIGNAL(titleChanged(QString)));
    if (!titles.isValid()) { blog(LOG_ERROR, "cc::Build browser title signal missing"); return false; }
    const QString url = QUrl::fromLocalFile(QString::fromUtf8(directory) + "/browser-panel.html").toString();
    if (!edit_cell(1, url)) return false;
    blog(LOG_INFO, "cc::Build navigating browser dock");
    apply->click();
    bool loaded = false;
    QElapsedTimer timer; timer.start();
    while (!loaded && timer.elapsed() < 20000) {
        for (const auto &signal : titles)
            if (!signal.isEmpty() && signal.front().toString() == "Native CEF Panel Ready") loaded = true;
        if (!loaded) {
            const auto remaining = 20000 - timer.elapsed();
            if (remaining <= 0 || !titles.wait(static_cast<int>(remaining))) break;
        }
    }
    bool saved = dialog->grab().save(QString::fromUtf8(directory) + "/browser-dock-editor.png");
    // CEF uses a native child window, which QWidget::grab() cannot capture.
    dialog->hide(); dock->raise(); dock->activateWindow();
    QImage pixels;
    bool rendered = false;
    timer.restart();
    while (!rendered && timer.elapsed() < 5000) {
        QTest::qWait(50);
        pixels = dock->screen()->grabWindow(dock->winId()).toImage();
        if (!pixels.isNull()) {
            const auto color = pixels.pixelColor(pixels.width() / 2, pixels.height() / 2);
            rendered = std::abs(color.red() - 17) <= 4 && std::abs(color.green() - 203) <= 4 && std::abs(color.blue() - 77) <= 4;
        }
    }
    saved = !pixels.isNull() && pixels.save(QString::fromUtf8(directory) + "/browser-panel.png") && saved;
    dialog->close(); dock->hide();
    if (!loaded || !rendered || !saved) { blog(LOG_ERROR, "cc::Build browser dock page readiness or rendered pixels failed"); return false; }
    blog(LOG_INFO, "cc::Build browser dock creation, URL update, CEF JavaScript title and rendered pixels verified");
    return true;
}

static void verify_dialogs()
{
    auto *main = static_cast<QMainWindow *>(obs_frontend_get_main_window());
    if (!main) { blog(LOG_ERROR, "cc::Build main window missing"); return; }
    QMenu *tools = nullptr;
    for (QAction *action : main->menuBar()->actions()) {
        QString text = action->text(); text.remove('&');
        if (text == "Tools") tools = action->menu();
    }
    if (!tools) { blog(LOG_ERROR, "cc::Build Tools menu missing"); return; }
    if (std::getenv("OBS_BUILD_FRONTEND_TOOLS")) {
        if (!open_dialog(tools, "Automatic Scene Switcher", "SceneSwitcher", "tools-scene-switcher.png")) return;
        if (!open_dialog(tools, "Output Timer", "OutputTimer", "tools-output-timer.png")) return;
        const char *count = std::getenv("OBS_BUILD_GUI_SCRIPT_COUNT");
        if (count && std::atoi(count) >= 0 && !open_dialog(tools, "Scripts", "ScriptsTool", "tools-scripts.png", std::atoi(count))) return;
        if (!count || std::atoi(count) < 0) {
            for (QAction *action : tools->actions()) {
                QString text = action->text(); text.remove('&');
                if (text == "Scripts") { blog(LOG_ERROR, "cc::Build disabled Scripts action is present"); return; }
            }
        }
    }
    if (std::getenv("OBS_BUILD_WEBSOCKET") &&
        !open_dialog(tools, "WebSocket Server Settings", "SettingsDialog", "websocket-settings.png", -1, true)) return;
    if (std::getenv("OBS_BUILD_BROWSER") && !std::getenv("OBS_BUILD_DISABLE_BROWSER_PANELS") &&
        !verify_browser_panel(main)) return;
    blog(LOG_INFO, "cc::Build frontend Tools dialogs verified");
}

static bool frontend_exited = false;

static void frontend_event(enum obs_frontend_event event, void *)
{
    if (event == OBS_FRONTEND_EVENT_EXIT) frontend_exited = true;
    if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING)
        QTimer::singleShot(0, verify_dialogs);
}

bool obs_module_load(void)
{
    obs_frontend_add_event_callback(frontend_event, nullptr);
    return true;
}
void obs_module_unload(void) { if (!frontend_exited) obs_frontend_remove_event_callback(frontend_event, nullptr); }
"#;
