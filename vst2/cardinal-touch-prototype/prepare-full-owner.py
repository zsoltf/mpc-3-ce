#!/usr/bin/env python3
"""Bind the touch API to the existing Cardinal and DPF owners at pinned seams."""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("source", type=Path)
args = parser.parse_args()
root = args.source
kit = Path(__file__).resolve().parent

def edit(path, replacements):
    source = path.read_text()
    for old, new in replacements:
        if source.count(old) != 1:
            raise SystemExit("Pinned source mismatch: " + str(path) + ": " + old[:60])
        source = source.replace(old,new)
    path.write_text(source)

# Only active slots are consumed, under the same predicate that assigns them.
# Preserve the complete mixer arithmetic, CV branches and channel order.
edit(root / "plugins/BogaudioModules/src/matrix_base.cpp", [
    ('\tfloat in[maxN] {};', '\tfloat in[maxN]; // Active slots are assigned before every read.')
])

# A settled VCF gain is the exact fixed point of its positive-delta slew.
# Keep every changing gain, filter call and accumulation in its original order.
edit(root / "plugins/BogaudioModules/src/VCF.cpp", [
    ('\t\tfloat g = _gainSLs[i].next(_gains[i]);',
     '\t\t// A settled nonnegative gain is an exact fixed point of this slew.\n'
     '\t\tfloat g = _gainSLs[i]._last == _gains[i]\n'
     '\t\t\t? (_gainSLs[i]._last = _gains[i]) : _gainSLs[i].next(_gains[i]);')
])

# These immutable, deterministically initialized tables were duplicated in
# every Surge translation unit. C++17 keeps one per linked plugin image.
edit(root / "plugins/surgext/surge/libs/sst/sst-waveshapers/include/sst/waveshapers/WaveshaperTables.h", [
    ('const static WaveshaperTables globalWaveshaperTables;',
     'inline const WaveshaperTables globalWaveshaperTables;')
])

# Cache only the exact float projection of the existing BLEP width. Phase
# narrowing occurs only after modulo UINT32_MAX, whose result and complement
# fit uint32_t without changing their integer value or the DSP arithmetic.
edit(root / "plugins/BogaudioModules/src/dsp/oscillator.hpp", [
    ('\tphase_t _qd = 0;', '\tphase_t _qd = 0;\n\tfloat _qdFloat = 0.0f;')
])
edit(root / "plugins/BogaudioModules/src/dsp/oscillator.cpp", [
    ('\t_qd = q * _delta;', '\t_qd = q * _delta;\n\t_qdFloat = (float)_qd;'),
    ('((phase % cyclePhase) / (float)cyclePhase)', '((uint32_t)(phase % cyclePhase) / (float)cyclePhase)'),
    ('float i = (cyclePhase - phase) / (float)_qd;', 'float i = (uint32_t)(cyclePhase - phase) / _qdFloat;'),
    ('float i = phase / (float)_qd;', 'float i = (uint32_t)phase / _qdFloat;')
])

