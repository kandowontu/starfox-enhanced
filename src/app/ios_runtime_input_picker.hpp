#pragma once

// The iOS build uses UIKit's document browser because SDL's file-dialog
// backend is not implemented there. The callback is invoked on the app's
// event-pump thread with a sandbox-local copy or an error message.
using StarfoxIOSPickerCallback = void (*)(
    void* userdata, const char* selected_path, const char* error);

extern "C" void starfox_ios_show_runtime_input_picker(
    void* ui_window, StarfoxIOSPickerCallback callback, void* userdata);
