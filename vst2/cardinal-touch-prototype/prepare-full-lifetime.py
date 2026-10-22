#!/usr/bin/env python3
"""Repair catalog/channel ownership in the pinned full Cardinal source closure.

The catalog still belongs to Cardinal's shared Initializer; module channel
engines belong to their concrete Bogaudio module. Graph detachment stays locked;
widget and module destruction retains each existing owner outside that lock.
All seams are checked before writing. Reapplying the exact repair is harmless.
"""
import argparse
from pathlib import Path
import re


def prepare(root):
    pending = {}

    def replace(relative, before, after):
        path = root / relative
        text = pending.get(path, path.read_text())
        if text.count(after) == 1:
            return
        if text.count(before) != 1:
            raise SystemExit("Pinned lifetime seam mismatch: " + relative)
        pending[path] = text.replace(before, after)

    # The base destructor cannot dispatch to these derived removeChannel()
    # implementations. Clear active channels while their types still exist.
    # Arp/Assign also override removeChannel but own no channel allocations.
    classes = """AD ADSR ASR Additator CVD Chirp Clpr CmpDist DADSRH
        DADSRHPlus DGate EQ EQS EightFO FFB FMOp Follow FourFO LFO LLPG LPG
        LVCF Lmtr MegaGate Mix1 Mix2 Nsgt PEQ PEQ14 PEQ14XF PEQ14XR PEQ14XV
        PEQ6 PEQ6XF Pressor RGate Shaper ShaperPlus VCF Vish XCO VCOBase""".split()
    for cls in classes:
        stem = "vco_base" if cls == "VCOBase" else cls
        relative = f"plugins/BogaudioModules/src/{stem}.hpp"
        field = "_core" if cls in ("DADSRH", "DADSRHPlus", "Shaper", "ShaperPlus") else "_engines"
        before = "\tvoid removeChannel(int c) override;"
        after = (f"\t~{cls}() override {{\n"
                 "\t\twhile (_channels > 0) removeChannel(--_channels);\n"
                 "\t}\n\n" + before)
        path = root / relative
        text = path.read_text()
        if after in text:
            continue
        if f"~{cls}(" in text or f"{field}[maxChannels]" not in text:
            raise SystemExit("Pinned channel owner mismatch: " + relative)
        body = f"void {cls}::removeChannel(int c) {{\n\tdelete {field}[c];\n\t{field}[c] = NULL;\n}}"
        if body not in path.with_suffix(".cpp").read_text():
            raise SystemExit("Pinned channel cleanup mismatch: " + relative)
        replace(relative, before, after)

    # DHE is the exception to Rack's permanent-model convention: it allocated
    # fresh metadata on every init. Keep one model at each existing factory site,
    # as the rest of the catalog does. Plugin destruction clears its backlink.
    factories = 0
    for path in sorted((root / "plugins/DHE-Modules/src/modules").glob("*/init.cpp")):
        text = path.read_text()
        pattern = r'plugin->addModel\(\s*(rack::createModel<.*?>\(\s*"([^"]+)"\))\);'
        matches = list(re.finditer(pattern, text, re.S))
        if not matches:
            factories += len(re.findall(r"static auto\* const model\w+ = rack::createModel<", text))
            continue
        factories += len(matches)

        def factory(match):
            expression, slug = match.groups()
            if not slug.isidentifier():
                raise SystemExit("Pinned DHE model slug mismatch: " + slug)
            return f"static auto* const model{slug} = {expression};\n  plugin->addModel(model{slug});"

        pending[path] = re.sub(pattern, factory, text, flags=re.S)
    if factories != 28:
        raise SystemExit(f"Pinned DHE catalog mismatch: {factories} factories")

    # WSTD's process-wide manager appended all 96 samples on every catalog
    # initialization. Its last catalog owner now releases every sample and the
    # manager, after the final engine has closed. Other live owners share the
    # Initializer and therefore retain their samples until their own teardown.
    relative = "plugins/WSTD-Drums/src/model/SampleManager.hpp"
    replace(relative, "  static SampleManager *getInstance( );",
            "  static SampleManager *getInstance( );\n  static void destroyInstance( );")
    replace(relative, "  SampleManager( );", "  SampleManager( );\n  ~SampleManager( );")
    relative = "plugins/WSTD-Drums/src/model/SampleManager.cpp"
    replace(relative, "  SampleManager::SampleManager ( ) { }", """  SampleManager::SampleManager ( ) { }

  SampleManager::~SampleManager ( ) {
    for (Sample *sample : samples) {
      delete[] sample->values;
      delete sample;
    }
  }

  void SampleManager::destroyInstance ( ) {
    delete m_instanceSingleton;
    m_instanceSingleton = nullptr;
  }""")
    relative = "plugins/WSTD-Drums/src/WSTD_Drums.cpp"
    replace(relative, """  WSTD_Drums::SampleManager *sm = {0};
  sampleManager = sm->getInstance();""", "  sampleManager = WSTD_Drums::SampleManager::getInstance();")
    replace(relative, "void setupSamples ( ) {", """void destroySamples() {
  WSTD_Drums::SampleManager::destroyInstance();
  sampleManager = nullptr;
}

void setupSamples ( ) {""")
    relative = "plugins/plugins.cpp"
    replace(relative, "void setupSamples();", "void setupSamples();\nvoid destroySamples();")
    replace(relative, "void destroyStaticPlugins()\n{", "void destroyStaticPlugins()\n{\n    destroySamples();")

    # Cardinal's global model pointers outlive individual Initializers, but
    # cannot outlive their shared library. Register in the common concrete
    # model base so even load-only DSO cycles have the correct final owner.
    # The destructor unregisters models also deleted by manifest filtering.
    replace("include/helpers.hpp", "struct CardinalPluginModelHelper : plugin::Model {",
            "struct CardinalPluginModelHelper : plugin::Model {\n"
            "    CardinalPluginModelHelper();\n"
            "    ~CardinalPluginModelHelper() override;")
    relative = "src/override/Model.cpp"
    replace(relative, "#include <algorithm>", "#include <algorithm>\n#include <mutex>\n#include <vector>")
    replace(relative, "namespace rack {\nnamespace plugin {", """namespace rack {

namespace {
// The registry is initialized by the first model construction. Its final
// destruction follows later static owners, including the shared Initializer.
struct CardinalModelLifetime {
    std::mutex mutex;
    std::vector<CardinalPluginModelHelper*> models;

    ~CardinalModelLifetime() {
        while (!models.empty()) {
            CardinalPluginModelHelper* const model = models.back();
            models.pop_back();
            DISTRHO_SAFE_ASSERT(model->plugin == nullptr);
            delete model;
        }
    }
};

CardinalModelLifetime& cardinalModelLifetime() {
    static CardinalModelLifetime lifetime;
    return lifetime;
}
} // namespace

CardinalPluginModelHelper::CardinalPluginModelHelper() {
    CardinalModelLifetime& lifetime = cardinalModelLifetime();
    const std::lock_guard<std::mutex> guard(lifetime.mutex);
    lifetime.models.push_back(this);
}

CardinalPluginModelHelper::~CardinalPluginModelHelper() {
    CardinalModelLifetime& lifetime = cardinalModelLifetime();
    const std::lock_guard<std::mutex> guard(lifetime.mutex);
    const auto it = std::find(lifetime.models.begin(), lifetime.models.end(), this);
    if (it != lifetime.models.end()) lifetime.models.erase(it);
}

namespace plugin {""")

    # Sapphire's expander-role list shares the catalog's library lifetime.
    # Nodes borrow Model pointers; deleting metadata never deletes or reads a
    # Model. Keep the list across individual Initializer/instance lifetimes.
    replace("plugins/Sapphire/src/sapphire_vcv.cpp",
            "    ModelInfo *ModelInfo::front;",
            """    ModelInfo *ModelInfo::front;

    namespace {
        struct ModelInfoLifetime
        {
            ~ModelInfoLifetime()
            {
                while (ModelInfo *info = ModelInfo::front)
                {
                    ModelInfo::front = info->next;
                    delete info;
                }
            }
        } modelInfoLifetime;
    }
""")

    # An engine-loaded widget can remain unadopted (for example, a negative
    # imported ID). Preserve module pointers through derived/child destruction,
    # but leave final module deletion with the existing engine/load caller.
    replace('src/Rack/include/app/ModuleWidget.hpp',
            """	Transfers ownership to `this`.
	*/
	void setModule(engine::Module* module);

	widget::Widget* getPanel();
	/** Sets the panel and sets the size of the ModuleWidget from the panel.
""",
            """	Transfers ownership to `this`.
	*/
	void setModule(engine::Module* module);
	/** Relinquishes module ownership immediately before deleting this widget.
	The module pointer stays valid for derived and child destructors. The caller
	must keep the module alive until widget destruction completes and delete it.
	This widget must not be reused after this call.
	*/
	void releaseModuleOwnership();

	widget::Widget* getPanel();
	/** Sets the panel and sets the size of the ModuleWidget from the panel.
""")
    replace('src/override/ModuleWidget.cpp',
            """	bool dragEnabled = true;

	widget::Widget* panel = NULL;
};


""",
            """	bool dragEnabled = true;

	widget::Widget* panel = NULL;
	bool ownsModule = true;
};


""")
    replace('src/override/ModuleWidget.cpp',
            """
ModuleWidget::~ModuleWidget() {
	clearChildren();
	setModule(NULL);
	delete internal;
}

plugin::Model* ModuleWidget::getModel() {
""",
            """
ModuleWidget::~ModuleWidget() {
	clearChildren();
	if (internal->ownsModule)
		setModule(NULL);
	else
		module = NULL;
	delete internal;
}

void ModuleWidget::releaseModuleOwnership() {
	internal->ownsModule = false;
}

plugin::Model* ModuleWidget::getModel() {
""")
    replace('include/helpers.hpp',
            """        if (widgets.find(m) == widgets.end())
            return;

        if (widgetNeedsDeletion[m])
            delete widgets[m];

        widgets.erase(m);
        widgetNeedsDeletion.erase(m);
""",
            """        if (widgets.find(m) == widgets.end())
            return;

        if (widgetNeedsDeletion[m]) {
            // The engine/load caller retains the module until widget cleanup ends.
            static_cast<app::ModuleWidget*>(widgets[m])->releaseModuleOwnership();
            delete widgets[m];
        }

        widgets.erase(m);
        widgetNeedsDeletion.erase(m);
""")
    replace('src/override/Engine.cpp',
            """}


void Engine::clear() {
	std::lock_guard<SharedMutex> lock(internal->mutex);
	clear_NoLock();
}


void Engine::clear_NoLock() {
	// Copy lists because we'll be removing while iterating
	std::set<ParamHandle*> paramHandles = internal->paramHandles;
	for (ParamHandle* paramHandle : paramHandles) {
		removeParamHandle_NoLock(paramHandle);
		// Don't delete paramHandle because they're normally owned by Module subclasses
	}
	std::vector<Cable*> cables = internal->cables;
	for (Cable* cable : cables) {
		removeCable_NoLock(cable);
		delete cable;
	}
	std::vector<Module*> modules = internal->modules;
	for (Module* module : modules) {
		removeModule_NoLock(module);
		delete module;
	}
	std::vector<TerminalModule*> terminalModules = internal->terminalModules;
	for (TerminalModule* terminalModule : terminalModules) {
		removeModule_NoLock(terminalModule);
		delete terminalModule;
	}
}


""",
            """}


static void Engine_removeCachedModuleWidget(Module* module) {
	CardinalPluginModelHelper* const helper = dynamic_cast<CardinalPluginModelHelper*>(module->model);
	DISTRHO_SAFE_ASSERT_RETURN(helper != nullptr,);
	helper->removeCachedModuleWidget(module);
}


void Engine::clear() {
	std::vector<Module*> removedModules;
	std::set<ParamHandle*> originalParamHandles;
	{
		std::lock_guard<SharedMutex> lock(internal->mutex);
		originalParamHandles = internal->paramHandles;
		clear_NoLock(removedModules);
	}
	// The graph no longer processes these modules. Keep their handles registered
	// until their owning destructors remove them through the public engine API.
	for (Module* module : removedModules) {
		Engine_removeCachedModuleWidget(module);
		delete module;
	}
	// Clear surviving handles from the old graph without deleting their owners.
	std::lock_guard<SharedMutex> lock(internal->mutex);
	for (ParamHandle* paramHandle : originalParamHandles) {
		if (internal->paramHandles.find(paramHandle) != internal->paramHandles.end())
			removeParamHandle_NoLock(paramHandle);
	}
}


void Engine::clear_NoLock(std::vector<Module*>& removedModules) {
	// Preserve ordinary-module then terminal-module destruction order.
	removedModules = internal->modules;
	removedModules.insert(removedModules.end(), internal->terminalModules.begin(), internal->terminalModules.end());
	std::vector<Cable*> cables = internal->cables;
	for (Cable* cable : cables) {
		removeCable_NoLock(cable);
		delete cable;
	}
	for (Module* module : removedModules)
		removeModule_NoLock(module);
}


""")
    replace('src/override/Engine.cpp',
            """

void Engine::removeModule(Module* module) {
	std::lock_guard<SharedMutex> lock(internal->mutex);
	removeModule_NoLock(module);
}


static void removeModule_NoLock_common(Engine::Internal* internal, Module* module) {
	// Remove from widgets cache
	CardinalPluginModelHelper* const helper = dynamic_cast<CardinalPluginModelHelper*>(module->model);
	DISTRHO_SAFE_ASSERT_RETURN(helper != nullptr,);
	helper->removeCachedModuleWidget(module);
	// Dispatch RemoveEvent
	Module::RemoveEvent eRemove;
	module->onRemove(eRemove);
""",
            """

void Engine::removeModule(Module* module) {
	{
		std::lock_guard<SharedMutex> lock(internal->mutex);
		if (!removeModule_NoLock(module))
			return;
	}
	Engine_removeCachedModuleWidget(module);
}


static void removeModule_NoLock_common(Engine::Internal* internal, Module* module) {
	// Dispatch RemoveEvent
	Module::RemoveEvent eRemove;
	module->onRemove(eRemove);
""")
    replace('src/override/Engine.cpp',
            """}


void Engine::removeModule_NoLock(Module* module) {
	DISTRHO_SAFE_ASSERT_RETURN(module,);
	// Check that the module actually exists
	if (TerminalModule* const terminalModule = asTerminalModule(module)) {
		auto tit = std::find(internal->terminalModules.begin(), internal->terminalModules.end(), terminalModule);
		DISTRHO_SAFE_ASSERT_RETURN(tit != internal->terminalModules.end(),);
		removeModule_NoLock_common(internal, module);
		internal->terminalModules.erase(tit);
	}
	else {
		auto it = std::find(internal->modules.begin(), internal->modules.end(), module);
		DISTRHO_SAFE_ASSERT_RETURN(it != internal->modules.end(),);
		removeModule_NoLock_common(internal, module);
		internal->modules.erase(it);
	}
	Engine_refreshRouting(this);
}


""",
            """}


bool Engine::removeModule_NoLock(Module* module) {
	DISTRHO_SAFE_ASSERT_RETURN(module, false);
	// Check that the module actually exists
	if (TerminalModule* const terminalModule = asTerminalModule(module)) {
		auto tit = std::find(internal->terminalModules.begin(), internal->terminalModules.end(), terminalModule);
		DISTRHO_SAFE_ASSERT_RETURN(tit != internal->terminalModules.end(), false);
		removeModule_NoLock_common(internal, module);
		internal->terminalModules.erase(tit);
	}
	else {
		auto it = std::find(internal->modules.begin(), internal->modules.end(), module);
		DISTRHO_SAFE_ASSERT_RETURN(it != internal->modules.end(), false);
		removeModule_NoLock_common(internal, module);
		internal->modules.erase(it);
	}
	Engine_refreshRouting(this);
	return true;
}


""")
    replace('src/Rack/include/engine/Engine.hpp',
            """	Exclusively locks.
	*/
	void clear();
	PRIVATE void clear_NoLock();
	/** Advances the engine by `frames` frames.
	Only call this method from the master module.
	Share-locks. Also locks so only one stepBlock() can be called simultaneously or recursively.
""",
            """	Exclusively locks.
	*/
	void clear();
	/** Detaches the graph under the writer lock; clear() destroys modules after unlocking. */
	PRIVATE void clear_NoLock(std::vector<Module*>& removedModules);
	/** Advances the engine by `frames` frames.
	Only call this method from the master module.
	Share-locks. Also locks so only one stepBlock() can be called simultaneously or recursively.
""")
    replace('src/Rack/include/engine/Engine.hpp',
            """	Exclusively locks.
	*/
	void removeModule(Module* module);
	PRIVATE void removeModule_NoLock(Module* module);
	/** Checks whether a Module is in the rack.
	Share-locks.
	*/
""",
            """	Exclusively locks.
	*/
	void removeModule(Module* module);
	PRIVATE bool removeModule_NoLock(Module* module);
	/** Checks whether a Module is in the rack.
	Share-locks.
	*/
""")

    for path, text in pending.items():
        path.write_text(text)
    print(f"Prepared full catalog lifetime repair: {len(pending)} files, 42 channel owners, 28 DHE models, DSO model and Sapphire metadata cleanup")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    prepare(parser.parse_args().source)
