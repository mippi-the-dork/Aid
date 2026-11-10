#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AidDocument.h"
#include "AidDocumentFactory.h"
#include "AssetDefinition_AidDocument.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAidDedicatedAssetFactoryTest,
    "Aid.Editor.Assets.DedicatedDocumentFactory",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAidDedicatedAssetFactoryTest::RunTest(const FString& Parameters)
{
    UAidDocumentFactory* Factory = NewObject<UAidDocumentFactory>(GetTransientPackage());
    TestTrue(TEXT("Factory creates a dedicated document type"), Factory->SupportedClass == UAidDocument::StaticClass());
    UObject* Asset = Factory->FactoryCreateNew(UAidDocument::StaticClass(), GetTransientPackage(),
        MakeUniqueObjectName(GetTransientPackage(), UAidDocument::StaticClass(), TEXT("TestAidDoc")),
        RF_Transient, nullptr, nullptr);
    UAidDocument* Document = Cast<UAidDocument>(Asset);
    TestNotNull(TEXT("Factory creates AidDocument asset"), Document);
    if (Document)
    {
        TestTrue(TEXT("New document starts valid"), Document->ValidateGraph().IsEmpty());
    }
    UAssetDefinition_AidDocument* Definition = NewObject<UAssetDefinition_AidDocument>(GetTransientPackage());
    TestTrue(TEXT("Aid has dedicated asset identity"), Definition->GetAssetClass().Get() == UAidDocument::StaticClass());
    return true;
}

#endif
