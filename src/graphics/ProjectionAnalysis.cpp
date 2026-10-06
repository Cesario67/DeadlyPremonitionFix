#include "graphics/ProjectionAnalysis.h"

#include <cmath>
#include <numbers>

namespace dpsf::graphics {

namespace {

constexpr float kEpsilon = 1e-4f;

bool IsZero(float value) noexcept {
    return std::fabs(value) < kEpsilon;
}

bool IsNear(float value, float expected) noexcept {
    return std::fabs(value - expected) < kEpsilon;
}

// Cases de la matrice (indice ligne * 4 + colonne) qui doivent être nulles pour une projection en
// perspective : tout sauf [0][0], [1][1], [2][2], [2][3], [3][2].
bool PerspectiveZeros(const float (&m)[16]) noexcept {
    constexpr int kZeros[] = {1, 2, 3, 4, 6, 7, 8, 9, 12, 13, 15};
    for (const int index : kZeros) {
        if (!IsZero(m[index])) {
            return false;
        }
    }
    return true;
}

ProjectionInfo Analyze(const float (&m)[16]) noexcept {
    ProjectionInfo info;
    if (PerspectiveZeros(m) && !IsZero(m[0]) && !IsZero(m[5]) && !IsZero(m[10])) {
        // D3DXMatrixPerspectiveFovLH : m22 = zf/(zf-zn), m32 = -zn*zf/(zf-zn), m23 = 1.
        if (IsNear(m[11], 1.0f) && !IsNear(m[10], 1.0f)) {
            info.kind = ProjectionKind::PerspectiveLH;
            info.nearPlane = -m[14] / m[10];
            info.farPlane = m[10] * info.nearPlane / (m[10] - 1.0f);
        }
        // D3DXMatrixPerspectiveFovRH : m22 = zf/(zn-zf), m32 = zn*zf/(zn-zf), m23 = -1.
        else if (IsNear(m[11], -1.0f) && !IsNear(m[10], -1.0f)) {
            info.kind = ProjectionKind::PerspectiveRH;
            info.nearPlane = m[14] / m[10];
            info.farPlane = m[14] / (m[10] + 1.0f);
        }
        if (info.kind != ProjectionKind::None) {
            info.fovY = 2.0f * std::atan(1.0f / std::fabs(m[5])) * 180.0f / std::numbers::pi_v<float>;
            info.aspect = std::fabs(m[5] / m[0]);
        }
        return info;
    }
    // D3DXMatrixOrthoLH : m22 = 1/(zf-zn), m32 = -zn/(zf-zn), m33 = 1, le reste nul sauf m00 et m11.
    constexpr int kOrthoZeros[] = {1, 2, 3, 4, 6, 7, 8, 9, 11, 12, 13};
    bool ortho = IsNear(m[15], 1.0f) && !IsZero(m[0]) && !IsZero(m[5]) && !IsZero(m[10]);
    for (const int index : kOrthoZeros) {
        ortho = ortho && IsZero(m[index]);
    }
    if (ortho) {
        info.kind = ProjectionKind::OrthoLH;
        info.nearPlane = -m[14] / m[10];
        info.farPlane = info.nearPlane + 1.0f / m[10];
    }
    return info;
}

}  // namespace

ProjectionInfo AnalyzeProjection(const float (&matrix)[16]) noexcept {
    ProjectionInfo direct = Analyze(matrix);
    if (direct.kind != ProjectionKind::None) {
        return direct;
    }
    float transposed[16];
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            transposed[column * 4 + row] = matrix[row * 4 + column];
        }
    }
    ProjectionInfo result = Analyze(transposed);
    result.transposed = result.kind != ProjectionKind::None;
    return result;
}

const char* ProjectionKindName(ProjectionKind kind) noexcept {
    switch (kind) {
        case ProjectionKind::PerspectiveLH:
            return "perspective LH";
        case ProjectionKind::PerspectiveRH:
            return "perspective RH";
        case ProjectionKind::OrthoLH:
            return "orthographique LH";
        case ProjectionKind::None:
            break;
    }
    return "inconnue";
}

}  // namespace dpsf::graphics
