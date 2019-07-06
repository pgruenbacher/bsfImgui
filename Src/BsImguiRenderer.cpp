
#include "imgui.h"
#include "ImGuizmo.h"
#include "BsPrerequisites.h"
#include "RenderAPI/BsBlendState.h"
#include "Renderer/BsParamBlocks.h"
#include "Debug/BsBitmapWriter.h"
// #include "Mesh/BsMesh.h"
#include "Renderer/BsCamera.h"
#include "RenderAPI/BsVertexDataDesc.h"
#include "Mesh/BsMeshHeap.h"
// #include "Mesh/BsTransientMesh.h"
#include "Material/BsMaterial.h"
#include "RenderAPI/BsIndexBuffer.h"
#include "RenderAPI/BsVertexBuffer.h"
#include "Importer/BsImporter.h"
#include "Material/BsGpuParamsSet.h"
#include "Material/BsShader.h"
#include "Resources/BsBuiltinResources.h"
#include "FileSystem/BsFileSystem.h"
#include "FileSystem/BsDataStream.h"
#include "Renderer/BsRendererUtility.h"
#include "Renderer/BsRendererExtension.h"
#include "BsEngineConfig.h"

#include "./BsImgui.h"
#include "./BsImGuizmo.h"

namespace bs {

// forward declare
void updateImguiInputs();

HTexture createDefaultFonts()
{
  // Build texture atlas
	// for now let's just use bsf default font.
  ImGuiIO& io = ImGui::GetIO();
  unsigned char* pixels;
  int width, height;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);   
  // Load as RGBA 32-bits (75% of the memory is wasted, but default font is so
  // small) because it is more likely to be compatible with user's existing
  // shaders. If your ImTextureId represent a higher-level concept than just a
  // GL texture id, consider calling GetTexDataAsAlpha8() instead to save on
  // GPU memory.

  if (false) {
  	// debug bmp output of the generated texture.
	  UINT32 bytesPerPixel = 4;
		UINT32 bmpDataSize = BitmapWriter::getBMPSize(width, height, bytesPerPixel);
		UINT8* bmpBuffer = bs_newN<UINT8>(bmpDataSize);
		for (int i = 0; i < width * height; ++i) {
			// convert alph to black for debugging.
			// since otherwise it won't be as easy to see the alpha.
			pixels[i*4] = pixels[i*4 + 3];
			pixels[i*4 + 1] = pixels[i*4 + 3];
			pixels[i*4 + 2] = pixels[i*4 + 3];
		}
		BitmapWriter::rawPixelsToBMP(pixels, bmpBuffer, width, height, bytesPerPixel);
		SPtr<DataStream> ds = FileSystem::createAndOpenFile("test.bmp");
		ds->write(bmpBuffer, bmpDataSize);
		ds->close();	  	
  }

	TEXTURE_DESC textureDesc;
	textureDesc.format = PF_RGBA8;
	textureDesc.width = width;
	textureDesc.height = height;
	HTexture texture = Texture::create(textureDesc);
	SPtr<PixelData> pixelData = PixelData::create(width, height, 1, PF_RGBA8);
	Vector<Color> colors;
	for (int i = 0; i < width * height; ++i) {
		colors.push_back(Color(1,1,1, pixels[i*4 + 3] / 255.f));
	}
	pixelData->setColors(colors);
	texture->writeData(pixelData);

  return texture;
}

HMaterial defaultImguiMaterial() {
	// assume that the plugin is a submodule of the bsf framework under
	// bsf/Source/Plugins/bsfImgui a better way to access the data will likely
	// come up as more plugins are adopted in the framework.
	Path imguipath = Path(RAW_APP_ROOT) + Path("Source/Plugins/bsfImgui/Data/Shader/imgui.bsl");
	// originally the plugin was using a shader in the builtin shaders directory.
	// Path imguiPath = BuiltinResources::instance().getRawShaderFolder().append("Imgui.bsl");
	HShader shader = gImporter().import<Shader>(imguiPath);
	HMaterial material = Material::create(shader);
	HTexture texture = createDefaultFonts();
	material->setTexture("gMainTexture", texture);
	return material;
}

}  // namespace bs

