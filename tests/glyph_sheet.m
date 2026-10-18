// Offscreen contact sheet of the menu bar glyphs: tests/glyph_sheet.m includes src/menubar.m
// so it renders the exact masks the daemon builds. Usage: glyph_sheet out.png [App.app ...]
#include "../src/menubar.m"

CGImageRef apps_icon(pid_t pid, int px) {
    (void)pid; (void)px;
    return NULL;
}
char config_key_char(int i) { return (char)('1' + i); }

enum { Z = 5, CELL = GLYPH_PX * Z / 2 + 16 };

static CGImageRef icon_at(NSString *path, int px, NSString **bid) {
    NSBundle *b = [NSBundle bundleWithPath:path];
    *bid = b.bundleIdentifier;
    NSImage *img = [NSWorkspace.sharedWorkspace iconForFile:path];
    NSRect r = NSMakeRect(0, 0, px, px);
    CGImageRef src = [img CGImageForProposedRect:&r context:nil hints:nil];
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef c = CGBitmapContextCreate(NULL, px, px, 8, 0, cs, (CGBitmapInfo)kCGImageAlphaPremultipliedLast);
    CGColorSpaceRelease(cs);
    CGContextSetInterpolationQuality(c, kCGInterpolationHigh);
    CGContextDrawImage(c, CGRectMake(0, 0, px, px), src);
    CGImageRef out = CGBitmapContextCreateImage(c);
    CGContextRelease(c);
    return out;
}

int main(int argc, char **argv) {
    @autoreleasepool {
        NSArray *paths = argc > 2 ? nil : @[
            @"/Applications/1Password.app", @"/Applications/Slack.app", @"/Applications/Visual Studio Code.app",
            @"/Applications/ChatGPT.app", @"/Applications/Ghostty.app", @"/Applications/Raycast.app",
            @"/System/Library/CoreServices/Finder.app", @"/Applications/Safari.app",
            @"/System/Applications/Utilities/Terminal.app", @"/System/Applications/System Settings.app",
            @"/System/Applications/Notes.app", @"/System/Applications/Mail.app",
            @"/System/Applications/Calendar.app", @"/Applications/Google Chrome.app"];
        if (!paths) {
            NSMutableArray *a = [NSMutableArray new];
            for (int i = 2; i < argc; i++) [a addObject:@(argv[i])];
            paths = a;
        }
        int cols = 7, rows = ((int)paths.count + cols - 1) / cols;
        int cw = CELL, ch = CELL * 3;
        CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
        CGContextRef c = CGBitmapContextCreate(NULL, cols * cw, rows * ch, 8, 0, cs, (CGBitmapInfo)kCGImageAlphaPremultipliedLast);
        CGContextSetRGBFillColor(c, 0.16, 0.16, 0.18, 1);
        CGContextFillRect(c, CGRectMake(0, 0, cols * cw, rows * ch));
        CGContextSetInterpolationQuality(c, kCGInterpolationNone);
        int s = GLYPH_PX * Z / 2;
        for (int k = 0; k < (int)paths.count; k++) {
            int cx = (k % cols) * cw, cy = (rows - 1 - k / cols) * ch;
            NSString *bid;
            CGImageRef icon = icon_at(paths[k], GLYPH_PX, &bid);
            CGImageRef mask = drawn_mask(bid);
            if (!mask) mask = glyph_mask(icon);
            CGContextDrawImage(c, CGRectMake(cx + 8, cy + 2 * CELL + 8, s, s), icon);
            for (int f = 0; f < 2; f++) {
                CGContextRef l = CGBitmapContextCreate(NULL, s, s, 8, 0, cs, (CGBitmapInfo)kCGImageAlphaPremultipliedLast);
                CGContextSetRGBFillColor(l, 1, 1, 1, 1);
                CGRect cell = CGRectMake(0, 0, s, s);
                if (f) {
                    CGPathRef t = CGPathCreateWithRoundedRect(cell, 5 * Z / 2.0, 5 * Z / 2.0, NULL);
                    CGContextAddPath(l, t);
                    CGContextFillPath(l);
                    CGPathRelease(t);
                    CGContextSetBlendMode(l, kCGBlendModeDestinationOut);
                }
                CGContextSetInterpolationQuality(l, kCGInterpolationNone);
                if (mask) CGContextDrawImage(l, CGRectInset(cell, PAD * Z, PAD * Z), mask);
                CGImageRef li = CGBitmapContextCreateImage(l);
                CGContextDrawImage(c, CGRectMake(cx + 8, cy + (1 - f) * CELL + 8, s, s), li);
                CGImageRelease(li);
                CGContextRelease(l);
            }
            if (mask) CGImageRelease(mask);
            CGImageRelease(icon);
        }
        CGImageRef out = CGBitmapContextCreateImage(c);
        NSBitmapImageRep *rep = [[NSBitmapImageRep alloc] initWithCGImage:out];
        [[rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}] writeToFile:@(argv[1]) atomically:YES];
    }
    return 0;
}
