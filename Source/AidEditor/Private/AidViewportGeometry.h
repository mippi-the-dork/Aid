#pragma once

#include "CoreMinimal.h"

/** Stateless viewport geometry used by the Aid HUD and by editor automation tests. */
namespace AidViewportGeometry
{
    inline bool ClipToViewport(FVector2D& A, FVector2D& B, const FIntPoint& Size)
    {
        const FVector2D Delta = B - A;
        float Enter = 0.f, Exit = 1.f;
        const float P[4] = { -Delta.X, Delta.X, -Delta.Y, Delta.Y };
        const float Q[4] = { A.X, static_cast<float>(Size.X) - A.X, A.Y, static_cast<float>(Size.Y) - A.Y };
        for (int32 I = 0; I < 4; ++I)
        {
            if (FMath::IsNearlyZero(P[I])) { if (Q[I] < 0.f) return false; continue; }
            const float T = Q[I] / P[I];
            if (P[I] < 0.f) Enter = FMath::Max(Enter, T);
            else Exit = FMath::Min(Exit, T);
            if (Enter > Exit) return false;
        }
        B = A + Delta * Exit;
        A += Delta * Enter;
        return FVector2D::DistSquared(A, B) > 0.5f;
    }

}
