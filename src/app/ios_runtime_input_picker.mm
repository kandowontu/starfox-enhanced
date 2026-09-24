#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <dispatch/dispatch.h>

#include "ios_runtime_input_picker.hpp"

@interface StarfoxRuntimeInputPickerDelegate : NSObject <UIDocumentPickerDelegate> {
@public
    StarfoxIOSPickerCallback callback;
    void* userdata;
}
@end

static StarfoxRuntimeInputPickerDelegate* activePicker = nil;

@implementation StarfoxRuntimeInputPickerDelegate

- (void)finishWithPath:(NSString*)path error:(NSString*)error {
    if (callback != nullptr) {
        callback(userdata, path == nil ? nullptr : path.fileSystemRepresentation,
            error == nil ? nullptr : error.UTF8String);
        callback = nullptr;
    }
    activePicker = nil;
}

- (void)documentPicker:(UIDocumentPickerViewController*)picker
    didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls {
    (void)picker;
    NSURL* selected = urls.firstObject;
    if (selected == nil) {
        [self finishWithPath:nil error:nil];
        return;
    }

    // A provider URL may lose access when the picker closes. Stage a copy in
    // our own temporary directory before returning to the C++ asset loader.
    const BOOL scoped = [selected startAccessingSecurityScopedResource];
    NSString* destination = [NSTemporaryDirectory() stringByAppendingPathComponent:
        [NSString stringWithFormat:@"starfox-runtime-input-%@.bin",
            [NSUUID UUID].UUIDString]];
    NSError* copyError = nil;
    BOOL copied = [[NSFileManager defaultManager] copyItemAtURL:selected
        toURL:[NSURL fileURLWithPath:destination] error:&copyError];
    if (scoped) [selected stopAccessingSecurityScopedResource];
    if (!copied) {
        [self finishWithPath:nil error:copyError.localizedDescription
            ?: @"Could not copy the selected file into the app sandbox"];
        return;
    }
    [self finishWithPath:destination error:nil];
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController*)picker {
    (void)picker;
    [self finishWithPath:nil error:nil];
}

@end

extern "C" void starfox_ios_show_runtime_input_picker(
    void* ui_window, StarfoxIOSPickerCallback callback, void* userdata) {
    if (callback == nullptr) return;
    if (![NSThread isMainThread]) {
        // SDL may run the game loop off the UIKit thread. Never fail the
        // import merely because its request originated there.
        dispatch_async(dispatch_get_main_queue(), ^{
            starfox_ios_show_runtime_input_picker(ui_window, callback, userdata);
        });
        return;
    }
    UIWindow* window = (__bridge UIWindow*)ui_window;
    UIViewController* presenter = window.rootViewController;
    if (presenter == nil || activePicker != nil) {
        callback(userdata, nullptr, "The iOS file picker is not ready");
        return;
    }
    while (presenter.presentedViewController != nil) {
        presenter = presenter.presentedViewController;
    }
    activePicker = [StarfoxRuntimeInputPickerDelegate new];
    activePicker->callback = callback;
    activePicker->userdata = userdata;
    UIDocumentPickerViewController* picker =
        [[UIDocumentPickerViewController alloc]
            initForOpeningContentTypes:@[UTTypeData] asCopy:YES];
    picker.delegate = activePicker;
    [presenter presentViewController:picker animated:YES completion:nil];
}