edit(root / "src/CardinalPlugin.cpp", [
    ('#include "CardinalPluginContext.hpp"', '#include "CardinalPluginContext.hpp"\n#include "full-touch-owner.hpp"'),
    ('struct ScopedContext {\n    ScopedContext(const CardinalBasePlugin* const plugin)\n    {',
     'struct ScopedContext {\n    rack::Context* previous;\n    ScopedContext(const CardinalBasePlugin* const plugin)\n        : previous(rack::mpcContextPeek())\n    {'),
    ('    ~ScopedContext()\n    {\n        rack::contextSet(nullptr);', '    ~ScopedContext()\n    {\n        rack::contextSet(previous);'),
    ('            context->window = new rack::window::Window;', '            context->window = new rack::window::Window;\n        const MpcTouchGraphics initialGraphics(context->window);'),
    ('        fInitializer->remotePluginInstance = this;\n       #endif\n    }', '        fInitializer->remotePluginInstance = this;\n       #endif\n        if (!isDummyInstance()) mpcTouchRegister(this, context, DISTRHO_PLUGIN_NAME);\n    }'),
    ('    ~CardinalPlugin() override\n    {', '    ~CardinalPlugin() override\n    {\n        mpcTouchRetire(this);'),
    ('            context->patch->clear();', '            { const MpcTouchGraphics closingGraphics(context->window); context->patch->clear(); }'),
    ('    String getState(const char* const key) const override\n    {', '    String getState(const char* const key) const override\n    {\n        const std::lock_guard<std::recursive_mutex> touchGuard(mpcTouchMutex());\n        const MpcTouchGraphics graphics(context->window);'),
    ('    void setState(const char* const key, const char* const value) override\n    {', '    void setState(const char* const key, const char* const value) override\n    {\n        const std::lock_guard<std::recursive_mutex> touchGuard(mpcTouchMutex());\n        const MpcTouchGraphics graphics(context->window);'),
])
edit(root / "src/override/context.cpp", [
    ('static thread_local Context* threadContext = NULL;',
     'static thread_local Context* threadContext = NULL;\n\n// Scope bookkeeping may observe no selected owner; APP still asserts.\nContext* mpcContextPeek() { return threadContext; }'),
])
edit(root / "src/CardinalRemote.cpp", [
    ('    DISTRHO_SAFE_ASSERT_RETURN(ui != nullptr, nullptr);',
     '    if (ui == nullptr) return nullptr; // Native view exposes no desktop remote control.'),
])
edit(root / "src/custom/glfw.cpp", [
    ('#include "widget/event.hpp"', '#include "widget/event.hpp"\n#include "system.hpp"'),
    ('    DISTRHO_SAFE_ASSERT_RETURN(context->tlw != nullptr,);\n\n    context->tlw->setCursor',
     '    if (context->tlw == nullptr) return; // Touchscreen view has no desktop cursor.\n\n    context->tlw->setCursor'),
    ('    DISTRHO_SAFE_ASSERT_RETURN(context->tlw != nullptr, 0.0);\n\n    return context->tlw->getApp().getTime();',
     '    // The offscreen view uses this existing Rack monotonic time source too.\n    // Reading it advances no engine clock and creates no scheduler or timer.\n    if (context->tlw == nullptr) return rack::system::getTime();\n\n    return context->tlw->getApp().getTime();'),
])
edit(root / "src/Makefile.cardinal.mk", [
    ('FILES_DSP  = CardinalPlugin.cpp', 'FILES_DSP  = CardinalPlugin.cpp\nFILES_DSP += full-touch-owner.cc'),
    ('EXTRA_DSP_LIBS = $(RACK_EXTRA_LIBS) $(CARLA_EXTRA_LIBS) $(STATIC_CARLA_PLUGIN_LIBS)', 'EXTRA_DSP_LIBS = $(RACK_EXTRA_LIBS) $(CARLA_EXTRA_LIBS) $(STATIC_CARLA_PLUGIN_LIBS) -lEGL -lGLESv2 -lgbm'),
])
edit(root / "dpf/distrho/src/DistrhoPluginVST2.cpp", [
    ('#include "xaymar-vst2/vst.h"', '#include "xaymar-vst2/vst.h"\n#include "../../../src/full-touch-api.h"'),
    ('    effect->flags |= 1 << 0;', '    // The MPC native page owns presentation; never advertise an X11 editor.'),
    ('        case VST_EFFECT_OPCODE_WINDOW_CREATE:\n', '        case VST_EFFECT_OPCODE_WINDOW_CREATE:\n            return 0; // Native MPC image page, not an X11 custom-editor handle.\n'),
    ('    ~PluginVst()\n', '    void* mpcTouchInstance() { return fPlugin.getInstancePointer(); }\n' + (kit / 'full-host-parameters.inc').read_text() + '\n    ~PluginVst()\n'),
    ('static intptr_t VST_FUNCTION_INTERFACE vst_dispatcherCallback(', 'extern "C" bool mpcTouchDeferDestroy(void*, void*, void (*)(void*));\nextern "C" void mpcTouchBindEffect(void*, void*, int (*)(void*,uint32_t,uint32_t,MpcEditorParameter*));\n\nstatic intptr_t VST_FUNCTION_INTERFACE vst_dispatcherCallback('),
    ('            exteffect->pluginPtr = new PluginVst(audioMaster, effect);',
     '            exteffect->pluginPtr = new PluginVst(audioMaster, effect);\n            mpcTouchBindEffect(exteffect->pluginPtr->mpcTouchInstance(), effect,\n                [](void* object,uint32_t index,uint32_t event,MpcEditorParameter* out) {\n                    return getEffectPlugin(static_cast<vst_effect*>(object))->mpcTouchHostParameter(index,event,out);\n                });'),
    ('            // delete plugin object\n', '''            // Defer the whole wrapper/effect when close re-enters an active view call.
            if (exteffect->pluginPtr && mpcTouchDeferDestroy(exteffect->pluginPtr->mpcTouchInstance(), effect,
                [](void* object) { vst_dispatcherCallback(static_cast<vst_effect*>(object),
                    VST_EFFECT_OPCODE_DESTROY, 0, 0, nullptr, 0); })) return 1;
            // delete plugin object
'''),
])
edit(root / "dpf/utils/symbols/vst2.version", [
    ('global: VSTPluginMain; main;', 'global: VSTPluginMain; main; mpclearn_cardinal_touch_api;\n  mpclearn_cardinal_overlay_api;'),
])
for filename in ("full-touch-owner.cc", "full-touch-owner.hpp", "full-touch-api.h", "full-touch-interaction.inc"):
    shutil.copyfile(kit / filename, root / "src" / filename)
