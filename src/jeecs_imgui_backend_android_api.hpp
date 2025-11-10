#pragma once

void jegui_android_init(void* egl_interface);
void jegui_android_shutdown();

void jegui_android_handleInputEvent();

int jegui_android_ShowSoftKeyboardInput();
int jegui_android_PollUnicodeChars();
