#include "LocalBrowser.h"

#include "gui/Button.h"
#include "gui/IconTextBox.h"
#include "gui/IconTextBox.h"
#include "gui/Icons.h"
#include "gui/SDLWindow.h"
#include "gui/Dragger.h"
#include "gui/DropDown.h"
#include "gui/Appearance.h"
#include "gui/Pagination.h"
#include "gui/Spinner.h"
#include "common/Platform.h"
#include "common/Worker.h"
#include "common/tpt-compat.h"
#include "language/Language.h"
#include "graphics/ThumbnailRenderer.h"
#include "simulation/ElementGraphics.h"
#include "activities/game/Game.h"
#include "activities/dialog/Confirm.h"
#include "simulation/GameSave.h"
#include "Button.h"
#include "Metrics.h"

#include <iostream>

namespace activities::browser
{
	class SearchSavesTask : public common::WorkerTask
	{
	public:
		struct SaveInfo
		{
			Path path;
			String displayName;
			std::shared_ptr<GameSave> save;
			gui::Point saveDataSizeB;
		};

	private:
		Path path;
		int page;

		void Process()
		{
			auto extension = String(".cps");
			std::vector<SaveInfo> unfiltered;
			for (auto &name : Platform::DirectorySearch(String(path).ToUtf8(), "", {}))
			{
				if (name == "." || name == "..")
				{
					continue;
				}
				SaveInfo info;
				info.path = path.Append(name.FromUtf8());
				auto pathStr = String(info.path);
				if (pathStr.EndsWith(extension))
				{
					info.displayName = name.Substr(0, name.size() - extension.size()).FromUtf8();
				}
				else
				{
					continue;
				}
				unfiltered.push_back(info);
			}
			std::sort(unfiltered.begin(), unfiltered.end(), [](const SaveInfo &lhs, const SaveInfo &rhs) {
				if (lhs.displayName != rhs.displayName) return lhs.displayName < rhs.displayName;
				return false;
			});
			for (auto i = page * pageSize; i < (page + 1) * pageSize && i < int(unfiltered.size()); ++i)
			{
				auto &info = unfiltered[i];
				try
				{
					auto saveData = Platform::ReadFile(String(info.path).ToUtf8());
					if (saveData.size())
					{
						info.save = std::make_shared<GameSave>(saveData);
						info.save->Expand();
						info.save->BlockSizeAfterExtract(0, 0, 0, info.saveDataSizeB.x, info.saveDataSizeB.y);
					}
					else
					{
						std::cerr << "skipping save " << String(info.path).ToUtf8() << ": failed to open" << std::endl;
					}
				}
				catch (const ParseException &e)
				{
					info.save.reset();
					// * TODO-REDO_UI: Do something with this.
				}
				saves.push_back(info);
			}
			status = true;
		}

	public:
		int saveCount;
		std::vector<SaveInfo> saves;

		void Start(std::shared_ptr<common::Task> self) final override
		{
			common::Worker::Ref().Dispatch(std::static_pointer_cast<common::WorkerTask>(self));
		}

		SearchSavesTask(Path newPath, int newPage) : path(newPath), page(newPage)
		{
			progressIndeterminate = true;
			process = [](WorkerTask &self) {
				static_cast<SearchSavesTask &>(self).Process();
			};
		}
	};

