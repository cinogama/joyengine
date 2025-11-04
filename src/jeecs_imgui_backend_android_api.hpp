#pragma once

void jegui_android_init(struct android_app* app);
void jegui_android_shutdown();

void jegui_android_handleInputEvent();

int jegui_android_ShowSoftKeyboardInput();
int jegui_android_PollUnicodeChars();
