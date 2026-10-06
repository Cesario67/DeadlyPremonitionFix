#pragma once

namespace dpsf::graphics {

// Reconnaît une matrice de projection Direct3D classique (celles de D3DXMatrixPerspectiveFovLH/RH et
// D3DXMatrixOrthoLH, 16 flottants rangés ligne par ligne comme D3DMATRIX) et en déduit ses plans proche et
// lointain. Fonction pure : testée par tests/unit, utilisée par le diagnostic de rendu (RenderDiagnostics).
//
// Une constante de vertex shader contient souvent la matrice transposée : on essaie donc aussi la
// transposée. Une matrice « monde * vue * projection » combinée ne ressemble à aucune forme connue
// et n'est pas reconnue (resultat None).
enum class ProjectionKind {
    None,
    PerspectiveLH,  // D3DXMatrixPerspectiveFovLH : [2][3] = 1
    PerspectiveRH,  // D3DXMatrixPerspectiveFovRH : [2][3] = -1
    OrthoLH,        // D3DXMatrixOrthoLH : [3][3] = 1
};

struct ProjectionInfo {
    ProjectionKind kind = ProjectionKind::None;
    bool transposed = false;  // vrai si la forme n'apparait qu'après transposition
    float nearPlane = 0.0f;
    float farPlane = 0.0f;
    float fovY = 0.0f;    // en degrés, perspective seulement
    float aspect = 0.0f;  // largeur / hauteur, perspective seulement
};

ProjectionInfo AnalyzeProjection(const float (&matrix)[16]) noexcept;

const char* ProjectionKindName(ProjectionKind kind) noexcept;

}  // namespace dpsf::graphics