shutil.copyfile(kit.parent / "pd-rack-prototype/editor-view.h", root / "src/editor-view.h")
# Expose the upstream browser's complete ModuleAdd transaction. The native
# palette supplies metadata and a drop point; Rack retains creation/history.
edit(root / "src/Rack/src/app/Browser.cpp", [
    ('static ModuleWidget* chooseModel(plugin::Model* model) {',
     'ModuleWidget* chooseModel(plugin::Model* model) {'),
])
# Touch Edit selects a module on contact. A single module must retain Rack's
# ordinary squeeze placement; only actual groups need the collision-preserving
# selection move. Rack still owns positions, neighbor movement and history.
edit(root / "src/override/ModuleWidget.cpp", [
    ('if (APP->scene->rack->isSelected(this)) {\n\t\t\t\tpos =',
     'if (APP->scene->rack->isSelected(this) && APP->scene->rack->getSelected().size() > 1) {\n\t\t\t\tpos ='),
])
for variant in ("CardinalSynth", "CardinalFX"):
    target = root / "src" / variant / "full-touch-owner.cc"
    target.symlink_to("../full-touch-owner.cc")
engine = root / "src/override/Engine.cpp"
edit(engine, [
    ('#include <algorithm>', '#include <algorithm>\n#include "full-voltage-copy.hpp"'),
    ('static void Cable_step(Cable* that) {\n\tOutput* output = &that->outputModule->outputs[that->outputId];',
     'static void Cable_step(Cable* that, const Output* output) {'),
    ('''\t// Copy all voltages from output to input
\tfor (int c = 0; c < channels; c++) {
\t\tif (!std::isfinite(output->voltages[c]))
\t\t\t__builtin_unreachable();
\t\tinput->voltages[c] = output->voltages[c];
\t}
\t// Set higher channel voltages to 0
\tfor (int c = channels; c < input->channels; c++) {
\t\tinput->voltages[c] = 0.f;
\t}''', '''\t// Preserve active bits and old-channel clearing without a branch per float.
\tmpclearn_cardinal::copyVoltages(input->voltages, output->voltages, channels, input->channels);'''),
])
content = engine.read_text()
if content.count('Cable_step(cable);') != 2:
    raise SystemExit('Pinned engine cable caller mismatch')
