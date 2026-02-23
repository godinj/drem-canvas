#import <Cocoa/Cocoa.h>
#include "NativeDialogs.h"

namespace dc
{
namespace platform
{

void NativeDialogs::showOpenPanel (const std::string& title,
                                    const std::vector<std::string>& fileTypes,
                                    std::function<void (const std::string&)> callback)
{
    @autoreleasepool
    {
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        [panel setTitle:[NSString stringWithUTF8String:title.c_str()]];
        [panel setCanChooseFiles:YES];
        [panel setCanChooseDirectories:NO];
        [panel setAllowsMultipleSelection:NO];

        if (!fileTypes.empty())
        {
            NSMutableArray* types = [NSMutableArray array];
            for (const auto& type : fileTypes)
                [types addObject:[NSString stringWithUTF8String:type.c_str()]];
            [panel setAllowedFileTypes:types];
        }

        [panel beginWithCompletionHandler:^(NSModalResponse result)
        {
            if (result == NSModalResponseOK && panel.URLs.count > 0)
            {
                std::string path = [[panel.URLs[0] path] UTF8String];
                if (callback) callback (path);
            }
        }];
    }
}

void NativeDialogs::showSavePanel (const std::string& title,
                                    const std::string& defaultName,
                                    std::function<void (const std::string&)> callback)
{
    @autoreleasepool
    {
        NSSavePanel* panel = [NSSavePanel savePanel];
        [panel setTitle:[NSString stringWithUTF8String:title.c_str()]];
        [panel setNameFieldStringValue:[NSString stringWithUTF8String:defaultName.c_str()]];

        [panel beginWithCompletionHandler:^(NSModalResponse result)
        {
            if (result == NSModalResponseOK && panel.URL != nil)
            {
                std::string path = [[panel.URL path] UTF8String];
                if (callback) callback (path);
            }
        }];
    }
}

void NativeDialogs::showAlert (const std::string& title, const std::string& message)
{
    @autoreleasepool
    {
        NSAlert* alert = [[NSAlert alloc] init];
        [alert setMessageText:[NSString stringWithUTF8String:title.c_str()]];
        [alert setInformativeText:[NSString stringWithUTF8String:message.c_str()]];
        [alert setAlertStyle:NSAlertStyleInformational];
        [alert addButtonWithTitle:@"OK"];
        [alert runModal];
    }
}

bool NativeDialogs::showConfirmation (const std::string& title, const std::string& message)
{
    @autoreleasepool
    {
        NSAlert* alert = [[NSAlert alloc] init];
        [alert setMessageText:[NSString stringWithUTF8String:title.c_str()]];
        [alert setInformativeText:[NSString stringWithUTF8String:message.c_str()]];
        [alert setAlertStyle:NSAlertStyleWarning];
        [alert addButtonWithTitle:@"OK"];
        [alert addButtonWithTitle:@"Cancel"];
        return [alert runModal] == NSAlertFirstButtonReturn;
    }
}

} // namespace platform
} // namespace dc
