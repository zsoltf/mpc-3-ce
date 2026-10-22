#!/usr/bin/env python3
"""Apply explicit MPC render-node adaptations to the pinned Cardinal source copy."""
import argparse
from pathlib import Path
import shutil

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("source", type=Path)
args = parser.parse_args()
kit = Path(__file__).resolve().parent
root = args.source
window = root / "src/override/Window.cpp"
source = window.read_text()
if "mpclearn-full-gpu.hpp" in source:
    raise SystemExit("Window already prepared; preserve and inspect the existing adaptation")

def replace(old, new):
    global source
    if source.count(old) != 1:
        raise SystemExit("Pinned Window source no longer matches: " + old[:80])
    source = source.replace(old, new)

replace("namespace rack {\nnamespace window {", '#include "mpclearn-full-gpu.hpp"\n#include "overlay-gpu.hpp"\n#include <vector>\nvoid mpcTouchDrawOverlay(rack::window::Window*, NVGcontext*);\n\nnamespace rack {\nnamespace window {')
replace("\tDGL_NAMESPACE::Application hiddenApp;\n\tDGL_NAMESPACE::Window hiddenWindow;", """\tTouchGpu gpu;
\tbool restoreFailed = false;
\tbool touchViewportMoving = false, touchViewportDirty = false;
\tNVGLUframebuffer* touchFramebuffer = nullptr;
\tuint32_t touchWidth = 0, touchHeight = 0, touchRevision = 0;
\tstd::vector<unsigned char> touchReadback;
\tCardinalOverlayGpu overlayGpu;
\tuint32_t readbackRevision = 0;""")
replace("""\tInternal()
#if DISTRHO_PLUGIN_WANT_DIRECT_ACCESS
\t\t: hiddenApp(false),
\t\t  hiddenWindow(hiddenApp, 0, DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT, 0.0, true)
\t{
\t\thiddenWindow.setIgnoringKeyRepeat(true);
\t\thiddenApp.idle();
\t}
#else
\t{}
#endif""", "\tInternal() {}")
replace("\tconst DGL_NAMESPACE::Window::ScopedGraphicsContext sgc(internal->hiddenWindow);", """\tTouchEglScope sgc(internal->gpu, internal->restoreFailed);
\tinternal->gpu.open(sgc.original.display, sgc.es.display);
\tinternal->gpu.enter();""")
replace("""\t\tDGL_NAMESPACE::Window::ScopedGraphicsContext sgc(internal->hiddenWindow);
\t\tinternal->hiddenWindow.close();
\t\tinternal->hiddenApp.idle();""", """\t\tTouchEglScope sgc(internal->gpu, internal->restoreFailed);
\t\tinternal->gpu.enter();
\t\t// This owned NanoVG context never attaches a desktop UI. Deliver the
\t\t// canonical detach event before deleting it, while the scene and GL
\t\t// context still exist. Cached widgets otherwise outlive their context.
\t\tif (vg && APP->scene) {
\t\t\twidget::Widget::ContextDestroyEvent event = {};
\t\t\tevent.vg = vg;
\t\t\tAPP->scene->onContextDestroy(event);
\t\t}
\t\tinternal->overlayGpu.release(internal->gpu.display);
\t\tif (internal->touchFramebuffer) nvgluDeleteFramebuffer(internal->touchFramebuffer);""")
replace("\tdelete internal;\n}", "\tinternal->gpu.closeUnbound();\n\tdelete internal;\n}")
replace("void Window::screenshot(const std::string& screenshotPath)", '#include "mpclearn-full-window-adapter.inc"\n\nvoid Window::screenshot(const std::string& screenshotPath)')
replace("double Window::getFrameDurationRemaining() {", """double Window::getFrameDurationRemaining() {
\t// The native view paints on input. No idle frame will finish skipped
\t// widget caches, so complete this paint's dirty caches before presenting.
\tif (internal->touchFramebuffer) return 0.0;
""")
window.write_text(source)
# Reuse canonical widget textures while the viewport moves. Rack already
# scales a cached image to the current world transform; only scale/subpixel
# invalidation needs deferring. Content dirtiness and newly exposed clipping
# still render immediately, at the cached resolution until navigation ends.
framebuffer = root / "src/Rack/src/widget/FramebufferWidget.cpp"
content = framebuffer.read_text()
old = "namespace rack {\nnamespace widget {"
new = "namespace rack {\nnamespace window { bool mpcWindowViewportMoving(Window*); }\nnamespace widget {"
if content.count(old) != 1:
    raise SystemExit("Pinned FramebufferWidget namespace mismatch")
