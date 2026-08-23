/**
 * @file opengl_platform.mm
 * @author clk
 */

#import <Cocoa/Cocoa.h>
#import <OpenGL/OpenGL.h>

#import <spiration/renderer/opengl_platform.h>

static NSOpenGLContext* CreateOpenGLContext(NSView* view) {
    if (!view) return nil;

    NSOpenGLPixelFormatAttribute accelerated_attrs[] = {
        NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion4_1Core,
        NSOpenGLPFAColorSize, 24,
        NSOpenGLPFAAlphaSize, 8,
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFAAccelerated,
        0
    };
    NSOpenGLPixelFormat* pixel_format =
        [[NSOpenGLPixelFormat alloc] initWithAttributes:accelerated_attrs];

    if (!pixel_format) {
        NSOpenGLPixelFormatAttribute software_attrs[] = {
            NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion4_1Core,
            NSOpenGLPFAColorSize, 24,
            NSOpenGLPFAAlphaSize, 8,
            NSOpenGLPFADoubleBuffer,
            0
        };
        pixel_format = [[NSOpenGLPixelFormat alloc] initWithAttributes:software_attrs];
    }
    if (!pixel_format) {
        NSLog(@"Failed to create NSOpenGLPixelFormat (4.1 Core Profile)");
        return nil;
    }

    NSOpenGLContext* context = [[NSOpenGLContext alloc] initWithFormat:pixel_format
                                                         shareContext:nil];
    if (!context) {
        NSLog(@"Failed to create NSOpenGLContext");
        return nil;
    }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    [context setView:view];
#pragma clang diagnostic pop

    GLint swap_interval = 1;
    [context setValues:&swap_interval forParameter:NSOpenGLContextParameterSwapInterval];

    return context;
}

extern "C" {

void* spiration_gl_create_context(void* ns_view) {
    NSOpenGLContext* context = CreateOpenGLContext((__bridge NSView*)ns_view);
    if (!context) return nullptr;
    return (__bridge_retained void*)context;
}

int spiration_gl_make_current(void* context) {
    NSOpenGLContext* ctx = (__bridge NSOpenGLContext*)context;
    if (!ctx) return 0;
    [ctx makeCurrentContext];
    return 1;
}

void spiration_gl_flush(void* context) {
    NSOpenGLContext* ctx = (__bridge NSOpenGLContext*)context;
    [ctx flushBuffer];
}

void spiration_gl_update(void* context) {
    NSOpenGLContext* ctx = (__bridge NSOpenGLContext*)context;
    [ctx update];
}

void spiration_gl_destroy(void* context) {
    if (!context) return;
    NSOpenGLContext* ctx = (__bridge_transfer NSOpenGLContext*)context;
    [ctx clearDrawable];
    ctx = nil;
}

} // extern "C"