	LocalBrowser::LocalBrowser()
	{
		queryBox->PlaceHolder(String::Build("[", "DEFAULT_LS_LOCALBROWSER_SEARCH"_Ls(), "]"));

		foldersButton = EmplaceChild<gui::Button>().get();
		foldersButton->Text(String::Build(gui::ColorString({ 0xC0, 0xA0, 0x40 }), gui::IconString(gui::Icons::folderBody   , { 1, -2 }), gui::StepBackString(),
		                                  gui::ColorString({ 0xFF, 0xFF, 0xFF }), gui::IconString(gui::Icons::folderOutline, { 1, -2 }), " ", "DEFAULT_LS_LOCALBROWSER_FOLDER"_Ls()));
		foldersButton->Position({ 4, 4 });
		foldersButton->Size({ 70, 17 });

		deleteButton = EmplaceChild<gui::Button>().get();
		deleteButton->Visible(false);
		deleteButton->Text("DEFAULT_LS_BROWSER_DELETE"_Ls());
		deleteButton->Click([this]() {
			DeleteSelected();
		});

		searchSortDrop = EmplaceChild<gui::DropDown>().get();
		searchSortDrop->Options({
			String::Build(gui::ColorString({ 0xFF, 0xFF, 0xFF }), gui::IconString(gui::Icons::signPtrCenter, { 0, -1 }), gui::ResetString(), " ", "DEFAULT_LS_LOCALBROWSER_SORTMODE_BYNAME"_Ls()),
			String::Build(gui::ColorString({ 0xFF, 0x80, 0x80 }), gui::IconString(gui::Icons::calendar     , { 0, -1 }), gui::ResetString(), " ", "DEFAULT_LS_LOCALBROWSER_SORTMODE_BYDATE"_Ls()),
		});
		searchSortDrop->CenterSelected(false);
		searchSortDrop->Position({ windowSize.x - 74, 4 });
		searchSortDrop->Size({ 70, 17 });
		searchSortDrop->Change([this](int value) {
			LocalSearchStart(false, query, false, 0, true, SearchSort(value), false);
		});

		saveControls.push_back(deleteButton);
		saveControls.push_back(clearSelectionButton);

		searchRoot = Path::FromString(Platform::GetCwd().FromUtf8()).Append(LOCAL_SAVE_DIR);
	}

	void LocalBrowser::LocalSearchStart(
		bool force,
		String newQuery,
		bool resetQueryBox,
		int newPage,
		bool resetPageBox,
		SearchSort newSearchSort,
		bool resetSearchSortDrop
	)
	{
		if (!force &&
		    newSearchSort == searchSort &&
		    newPage == pagination->Page() &&
		    newQuery == query)
		{
			return;
		}
		if (!pagination->ValidPage(newPage))
		{
			return;
		}
		Browser::SearchStart(force, newQuery, resetQueryBox, newPage, resetPageBox);
		searchSaves = std::make_shared<SearchSavesTask>(searchRoot, pagination->Page());
		searchSaves->Start(searchSaves);
	}

	void LocalBrowser::SearchSavesFinish()
	{
		searchSpinner->Visible(false);
		if (searchSaves->status)
		{
			{
				auto pageMax = searchSaves->saveCount / pageSize + (searchSaves->saveCount % pageSize ? 1 : 0) - 1;
				if (pageMax < 0)
				{
					pageMax = 0;
				}
				pagination->PageMax(pageMax);
			}

			for (auto i = 0; i < int(searchSaves->saves.size()); ++i)
			{
				auto &info = searchSaves->saves[i];
				auto *button = EmplaceChild<Button>(info.displayName, "", 0, 0, false, false).get();
				button->ContextMenu([this, save = info.save, name = info.displayName, button]() {
					std::vector<Button::ContextMenuEntry> entries;
					if (save)
					{
						entries.push_back(Button::ContextMenuEntry{ "DEFAULT_LS_BROWSER_CONTEXT_OPEN"_Ls(), [this, save, name]() {
							Open(name, save);
						} });
					}
					if (button->Selected())
					{
						entries.push_back(Button::ContextMenuEntry{ "DEFAULT_LS_BROWSER_CONTEXT_DESELECT"_Ls(), [this, button]() {
							button->Selected(false);
							UpdateSaveControls();
						} });
					}
					else
					{
						entries.push_back(Button::ContextMenuEntry{ "DEFAULT_LS_BROWSER_CONTEXT_SELECT"_Ls(), [this, button]() {
							button->Selected(true);
							UpdateSaveControls();
						} });
					}
					return entries;
				});
				button->Click([this, name = info.displayName, save = info.save]() {
					Open(name, save);
				});
				button->Select([this](bool selected) {
					UpdateSaveControls();
				});
				auto sbi = std::make_unique<SaveButtonInfo>();
				sbi->button = button;
				sbi->path = info.path;
				sbi->displayName = info.displayName;
				sbi->slot = i;
				if (info.save)
				{
					sbi->saveRender = graphics::ThumbnailRendererTask::Create({
						info.save,
						true,
						true,
						RENDER_BASC | RENDER_FIRE | RENDER_SPRK | RENDER_EFFE,
						0,
						COLOUR_DEFAULT,
						0,
						{ 0, 0 },
						{ { 0, 0 }, info.saveDataSizeB },
					});
					sbi->saveRender->Start(sbi->saveRender);
				}
				saveButtonsToDisplay.push_back(sbi.get());
				saveButtons.push_back(std::move(sbi));
			}
			DisplaySaveButtons();
			if (!searchSaves->saves.size())
			{
				errorStatic->Visible(true);
				errorStatic->Text("DEFAULT_LS_BROWSER_NOSAVES"_Ls());
			}
		}
		else
		{
			errorStatic->Visible(true);
			errorStatic->Text(String::Build("DEFAULT_LS_LOCALBROWSER_SAVELIST_ERROR"_Ls(), searchSaves->error));
			pagination->PageMax(0);
		}
		searchSaves.reset();
	}

