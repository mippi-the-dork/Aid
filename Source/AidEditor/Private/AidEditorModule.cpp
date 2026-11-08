#include "Modules/ModuleManager.h"
#include "AidBrowser.h"
#include "AidViewportMode.h"
#include "EditorModeRegistry.h"
#include "Framework/Docking/TabManager.h"
#include "ToolMenus.h"
#include "Textures/SlateIcon.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "AidEditor"

class FAidEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        FEditorModeRegistry::Get().RegisterMode<FAidViewportMode>(
            FAidViewportMode::ModeId, LOCTEXT("ModeName", "Aid Overlay"), FSlateIcon(), false, 7000);

        FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
            TEXT("AidBrowser"), FOnSpawnTab::CreateRaw(this, &FAidEditorModule::SpawnBrowser))
            .SetDisplayName(LOCTEXT("TabName", "Aid Browser"))
            .SetTooltipText(LOCTEXT("TabTooltip", "Spatial design and QA annotation authoring"))
            .SetMenuType(ETabSpawnerMenuType::Hidden);

        UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(
            this, &FAidEditorModule::RegisterMenus));
    }

    virtual void ShutdownModule() override
    {
        if (UToolMenus::IsToolMenuUIEnabled())
        {
            UToolMenus::UnRegisterStartupCallback(this);
            UToolMenus::UnregisterOwner(this);
        }
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TEXT("AidBrowser"));
        FEditorModeRegistry::Get().UnregisterMode(FAidViewportMode::ModeId);
    }

private:
    TSharedRef<SDockTab> SpawnBrowser(const FSpawnTabArgs& Args)
    {
        return SNew(SDockTab)
            .TabRole(ETabRole::NomadTab)
            [ SNew(SAidBrowser) ];
    }

    void RegisterMenus()
    {
        FToolMenuOwnerScoped Owner(this);
        UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
        if (!Menu)
        {
            return;
        }
        FToolMenuSection& Section = Menu->FindOrAddSection(TEXT("Aid"));
        Section.AddMenuEntry(
            TEXT("AidBrowserOpen"),
            LOCTEXT("OpenBrowser", "Aid Browser"),
            LOCTEXT("OpenBrowserTip", "Open the Aid spatial annotation panel"),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateLambda([]()
            {
                FGlobalTabmanager::Get()->TryInvokeTab(FTabId(TEXT("AidBrowser")));
            })));
    }
};

IMPLEMENT_MODULE(FAidEditorModule, AidEditor)

#undef LOCTEXT_NAMESPACE