namespace bs::ct {

BS_PARAM_BLOCK_BEGIN(ImguiParamBlockDef)
BS_PARAM_BLOCK_ENTRY(float, gInvViewportWidth)
BS_PARAM_BLOCK_ENTRY(float, gInvViewportHeight)
BS_PARAM_BLOCK_ENTRY(float, gViewportYFlip)
BS_PARAM_BLOCK_END

ImguiParamBlockDef gImguiParamBlockDef;

const UINT32 ImguiRenderPriority = 10;

ImguiRendererExtension::ImguiRendererExtension()
    : RendererExtension(RenderLocation::Overlay, ImguiRenderPriority) {
  auto vertexDeclDesc = bs_shared_ptr_new<VertexDataDesc>();
  vertexDeclDesc->addVertElem(VET_FLOAT2, VES_POSITION);
  vertexDeclDesc->addVertElem(VET_FLOAT2, VES_TEXCOORD);
  vertexDeclDesc->addVertElem(VET_COLOR, VES_COLOR);
  gVertexDecl = VertexDeclaration::create(vertexDeclDesc);
  assert(vertexDeclDesc->getVertexStride() == sizeof(ImDrawVert));
}
// ... other extension code

void ImguiRendererExtension::initialize(const Any& data) {
  gMaterial = any_cast<HMaterial>(data);
}

void ImguiRendererExtension::destroy() {
}

bool ImguiRendererExtension::check(const ct::Camera& camera) { return true; }

void ImguiRendererExtension::render(const ct::Camera& camera) {

  assert(gMaterial.isLoaded());
  // lock the renderer so that when we are writing out the draw data, it is
  // not attempting to render at the same time.
  mImguiRenderMutex.lock();
  if (mCopiedDrawData.Valid) {
    // renderDrawData(ImGui::GetDrawData(), camera);
    renderDrawData(&mCopiedDrawData, camera);
  }
  mImguiRenderMutex.unlock();
}

// must be called in the main thread. the plugin update method should handle
// it. we need to copy the cmd list data because the newFrame and imgui begin
// methods will clear the data otherwise
void ImguiRendererExtension::syncImDrawDataToCore() {
  mImguiRenderMutex.lock();
  // lock the renderer so that when we are writing out the draw data, it is
  // not attempting to render at the same time.
  ImGui::Render();
  // we clone the cmd list data for each cmd so that when the next frames are
  // resetting the data we can still use the current data.
  if (ImGui::GetDrawData()) {
    mCopiedDrawData = *ImGui::GetDrawData();
    for (int n = 0; n < mCopiedDrawData.CmdListsCount; n++) {
      mCopiedDrawData.CmdLists[n] = mCopiedDrawData.CmdLists[n]->CloneOutput();
    }
  }
  mImguiRenderMutex.unlock();
  updateImguiInputs();
  ImGui::NewFrame();
  ImGuizmo::BeginFrame();
}

// setupRenderState is meant to update the global gpu parameters that will be
// consistent for all of the imguid draw commands.
void ImguiRendererExtension::setupRenderState(const ct::Camera& camera,
                                              ImDrawData* draw_data, int width,
                                              int height) {

  float invViewportWidth =
      1.0f / (camera.getViewport()->getPixelArea().width * 0.5f);
  float invViewportHeight =
      1.0f / (camera.getViewport()->getPixelArea().height * 0.5f);
  float viewflipYFlip =
      (gCaps().conventions.ndcYAxis == Conventions::Axis::Down) ? -1.0f : 1.0f;

  if (!gBuffer) {
    gBuffer = gImguiParamBlockDef.createBuffer();
  }
  gImguiParamBlockDef.gInvViewportWidth.set(gBuffer, invViewportWidth);
  gImguiParamBlockDef.gInvViewportHeight.set(gBuffer, invViewportHeight);
  gImguiParamBlockDef.gViewportYFlip.set(gBuffer, viewflipYFlip);
  gBuffer->flushToGPU();

  UINT32 passIdx = 0;
  UINT32 techniqueIdx = gMaterial->getDefaultTechnique();

  SPtr<GpuParamsSet> paramSet =
      gMaterial->getCore()->createParamsSet(techniqueIdx);
  auto mParamBufferIdx = paramSet->getParamBlockBufferIndex("GUIParams");
  // confirm that invalid param buffer indices are (UINT32-1)
  assert(paramSet->getParamBlockBufferIndex("NoParamsTest") == (UINT32)-1);
  // confirm that buffer index is valid
  assert(mParamBufferIdx != (UINT32)-1);
  gMaterial->getCore()->updateParamsSet(paramSet);
  paramSet->setParamBlockBuffer(mParamBufferIdx, gBuffer, false);
  gRendererUtility().setPass(gMaterial->getCore(), passIdx, techniqueIdx);
  gRendererUtility().setPassParams(paramSet);
}

// given the imgui draw data, render out using the bsf render api immediately.
void ImguiRendererExtension::renderDrawData(ImDrawData* draw_data,
                                            const ct::Camera& camera) {
  // Avoid rendering when minimized, scale coordinates for retina displays
  // (screen coordinates != framebuffer coordinates)
  int fb_width =
      (int)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
  int fb_height =
      (int)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
  if (fb_width == 0 || fb_height == 0) return;

  setupRenderState(camera, draw_data, fb_width, fb_height);

  auto& renderAPI = RenderAPI::instance();

  // Will project scissor/clipping rectangles into framebuffer space
  // (0,0) unless using multi-viewports
  ImVec2 clip_off =  draw_data->DisplayPos;  
  // (1,1) unless using retina
	// display which are often (2,2)
  ImVec2 clip_scale = draw_data->FramebufferScale;  

  for (int n = 0; n < draw_data->CmdListsCount; n++) {
    const ImDrawList* cmd_list = draw_data->CmdLists[n];


    /*----------  Write out vertex and index buffers  ----------*/
    
    VERTEX_BUFFER_DESC desc;
    desc.vertexSize = sizeof(ImDrawVert);
    desc.numVerts = cmd_list->VtxBuffer.Size;
    desc.usage = GBU_STATIC;
    // == 0 seems to happen during multi-threading, we'll just skip.
    if (desc.numVerts == 0) continue;
    SPtr<VertexBuffer> vbuf = VertexBuffer::create(desc);
    vbuf->writeData(0, sizeof(ImDrawVert) * desc.numVerts,
                    cmd_list->VtxBuffer.Data);

    // assert that imgui index type is 16 bit or 2 bytes.
    static_assert(sizeof(ImDrawIdx) == 2);
    INDEX_BUFFER_DESC indexDesc;
    indexDesc.indexType = IT_16BIT;
    indexDesc.numIndices = cmd_list->IdxBuffer.Size;
    // == 0 seems to happen during multi-threading, we'll just skip.
    if (indexDesc.numIndices == 0) continue;
    indexDesc.usage = GBU_STATIC;
    SPtr<IndexBuffer> ibuf = IndexBuffer::create(indexDesc);
    ibuf->writeData(0, sizeof(ImDrawIdx) * indexDesc.numIndices,
                    cmd_list->IdxBuffer.Data);

    /*----------  Bind the vertex and index buffers  ----------*/
    UINT32 numBuffers = 1;
    renderAPI.setVertexBuffers(0, &vbuf, numBuffers);
    renderAPI.setVertexDeclaration(gVertexDecl);
    renderAPI.setIndexBuffer(ibuf);
    renderAPI.setDrawOperation(DOT_TRIANGLE_LIST);

    /*----------  For each command, scissor and draw  ----------*/
    for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
      const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
      if (pcmd->UserCallback) {
        // User callback, registered via ImDrawList::AddCallback()
        // (ImDrawCallback_ResetRenderState is a special callback value used by
        // the user to request the renderer to reset render state.)
        if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
          setupRenderState(camera, draw_data, fb_width, fb_height);
        else
          pcmd->UserCallback(cmd_list, pcmd);
      } else {
        // Project scissor/clipping rectangles into framebuffer space
        ImVec4 clip_rect;
        clip_rect.x = (pcmd->ClipRect.x - clip_off.x) * clip_scale.x;
        clip_rect.y = (pcmd->ClipRect.y - clip_off.y) * clip_scale.y;
        clip_rect.z = (pcmd->ClipRect.z - clip_off.x) * clip_scale.x;
        clip_rect.w = (pcmd->ClipRect.w - clip_off.y) * clip_scale.y;

        if (clip_rect.x < fb_width && clip_rect.y < fb_height &&
            clip_rect.z >= 0.0f && clip_rect.w >= 0.0f) {
          assert((clip_rect.z - clip_rect.x) >= 0);
          assert(((fb_height - clip_rect.w)) >= 0);
          assert(((clip_rect.w - clip_rect.y)) >= 0);

          renderAPI.setScissorRect((int)(clip_rect.x), (int)(clip_rect.y),
                                   (int)(clip_rect.z), (int)(clip_rect.w));

          assert(pcmd->ElemCount % 3 == 0); // should always be triangle indices.
          assert(pcmd->VtxOffset == 0); // should always be zero
          // skip if idx offset doesn't make sense. this may not be an issue
          // anymore now that multi-threading is resolved.
          if (pcmd->IdxOffset >= indexDesc.numIndices) continue;
          assert(pcmd->IdxOffset < indexDesc.numIndices);
          UINT32 instanceCount = 1;
          renderAPI.drawIndexed(pcmd->IdxOffset, pcmd->ElemCount,
                                pcmd->VtxOffset, desc.numVerts, instanceCount);
        }
      }
    }
  }
}

}  // namespace bs::ct