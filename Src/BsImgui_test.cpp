#include "Testing/BsTestSuite.h"

#include "BsImGuizmo.h"
#include "BsImgui.h"
#include "imgui.h"
// #include "imgui_impl_glew.h"
#include "BsApplication.h"
#include "BsCoreApplication.h"
#include "Components/BsCRenderable.h"
#include "CoreThread/BsCoreThread.h"
#include "Material/BsMaterial.h"
#include "Resources/BsBuiltinResources.h"
#include "Scene/BsSceneManager.h"
#include "Scene/BsSceneObject.h"

namespace bs {

class ImguiTestSuite : public bs::Test {};

class DemoUI : public Component {
 public:
  DemoUI(const HSceneObject& parent) : Component(parent) {
    setFlag(ComponentFlag::AlwaysRun, true);
    setName("DemoUI");
  }

  void update() override {
    Component::update();
    demoImguiUI();
  }
};

class DemoEditTransform : public Component {
  // keep a modifiable reference to the scene object.
  HSceneObject mParent;

 public:
  DemoEditTransform(HSceneObject parent) : Component(parent), mParent(parent) {
    setFlag(ComponentFlag::AlwaysRun, true);
    setName("DemoEditTransform");
  }

  void update() override {
    Component::update();
    Transform transform = mParent->getLocalTransform();

    ImGui::Begin("Transform example");
    EditTransform(transform, gSceneManager().getMainCamera());
    ImGui::End();
    ManipulateTransform(transform, gSceneManager().getMainCamera());
    // mParent->setTransform(transform);
    // SO()->setPosition({0,3,0});
    auto& so = SO();
    so->setLocalTransform(transform);
  }
};

HSceneObject addBox() {
  HShader shader =
      gBuiltinResources().getBuiltinShader(BuiltinShader::Standard);
  HMaterial material = Material::create(shader);
  HMesh boxMesh = gBuiltinResources().getMesh(BuiltinMesh::Box);
  HSceneObject boxSO = SceneObject::create("Box");
  HRenderable boxRenderable = boxSO->addComponent<CRenderable>();
  boxRenderable->setMesh(boxMesh);
  boxRenderable->setMaterial(material);
  // boxSO->setPosition(Vector3(0.0f, 0.5f, 0.5f));
  return boxSO;
}

TEST_F(ImguiTestSuite, TestImgui) {  // Setup Dear ImGui context

  addFlyableCamera();

  DynLib* imguiPlugin = nullptr;

  Application::instance().loadPlugin("bsfImgui", &imguiPlugin);

  // makeInterfaceFrame();
  HSceneObject box = addBox();
  box->addComponent<DemoEditTransform>();

  HSceneObject obj = SceneObject::create("UI");
  obj->addComponent<DemoUI>();
  Application::instance().runMainLoop();

  Application::instance().unloadPlugin(imguiPlugin);
}

}  // namespace bs