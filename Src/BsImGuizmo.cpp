 #include "BsPrerequisites.h"
#include "imgui.h"
#include "ImGuizmo.h"
#include "Renderer/BsCamera.h"
#include "Scene/BsTransform.h"
#include "Input/BsInputFwd.h"

namespace bs {


static ImGuizmo::OPERATION mCurrentGizmoOperation{ImGuizmo::ROTATE};
static ImGuizmo::MODE mCurrentGizmoMode{ImGuizmo::WORLD};

void manipulateMatrix(Matrix4& matrix, const Matrix4& proj, const Matrix4& view,
                      ImGuizmo::OPERATION operation, ImGuizmo::MODE mode) {
  // unfortunately there's no easy way to access the private float array of the
  // matrices, but we can safeuly assume as of July 2019 that the matrix4x4 is a
  // float[4][4] so we'll just cast.
  static_assert(sizeof(float[4][4]) == sizeof(Matrix4), "Confirm bs::Matrix4 is float[4][4] size");

  // imguizmo and bsf are tranposed...
  Matrix4 transView = view.transpose();
  Matrix4 transProj = proj.transpose();

  matrix = matrix.transpose();

  // ImGuizmo::SetDrawlist();
  // ImGuizmo::DrawCube((const float*)(&transView), (const float*)(&transProj),
  // (float*)(&matrix));
  ImGuizmo::Manipulate((const float*)(&transView), (const float*)(&transProj),
                       operation, mode, (float*)(&matrix));

  // transpose back
  matrix = matrix.transpose();
}

void manipulateMatrix(Matrix4& matrix, const SPtr<Camera> camera,
                      ImGuizmo::OPERATION operation, ImGuizmo::MODE mode) {
  manipulateMatrix(matrix, camera->getProjectionMatrix(),
                   camera->getViewMatrix(), operation, mode);
}

void ManipulateTransform(Transform& transform, const SPtr<Camera> camera) {
  Matrix4 matrix = transform.getMatrix();

  manipulateMatrix(matrix, camera->getProjectionMatrix(),
                   camera->getViewMatrix(), mCurrentGizmoOperation,
                   mCurrentGizmoMode);

  Vector3 trans, scale;
  Quaternion rotation;
  matrix.decomposition(trans, rotation, scale);

  transform = Transform(trans, rotation, scale);
}

void EditTransform(Transform& transform, const SPtr<Camera> camera) {
  /*----------  HotKeys  ----------*/

  if (ImGui::IsKeyPressed(BC_Z)) mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
  if (ImGui::IsKeyPressed(BC_X)) mCurrentGizmoOperation = ImGuizmo::ROTATE;
  if (ImGui::IsKeyPressed(BC_C))  // r Key
    mCurrentGizmoOperation = ImGuizmo::SCALE;

  /*----------  Operation Checkboxes  ----------*/

  if (ImGui::RadioButton("Translate",
                         mCurrentGizmoOperation == ImGuizmo::TRANSLATE))
    mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
  ImGui::SameLine();
  if (ImGui::RadioButton("Rotate", mCurrentGizmoOperation == ImGuizmo::ROTATE))
    mCurrentGizmoOperation = ImGuizmo::ROTATE;
  ImGui::SameLine();
  if (ImGui::RadioButton("Scale", mCurrentGizmoOperation == ImGuizmo::SCALE))
    mCurrentGizmoOperation = ImGuizmo::SCALE;

  Matrix4 matrix = transform.getMatrix();

  Vector3 trans, scale;
  Quaternion rotation;
  Radian angles[3];
  matrix.decomposition(trans, rotation, scale);
  rotation.toEulerAngles(angles[0], angles[1], angles[2]);
  ImGui::InputFloat3("Trans:", trans.ptr(), 3);
  ImGui::InputFloat3("Rotat:", (float*)(&angles), 3);
  ImGui::InputFloat3("Scale:", scale.ptr(), 3);
  rotation.fromEulerAngles(angles[0], angles[1], angles[2]);

  transform = Transform(trans, rotation, scale);
}

}  // namespace bs