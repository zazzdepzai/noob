// Trong Launcher::onLaunchClicked()
auto status = [this](const std::string& phase, float pct) {
    std::lock_guard<std::mutex> lk(m_mutex);
    s().statusText = phase;
    s().progress   = pct;
};

std::string java = Minecraft::FindJava();
if (java.empty()) {
    s().showJavaPopup = true;
    return;
}

if (!Minecraft::EnsureVanilla(mcDir, status)) {
    s().taskState = TaskState::Failed;
    s().lastError = "Vanilla install failed";
    return;
}

std::string forgeVer;
if (!Minecraft::EnsureForge(mcDir, java, status, forgeVer)) {
    s().taskState = TaskState::Failed;
    s().lastError = "Forge install failed";
    return;
}

if (selectedVersion == 2) {  // "Forge + OptiFine"
    if (!Minecraft::EnsureOptiFine(mcDir, status)) {
        // OptiFine fail → vẫn cho chạy Forge thuần
    }
}

auto r = Minecraft::Launch(mcDir, java, forgeVer, activeName,
                           settings.ramMB,
                           settings.windowWidth, settings.windowHeight);
if (!r.ok) {
    s().taskState = TaskState::Failed;
    s().lastError = r.error;
    return;
}

if (settings.closeAfterLaunch) PostQuitMessage(0);