content = content.replace(old, new)
old = "\t// Re-render if drawing to a new subpixel location."
new = """\tconst bool moving = window::mpcWindowViewportMoving(APP->window) && internal->fb;
\t// Re-render if drawing to a new subpixel location."""
if content.count(old) != 1:
    raise SystemExit("Pinned FramebufferWidget invalidation mismatch")
content = content.replace(old, new)
content = content.replace("if (dirtyOnSubpixelChange && APP->window->fbDirtyOnSubpixelChange()", "if (!moving && dirtyOnSubpixelChange && APP->window->fbDirtyOnSubpixelChange()")
content = content.replace("else if (!scale.equals(internal->fbScale))", "else if (!moving && !scale.equals(internal->fbScale))")
old = "\t\t\trender(scale, offsetF, args.clipBox);"
new = "\t\t\trender(moving ? internal->fbScale : scale, moving ? internal->fbOffsetF : offsetF, args.clipBox);"
if content.count(old) != 1:
    raise SystemExit("Pinned FramebufferWidget render mismatch")
content = content.replace(old, new)
framebuffer.write_text(content)
zoom = root / "src/Rack/src/widget/ZoomWidget.cpp"
content = zoom.read_text()
old = "namespace rack {\nnamespace widget {"
new = "namespace rack {\nnamespace window { bool mpcWindowDeferZoomDirty(Window*, widget::Widget*); }\nnamespace widget {"
if content.count(old) != 1:
    raise SystemExit("Pinned ZoomWidget namespace mismatch")
content = content.replace('#include <widget/ZoomWidget.hpp>', '#include <widget/ZoomWidget.hpp>\n#include <context.hpp>')
content = content.replace(old, new)
old = "\t// Dispatch Dirty event"
new = """\t// Navigation still updates geometry at input time. Only its cache
\t// invalidation is deferred, and replayed once when the gesture ends.
\tif (window::mpcWindowDeferZoomDirty(APP->window, this)) return;

\t// Dispatch Dirty event"""
if content.count(old) != 1:
    raise SystemExit("Pinned ZoomWidget dirty event mismatch")
zoom.write_text(content.replace(old, new))
# The bundled text widget's one-past-last-line endpoint serializes an extra
# newline. Preserve actual text across canonical Rack save/recall instead.
text_editor = root / "plugins/Cardinal/src/DearImGuiColorTextEditor/TextEditor.cpp"
content = text_editor.read_text()
old = """std::string TextEditor::GetText() const
{
\treturn GetText(Coordinates(), Coordinates((int)mLines.size(), 0));
}"""
new = """std::string TextEditor::GetText() const
{
\tif (mLines.empty()) return {};
\tconst int lastLine = (int)mLines.size() - 1;
\treturn GetText(Coordinates(), Coordinates(lastLine, GetLineMaxColumn(lastLine)));
}"""
if content.count(old) != 1:
    raise SystemExit("Pinned text serialization endpoint mismatch")
text_editor.write_text(content.replace(old, new))
base = root / "dpf/Makefile.base.mk"
content = base.read_text()
old = "OPENGL_FLAGS = $(shell $(PKG_CONFIG) --cflags gl x11)\nOPENGL_LIBS  = $(shell $(PKG_CONFIG) --libs gl x11)"
if content.count(old) != 1:
    raise SystemExit("Pinned DPF Linux GL link seam mismatch")
