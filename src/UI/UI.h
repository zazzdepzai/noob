#pragma once
#include "../Launcher.h"
#include "../Settings.h"
#include "../ModManager.h"

namespace UI {
    void ApplyStyle();
    void Render(Launcher& launcher, float dt);
    void RenderLoadingScreen(Launcher& launcher);
}