engine.write_text(content.replace('Cable_step(cable);', 'Cable_step(cable, &output);'))
# Every module in a sample receives the same frame. Share only the original
# port-light predicate; all port updates and their ordering remain unchanged.
edit(engine, [
    ('const Module::ProcessArgs& args, bool input) {',
     'const Module::ProcessArgs& args, bool input, bool stepPorts) {'),
    ('static void Module__doProcess(Module* const module, const Module::ProcessArgs& args) {',
     'static void Module__doProcess(Module* const module, const Module::ProcessArgs& args, bool stepPorts) {'),
    ('\tprocessArgs.frame = internal->frame;',
     '\tprocessArgs.frame = internal->frame;\n\t// The same immutable frame is passed to every module in this sample.\n\tconst bool stepPorts = processArgs.frame % PORT_DIVIDER == 0;'),
    ('TerminalModule__doProcess(terminalModule, processArgs, true);',
     'TerminalModule__doProcess(terminalModule, processArgs, true, stepPorts);'),
    ('Module__doProcess(module, processArgs);',
     'Module__doProcess(module, processArgs, stepPorts);'),
    ('TerminalModule__doProcess(terminalModule, processArgs, false);',
     'TerminalModule__doProcess(terminalModule, processArgs, false, stepPorts);'),
])
content = engine.read_text()
if content.count('if (args.frame % PORT_DIVIDER == 0) {') != 2:
    raise SystemExit('Pinned engine port predicate mismatch')