content = content.replace(old, """ifeq ($(USE_GLES2),true)
OPENGL_FLAGS = $(shell $(PKG_CONFIG) --cflags glesv2 x11)
# Native GLES owns drawing. GLX is linked only for dormant desktop UI code.
OPENGL_LIBS  = $(shell $(PKG_CONFIG) --libs glesv2 glx x11)
else
OPENGL_FLAGS = $(shell $(PKG_CONFIG) --cflags gl x11)
OPENGL_LIBS  = $(shell $(PKG_CONFIG) --libs gl x11)
endif""")
base.write_text(content)
vector = root / "carla/source/native-plugins/external/distrho-vectorjuice/VectorJuiceUI.cpp"
content = vector.read_text()
old = """    glColor4f(0.0f, 1.0f, 0.0f, 0.05f);
    glLineWidth(4);
    glBegin(GL_LINES);
        glVertex2i(x+ fImgRoundlet.getWidth()/2, y+ fImgRoundlet.getHeight()/2);
        glVertex2i(nOrbitX+15, nOrbitY+15);
    glEnd();
    glBegin(GL_LINES);
        glVertex2i(nOrbitX+15, nOrbitY+15);
        glVertex2i(nSubOrbitX+15, nSubOrbitY+14);
    glEnd();"""
if content.count(old) != 1:
    raise SystemExit("Pinned VectorJuice line rendering seam mismatch")
content = content.replace('#include "VectorJuiceUI.hpp"', '#include "VectorJuiceUI.hpp"\n#include "Color.hpp"\n#include "Geometry.hpp"')
content = content.replace(old, """    DGL_NAMESPACE::Color(0.0f, 1.0f, 0.0f, 0.05f).setFor(context, true);
    DGL_NAMESPACE::Line<int>(x + fImgRoundlet.getWidth()/2,
                            y + fImgRoundlet.getHeight()/2,
                            nOrbitX + 15, nOrbitY + 15).draw(context, 4);
    DGL_NAMESPACE::Line<int>(nOrbitX + 15, nOrbitY + 15,
                            nSubOrbitX + 15, nSubOrbitY + 14).draw(context, 4);""")
vector.write_text(content)
shutil.copyfile(kit / "touch-gpu.hpp", root / "src/mpclearn-full-gpu.hpp")
shutil.copyfile(kit / "full-window-adapter.inc", root / "src/mpclearn-full-window-adapter.inc")
for name in ("overlay-api.h", "overlay-gpu.hpp"):
    shutil.copyfile(kit / name, root / "src" / name)
for relative in ("Makefile", "src/Makefile.cardinal.mk"):
    path = root / relative
    content = path.read_text()
    old = "ifeq ($(WASM),true)\n"
    # Both pinned files set UI_TYPE inside this wasm branch. GLES2 needs the
    # same DPF renderer selection, independently of the Rack NanoVG define.
    marker = "UI_TYPE = gles2"
    at = content.index(marker)
    start = content.rfind(old, 0, at)
    if start < 0:
        raise SystemExit("Pinned GLES selection mismatch: " + relative)
    content = content[:start] + "ifeq ($(USE_GLES2),true)\nUI_TYPE = gles2\nendif\n\n" + content[start:]
    if relative == "Makefile":
        content = content.replace("else\nUI_TYPE = opengl\nendif", "else\nUI_TYPE ?= opengl\nendif", 1)
    else:
        seam = "include ../../dpf/Makefile.plugins.mk"
        if content.count(seam) != 1:
            raise SystemExit("Pinned Cardinal DGL link seam mismatch")
        content = content.replace(seam, """# Carla's dependency file supplies desktop GL before DPF appends its libs.
# Select GLES at this final consumer too, so --as-needed cannot bind drawing
# against desktop GL and discard the native GLES dependency.
ifeq ($(USE_GLES2),true)
DGL_LIBS = $(shell $(PKG_CONFIG) --libs glesv2 glx x11 xcursor xext xrandr)
endif
include ../../dpf/Makefile.plugins.mk""")
    path.write_text(content)
print("Prepared canonical Cardinal Window for owned offscreen GLES; native binding remains to be qualified")
