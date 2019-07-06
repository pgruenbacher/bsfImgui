
#include "imgui.h"
#include "BsCorePrerequisites.h"
#include "./BsImgui.h"

namespace bs {

// forward declare
bool initImgui();
void disconnectImgui();

static SPtr<ct::ImguiRendererExtension> gRendererExt;


extern "C" BS_PLUGIN_EXPORT const char* getPluginName()
{
	return "BsImgui";
}

extern "C" BS_PLUGIN_EXPORT void* loadPlugin()
{

	// we want the imgui initialization on the main thead, not the core thread
	// with the renderer extension.
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGui::StyleColorsDark();

	HMaterial imguiMaterial = defaultImguiMaterial();
	gRendererExt = RendererExtension::create<ct::ImguiRendererExtension>(imguiMaterial);
	
	initImgui();

	return nullptr; // Not used
}

extern "C" BS_PLUGIN_EXPORT void updatePlugin()
{
	// Copy the imgui draw data to be available to the core thread.
	gRendererExt->syncImDrawDataToCore();
}

extern "C" BS_PLUGIN_EXPORT void unloadPlugin()
{
	// don't destroy the render (the core thread will handle it)
	// but we do need to reset the static pointer.
	gRendererExt.reset();
	ImGui::EndFrame();
	disconnectImgui();
	ImGui::DestroyContext();
}

};  // namespace bs