engine.write_text(content.replace('if (args.frame % PORT_DIVIDER == 0) {', 'if (stepPorts) {'))
# Retain only ordered graph-derived traversal under the existing engine lock.
# Rebuild on canonical graph mutations; voltages, channels and DSP stay live.
# Module::config fixes both port vectors in the constructor. Retain the input
# address beside the output address at the same graph-mutation boundary.
edit(engine, [
    ('static void Cable_step(Cable* that, const Output* output) {\n\tInput* input = &that->inputModule->inputs[that->inputId];',
     'static void Cable_step(Input* input, const Output* output) {'),
    (r'''static constexpr const float METER_TIME = 1.f;


struct Engine::Internal {
	std::vector<Module*> modules;
	std::vector<TerminalModule*> terminalModules;
	std::vector<Cable*> cables;
	std::set<ParamHandle*> paramHandles;

	// moduleId
''', r'''static constexpr const float METER_TIME = 1.f;


struct ModuleRouting {
	Module* module;
	std::vector<std::pair<Input*, const Output*>> cables;
	explicit ModuleRouting(Module* module) : module(module) {}
};


struct Engine::Internal {
	std::vector<Module*> modules;
	std::vector<TerminalModule*> terminalModules;
	std::vector<Cable*> cables;
	// Derived traversal only. Graph mutation rebuilds it under the engine lock.
	std::vector<ModuleRouting> moduleRouting;
	std::vector<ModuleRouting> terminalRouting;
	std::set<ParamHandle*> paramHandles;

	// moduleId
'''),
    (r'''};


static void Engine_updateExpander_NoLock(Engine* that, Module* module, bool side) {
	Module::Expander& expander = side ? module->rightExpander : module->leftExpander;
	Module* oldExpanderModule = expander.module;
''', r'''};


static void Engine_appendRouting(std::vector<ModuleRouting>& routing, Module* module) {
	routing.emplace_back(module);
	ModuleRouting& entry = routing.back();
	for (Output& output : module->outputs) {
		for (Cable* cable : output.cables)
			entry.cables.emplace_back(&cable->inputModule->inputs[cable->inputId], &output);
	}
}


static void Engine_refreshRouting(Engine* that) {
	Engine::Internal* internal = that->internal;
	internal->moduleRouting.clear();
	internal->moduleRouting.reserve(internal->modules.size());
	for (Module* module : internal->modules)
		Engine_appendRouting(internal->moduleRouting, module);
	internal->terminalRouting.clear();
	internal->terminalRouting.reserve(internal->terminalModules.size());
	for (TerminalModule* module : internal->terminalModules)
		Engine_appendRouting(internal->terminalRouting, module);
}


static void Engine_updateExpander_NoLock(Engine* that, Module* module, bool side) {
	Module::Expander& expander = side ? module->rightExpander : module->leftExpander;
	Module* oldExpanderModule = expander.module;
'''),
    (r'''#endif


static void TerminalModule__doProcess(TerminalModule* const terminalModule, const Module::ProcessArgs& args, bool input, bool stepPorts) {
	// Step module
	if (input) {
		terminalModule->processTerminalInput(args);
		for (Output& output : terminalModule->outputs) {
			for (Cable* cable : output.cables)
				Cable_step(cable, &output);
		}
	} else {
		terminalModule->processTerminalOutput(args);
	}
''', r'''#endif


static void TerminalModule__doProcess(TerminalModule* const terminalModule, const Module::ProcessArgs& args, bool input, bool stepPorts, const ModuleRouting& routing) {
	// Step module
	if (input) {
		terminalModule->processTerminalInput(args);
		for (const auto& cable : routing.cables)
			Cable_step(cable.first, cable.second);
	} else {
		terminalModule->processTerminalOutput(args);
	}
'''),
    (r'''	const bool stepPorts = processArgs.frame % PORT_DIVIDER == 0;

	// Process terminal inputs first
	for (TerminalModule* terminalModule : internal->terminalModules) {
		TerminalModule__doProcess(terminalModule, processArgs, true, stepPorts);
	}

	// Step each module and cables
	for (Module* module : internal->modules) {
		Module__doProcess(module, processArgs, stepPorts);
		for (Output& output : module->outputs) {
			for (Cable* cable : output.cables)
				Cable_step(cable, &output);
		}
	}

	// Process terminal outputs last
	for (TerminalModule* terminalModule : internal->terminalModules) {
		TerminalModule__doProcess(terminalModule, processArgs, false, stepPorts);
	}

	++internal->frame;
''', r'''	const bool stepPorts = processArgs.frame % PORT_DIVIDER == 0;

	// Process terminal inputs first
	for (const ModuleRouting& routing : internal->terminalRouting) {
		TerminalModule__doProcess(static_cast<TerminalModule*>(routing.module), processArgs, true, stepPorts, routing);
	}

	// Step each module and cables
	for (const ModuleRouting& routing : internal->moduleRouting) {
		Module__doProcess(routing.module, processArgs, stepPorts);
		for (const auto& cable : routing.cables)
			Cable_step(cable.first, cable.second);
	}

	// Process terminal outputs last
	for (const ModuleRouting& routing : internal->terminalRouting) {
		TerminalModule__doProcess(static_cast<TerminalModule*>(routing.module), processArgs, false, stepPorts, routing);
	}

	++internal->frame;
'''),
    (r'''	}
	// Order the modules according to their connections
	Engine_orderModules(that);
}


''', r'''	}
	// Order the modules according to their connections
	Engine_orderModules(that);
	Engine_refreshRouting(that);
}


'''),
    (r'''#if DEBUG_ORDERED_MODULES
	printf("New module: %s - %ld\n", module->model->getFullName().c_str(), module->id);
#endif
}


''', r'''#if DEBUG_ORDERED_MODULES
	printf("New module: %s - %ld\n", module->model->getFullName().c_str(), module->id);
#endif
	Engine_refreshRouting(this);
}


'''),
    (r'''		removeModule_NoLock_common(internal, module);
		internal->modules.erase(it);
	}
}


''', r'''		removeModule_NoLock_common(internal, module);
		internal->modules.erase(it);
	}
	Engine_refreshRouting(this);
}


'''),
])
shutil.copyfile(kit / "full-voltage-copy.hpp", root / "src/full-voltage-copy.hpp")
subprocess.run([sys.executable, str(kit / "prepare-full-lifetime.py"), str(root)], check=True)
subprocess.run([sys.executable, str(kit / "prepare-full-dispatch.py"), str(root)], check=True)
subprocess.run([sys.executable, str(kit / "prepare-full-waveforms.py"), str(root)], check=True)
subprocess.run([sys.executable, str(kit / "prepare-full-dsp.py"), str(root)], check=True)
subprocess.run([sys.executable, str(kit / "prepare-full-midi.py"), str(root)], check=True)
subprocess.run([sys.executable, str(kit / "prepare-full-usability.py"), str(root)], check=True)
print("Prepared same-plugin view registration, state serialization lock and whole-wrapper deferred close")
