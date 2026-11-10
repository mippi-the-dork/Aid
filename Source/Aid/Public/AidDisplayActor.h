#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AidDisplayActor.generated.h"

class UAidDocument;
class ULineBatchComponent;
class UTextRenderComponent;
class USceneComponent;

/** Optional presentation of the same Aid Document inside PIE/standalone/QA builds. */
UCLASS(Blueprintable, meta=(DisplayName="Aid Display Actor"))
class AID_API AAidDisplayActor : public AActor
{
    GENERATED_BODY()

public:
    AAidDisplayActor();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aid|Data")
    TObjectPtr<UAidDocument> Document;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aid|Visibility")
    bool bShowInPlay = true;

    /** Opt-in only: explicitly allow Aid overlays in shipping builds. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aid|Visibility", AdvancedDisplay)
    bool bAllowInShipping = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aid|Appearance", meta=(ClampMin="10.0", ClampMax="500.0"))
    float TextWorldSize = 36.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aid|Appearance")
    bool bFaceLocalPlayerCamera = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aid|Appearance")
    bool bShowConnectionLines = true;

    /** Reloads runtime display from the document; call after changing asset reference at runtime. */
    UFUNCTION(BlueprintCallable, Category="Aid|Runtime")
    void SetAidDocument(UAidDocument* NewDocument);

    UFUNCTION(BlueprintCallable, Category="Aid|Runtime")
    void RefreshAidDisplay();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
    UPROPERTY(VisibleAnywhere, Category="Aid|Components")
    TObjectPtr<USceneComponent> AidRoot;

    UPROPERTY(VisibleAnywhere, Category="Aid|Components")
    TObjectPtr<ULineBatchComponent> ConnectionBatcher;

private:
    UPROPERTY(Transient)
    TArray<TObjectPtr<UTextRenderComponent>> RuntimeCards;

    UPROPERTY(Transient)
    TArray<uint8> RuntimeCardStates;

    bool ShouldDisplay() const;
    void ClearRuntimeCards();
};
