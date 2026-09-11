#pragma once
class Launcher;

class UI {
public:
    static void ApplyStyle();
    static void Render(Launcher& L, float dt);
    static void RenderLoadingScreen(Launcher& L);
};
