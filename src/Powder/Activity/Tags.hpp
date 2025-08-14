#pragma once
#include "Gui/View.hpp"
#include "common/String.h"
#include <future>
#include <list>

class SaveInfo;

namespace Powder::Activity
{
	class Game;

	class Tags : public Gui::View
	{
		SaveInfo &saveInfo;

		std::future<std::list<ByteString>> addTagFuture;
		std::future<std::list<ByteString>> removeTagFuture;

		bool CanAddTag() const;
		std::string newTag;

		void Gui() final override;
		CanApplyKind CanApply() const final override;
		void Apply() final override;
		void HandleTick() final override;

	public:
		Tags(Gui::Host &newHost, SaveInfo &newSaveInfo) : View(newHost), saveInfo(newSaveInfo)
		{
		}
	};
}
