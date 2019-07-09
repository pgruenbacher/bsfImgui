// imgui implementation via a RendererExtension. The imgui rendering logic can
// occure on main thread while the actual frame starting/ending and rendering
// can occure on the core thread. Thread-safety should be achievable since the
// render cmd data is cloned to the core thread by the main thread. Probably
// the main thing to worry about is makign sure the input callbacks are
// handled correctly.

#pragma once

#include "imgui.h"
#include "Renderer/BsRendererExtension.h"

struct ImDrawData;

namespace bs {

/* 
 * Retrieve the default imgui shader. Should be used as
 * part of the initialization of the ImguiRendererExtension
*/
HShader defaultImguiShader();

// Build default texture for fonts and symbols. Should be used as part of the
// initialization of the ImguiRendererExtension
HTexture createDefaultFonts();

/* call demo imgui windows */
void demoImguiUI();

namespace ct {

class ImguiRendererExtension : public RendererExtension {
  Mutex mImguiRenderMutex;
  SPtr<GpuParamBlockBuffer> mBuffer;
  SPtr<VertexDeclaration> mVertexDecl;
  SPtr<GpuParams> mParams;
  SPtr<GraphicsPipelineState> mPipeline;
  HShader mShader;
  HTexture mTexture;
  ImDrawData mCopiedDrawData;

 public:
  ImguiRendererExtension();
  // ... other extension code

  void initialize(const Any& data) override;
  void destroy() override;

  bool check(const ct::Camera& camera) override;

  void render(const ct::Camera& camera) override;

  // must be called in the main thread.
  void syncImDrawDataToCore();

 private:
  void setupRenderState(const ct::Camera& camera, ImDrawData* draw_data,
                        int width, int height);
  void renderDrawData(ImDrawData* draw_data, const ct::Camera& camera);
};

}  // namespace ct
}  // namespace bs