	void LocalBrowser::NavigateTo(Path newSearchRoot)
	{
		searchRoot = newSearchRoot;
		LocalSearchStart(true, "", true, 0, true, searchSort, false);
	}

	void LocalBrowser::SearchStart(bool force, String newQuery, bool resetQueryBox, int newPage, bool resetPageBox)
	{
		LocalSearchStart(force, newQuery, resetQueryBox, newPage, resetPageBox, searchSort, false);
	}

	void LocalBrowser::UpdateTasks()
	{
		if (searchSaves && searchSaves->complete)
		{
			SearchSavesFinish();
		}
		for (auto &sb : saveButtons)
		{
			auto *osb = static_cast<SaveButtonInfo *>(sb.get());
			if (osb->saveRender && osb->saveRender->complete)
			{
				if (!osb->saveRender->status)
				{
					// * TODO-REDO_UI: Make it so that this is guaranteed to not happen.
					throw std::runtime_error("rendering paste thumbnail failed");
				}
				osb->button->ImageData(osb->saveRender->output);
				osb->saveRender.reset();
			}
		}
	}

	void LocalBrowser::Tick()
	{
		ModalWindow::Tick();
		UpdateTasks();
	}

	void LocalBrowser::UpdateSaveControls()
	{
		auto somethingSelected = !GetSelected().empty();
		pagination->PageBefore()->Visible(!somethingSelected);
		pagination->PageBox()->Visible(!somethingSelected);
		pagination->PageAfter()->Visible(!somethingSelected);
		deleteButton->Visible(somethingSelected);
		clearSelectionButton->Visible(somethingSelected);
		Browser::UpdateSaveControls();
	}

	void LocalBrowser::Open(String displayName, std::shared_ptr<GameSave> save)
	{
		game::Game::Ref().LoadLocalSave(displayName, save);
		Quit();
	}

	void LocalBrowser::DeleteSelected()
	{
		auto titleString = "DEFAULT_LS_BROWSER_DELETE_TITLE"_Ls();
		auto confirmString = "DEFAULT_LS_BROWSER_DELETE_BODY"_Ls();
		StringBuilder bodyBuilder;
		bodyBuilder << confirmString << "\n";
		for (auto *sb : GetSelected())
		{
			auto *osb = static_cast<const SaveButtonInfo *>(sb);
			bodyBuilder << "\n - " << osb->displayName;
		}
		gui::SDLWindow::Ref().EmplaceBack<dialog::Confirm>(titleString, bodyBuilder.Build())->Finish([this](bool confirmed) {
			if (confirmed)
			{
				for (auto *sb : GetSelected())
				{
					auto *osb = static_cast<const SaveButtonInfo *>(sb);
					Platform::RemoveFile(osb->path.ToUtf8());
				}
				Refresh();
			}
		});
	}
}