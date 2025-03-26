#include "Game.hpp"
#include "Gui/StaticTexture.hpp"
#include "gui/game/tool/Tool.h"
#include "gui/game/tool/ElementTool.h"
#include "gui/game/tool/WallTool.h"
#include "gui/game/tool/DecorationTool.h"
#include "gui/game/tool/SignTool.h"
#include "gui/game/tool/SampleTool.h"
#include "gui/game/tool/GOLTool.h"
#include "gui/game/tool/PropertyTool.h"
#include "simulation/SimulationData.h"
#include "simulation/ToolClasses.h"
#include "simulation/ElementClasses.h"
#include "simulation/GOLString.h"
#include "prefs/GlobalPrefs.h"
#include "lua/CommandInterface.h"

namespace Powder::Activity
{
	std::optional<CustomGOLData> Game::CheckCustomGolToAdd(String ruleString, String nameString, RGB color1, RGB color2)
	{
		if (!ValidateGOLName(nameString))
		{
			return std::nullopt;
		}
		auto rule = ParseGOLString(ruleString);
		if (rule == -1)
		{
			return std::nullopt;
		}
		auto &sd = SimulationData::CRef();
		if (sd.GetCustomGOLByRule(rule))
		{
			return std::nullopt;
		}
		for (auto &gd : sd.GetCustomGol())
		{
			if (gd.nameString == nameString)
			{
				return std::nullopt;
			}
		}
		return CustomGOLData{ rule, color1, color2, nameString };
	}

	void Game::InitTools()
	{
		LoadFavorites();
		auto &sd = SimulationData::CRef();
		auto &elements = sd.elements;
		auto &builtinGol = SimulationData::builtinGol;
		for (int32_t i = 0; i < PT_NUM; ++i)
		{
			if (elements[i].Enabled)
			{
				AllocElementTool(i);
			}
		}
		for (int32_t i = 0; i < NGOL; ++i)
		{
			auto tool = std::make_unique<ElementTool>(PMAP(i, PT_LIFE), builtinGol[i].name, builtinGol[i].description, builtinGol[i].colour, "DEFAULT_PT_LIFE_" + builtinGol[i].name.ToAscii());
			tool->MenuSection = SC_LIFE;
			AllocTool(std::move(tool));
		}
		for (int32_t i = 0; i < UI_WALLCOUNT; ++i)
		{
			auto tool = std::make_unique<WallTool>(i, sd.wtypes[i].descs, sd.wtypes[i].colour, sd.wtypes[i].identifier, sd.wtypes[i].textureGen);
			tool->MenuSection = SC_WALL;
			AllocTool(std::move(tool));
		}
		for (auto &tool : ::GetTools())
		{
			AllocTool(std::make_unique<SimTool>(tool));
		}
		AllocTool(std::make_unique<DecorationTool>(*this, DECO_ADD     , "ADD" , "Colour blending: Add."                         , 0x000000_rgb, "DEFAULT_DECOR_ADD" ));
		AllocTool(std::make_unique<DecorationTool>(*this, DECO_SUBTRACT, "SUB" , "Colour blending: Subtract."                    , 0x000000_rgb, "DEFAULT_DECOR_SUB" ));
		AllocTool(std::make_unique<DecorationTool>(*this, DECO_MULTIPLY, "MUL" , "Colour blending: Multiply."                    , 0x000000_rgb, "DEFAULT_DECOR_MUL" ));
		AllocTool(std::make_unique<DecorationTool>(*this, DECO_DIVIDE  , "DIV" , "Colour blending: Divide."                      , 0x000000_rgb, "DEFAULT_DECOR_DIV" ));
		AllocTool(std::make_unique<DecorationTool>(*this, DECO_SMUDGE  , "SMDG", "Smudge tool, blends surrounding deco together.", 0x000000_rgb, "DEFAULT_DECOR_SMDG"));
		AllocTool(std::make_unique<DecorationTool>(*this, DECO_CLEAR   , "CLR" , "Erase any set decoration."                     , 0x000000_rgb, "DEFAULT_DECOR_CLR" ));
		AllocTool(std::make_unique<DecorationTool>(*this, DECO_DRAW    , "SET" , "Draw decoration (No blending)."                , 0x000000_rgb, "DEFAULT_DECOR_SET" ));
		AllocTool(std::make_unique<PropertyTool>(*this));
		AllocTool(std::make_unique<SignTool>(*this));
		AllocTool(std::make_unique<SampleTool>(*this));
		AllocTool(std::make_unique<GOLTool>(*this));
		LoadCustomGol();
		for (int32_t i = 0; i < toolSlotCount; ++i)
		{
			ResetToolSlot(i);
		}
		lastSelectedTool = toolSlots[0];
	}

	void Game::FreeTool(Tool *tool)
	{
		auto index = GetIndexFromTool(tool);
		if (!index)
		{
			return;
		}
		for (int32_t i = 0; i < toolSlotCount; ++i)
		{
			if (toolSlots[i] == tool)
			{
				ResetToolSlot(i);
			}
		}
		if (lastSelectedTool == tool)
		{
			lastSelectedTool = toolSlots[0];
		}
		if (lastHoveredTool == tool)
		{
			lastHoveredTool = nullptr;
		}
		commandInterface->SetToolIndex(tool->Identifier, std::nullopt);
		tools[*index].reset();
		shouldUpdateToolAtlas = true;
	}

	void Game::AllocElementTool(ElementIndex elementIndex)
	{
		auto &sd = SimulationData::CRef();
		auto &elements = sd.elements;
		auto &elem = elements[elementIndex];
		switch (elementIndex)
		{
		case PT_LIGH:
			AllocTool(std::make_unique<Element_LIGH_Tool>(elementIndex, elem.Identifier));
			break;

		case PT_TESC:
			AllocTool(std::make_unique<Element_TESC_Tool>(elementIndex, elem.Identifier));
			break;

		case PT_STKM:
		case PT_FIGH:
		case PT_STKM2:
			AllocTool(std::make_unique<PlopTool>(elementIndex, elem.Identifier));
			break;

		default:
			AllocTool(std::make_unique<ElementTool>(elementIndex, elem.Identifier));
			break;
		}
		UpdateElementTool(elementIndex);
	}

	void Game::UpdateElementTool(ElementIndex elementIndex)
	{
		auto &sd = SimulationData::CRef();
		auto &elements = sd.elements;
		auto &elem = elements[elementIndex];
		auto *tool = GetToolFromIdentifier(elem.Identifier);
		tool->Name = elem.Name;
		tool->Description = elem.Description;
		tool->Colour = elem.Colour;
		tool->textureGen = elem.IconGenerator;
		tool->MenuSection = elem.MenuSection;
		tool->MenuVisible = elem.MenuVisible;
		UpdateToolTexture(*GetIndexFromTool(tool));
	}

	void Game::ToggleFavorite(Tool *tool)
	{
		auto index = GetIndexFromTool(tool);
		auto &info = tools[*index];
		if (info->favorite)
		{
			info->favorite = false;
			favorites.erase(info->tool->Identifier);
		}
		else
		{
			info->favorite = true;
			favorites.insert(info->tool->Identifier);
		}
		SaveFavorites();
	}

	void Game::SelectTool(ToolSlotIndex toolSlotIndex, Tool *tool)
	{
		if (toolSlotIndex < 0 || toolSlotIndex >= toolSlotCount)
		{
			return;
		}
		toolSlots[toolSlotIndex] = tool;
		lastSelectedTool = tool;
		if (tool)
		{
			tool->Select(toolSlotIndex);
		}
	}

	Tool *Game::GetSelectedTool(ToolSlotIndex toolSlotIndex) const
	{
		if (toolSlotIndex < 0 || toolSlotIndex >= toolSlotCount)
		{
			return nullptr;
		}
		return toolSlots[toolSlotIndex];
	}

	Tool *Game::GetToolFromIdentifier(const ByteString &identifier)
	{
		for (auto &info : tools)
		{
			if (!info)
			{
				continue;
			}
			if (info->tool->Identifier == identifier)
			{
				return info->tool.get();
			}
		}
		return nullptr;
	}

	Tool *Game::GetToolFromIndex(ToolIndex index)
	{
		if (index >= 0 && index < int32_t(tools.size()) && tools[index])
		{
			return tools[index]->tool.get();
		}
		return nullptr;
	}

	std::optional<Game::ToolIndex> Game::GetIndexFromTool(const Tool *tool)
	{
		if (tool)
		{
			for (int32_t i = 0; i < int32_t(tools.size()); ++i)
			{
				if (tools[i] && tools[i]->tool.get() == tool)
				{
					return i;
				}
			}
		}
		return std::nullopt;
	}

	Tool *Game::AllocCustomGolTool(const CustomGOLData &gd)
	{
		auto tool = std::make_unique<ElementTool>(PMAP(gd.rule, PT_LIFE), gd.nameString, "Custom GOL type: " + SerialiseGOLRule(gd.rule), gd.colour1, "DEFAULT_PT_LIFECUST_" + gd.nameString.ToAscii(), nullptr); // TODO-REDO_UI-TRANSLATE
		tool->MenuSection = SC_LIFE;
		return AllocTool(std::move(tool));
	}

	Tool *Game::AllocTool(std::unique_ptr<Tool> tool)
	{
		std::optional<int32_t> index;
		for (int32_t i = 0; i < int32_t(tools.size()); ++i)
		{
			if (!tools[i])
			{
				index = i;
				break;
			}
		}
		if (!index)
		{
			index = int32_t(tools.size());
			tools.emplace_back();
		}
		commandInterface->SetToolIndex(tool->Identifier, *index);
		tools[*index] = GameToolInfo{};
		tools[*index]->favorite = favorites.find(tool->Identifier) != favorites.end();
		auto *ptr = tool.get();
		tools[*index]->tool = std::move(tool);
		UpdateToolTexture(*index);
		return ptr;
	}

	void Game::ResetToolSlot(ToolSlotIndex toolSlotIndex)
	{
		Tool *tool{};
		switch (toolSlotIndex)
		{
		case 0:
			tool = GetToolFromIdentifier("DEFAULT_PT_DUST");
			break;

		case 2:
			tool = GetToolFromIdentifier("DEFAULT_UI_SAMPLE");
			break;

		default:
			tool = GetToolFromIdentifier("DEFAULT_PT_NONE");
			break;
		}
		toolSlots[toolSlotIndex] = tool;
	}

	void Game::LoadFavorites()
	{
		auto vec = GlobalPrefs::Ref().Get("Favorites", std::vector<ByteString>{});
		for (auto &item : vec)
		{
			favorites.insert(item);
		}
	}

	void Game::SaveFavorites() const
	{
		std::vector<ByteString> vec;
		for (auto &item : favorites)
		{
			vec.push_back(item);
		}
		GlobalPrefs::Ref().Set("Favorites", vec);
	}

	void Game::SaveCustomGol() const
	{
		auto &prefs = GlobalPrefs::Ref();
		std::vector<ByteString> newCustomGOLTypes;
		auto &sd = SimulationData::CRef();
		for (auto &gd : sd.GetCustomGol())
		{
			StringBuilder sb;
			sb << gd.nameString << " " << SerialiseGOLRule(gd.rule) << " " << gd.colour1.Pack() << " " << gd.colour2.Pack();
			newCustomGOLTypes.push_back(sb.Build().ToUtf8());
		}
		prefs.Set("CustomGOL.Types", newCustomGOLTypes);
	}

	void Game::LoadCustomGol()
	{
		auto &prefs = GlobalPrefs::Ref();
		auto customGOLTypes = prefs.Get("CustomGOL.Types", std::vector<ByteString>{});
		bool removedAny = false;
		std::vector<CustomGOLData> newCustomGol;
		for (auto gol : customGOLTypes)
		{
			auto parts = gol.FromUtf8().PartitionBy(' ');
			if (parts.size() != 4)
			{
				removedAny = true;
				continue;
			}
			auto nameString = parts[0];
			auto ruleString = parts[1];
			auto &colour1String = parts[2];
			auto &colour2String = parts[3];
			RGB color1;
			RGB color2;
			try
			{
				color1 = RGB::Unpack(colour1String.ToNumber<int32_t>());
				color2 = RGB::Unpack(colour2String.ToNumber<int32_t>());
			}
			catch (std::exception &)
			{
				removedAny = true;
				continue;
			}
			if (auto gd = CheckCustomGolToAdd(ruleString, nameString, color1, color2))
			{
				newCustomGol.push_back(*gd);
				AllocCustomGolTool(*gd);
			}
			else
			{
				removedAny = true;
			}
		}
		auto &sd = SimulationData::Ref();
		sd.SetCustomGOL(newCustomGol);
		if (removedAny)
		{
			SaveCustomGol();
		}
	}

	Tool *Game::AddCustomGol(CustomGOLData gd)
	{
		auto &sd = SimulationData::Ref();
		auto newCustomGol = sd.GetCustomGol();
		newCustomGol.push_back(gd);
		sd.SetCustomGOL(newCustomGol);
		auto *tool = AllocCustomGolTool(gd);
		SaveCustomGol();
		return tool;
	}

	bool Game::RemoveCustomGol(const ByteString &identifier)
	{
		bool removedAny = false;
		std::vector<CustomGOLData> newCustomGol;
		auto &sd = SimulationData::Ref();
		for (auto gol : sd.GetCustomGol())
		{
			if ("DEFAULT_PT_LIFECUST_" + gol.nameString == identifier.FromUtf8())
			{
				removedAny = true;
			}
			else
			{
				newCustomGol.push_back(gol);
			}
		}
		if (removedAny)
		{
			sd.SetCustomGOL(newCustomGol);
			FreeTool(GetToolFromIdentifier(identifier));
			SaveCustomGol();
		}
		return removedAny;
	}

	void Game::UpdateToolTexture(ToolIndex index)
	{
		auto &info = *tools[index];
		info.texture.reset();
		shouldUpdateToolAtlas = true;
		std::unique_ptr<VideoBuffer> textureData;
		if (info.tool->MenuSection == SC_DECO)
		{
			textureData = static_cast<DecorationTool *>(info.tool.get())->GetIcon(info.tool->ToolID, toolTextureDataSize); // TODO: smelly >_>
		}
		else
		{
			textureData = info.tool->GetTexture(toolTextureDataSize);
		}
		if (!textureData)
		{
			return;
		}
		info.texture = GameToolInfo::TextureInfo{};
		info.texture->data = PlaneAdapter<std::vector<pixel>>(toolTextureDataSize);
		for (auto p : toolTextureDataSize.OriginRect())
		{
			info.texture->data[p] = textureData->Data()[p.Y * toolTextureDataSize.X + p.X];
		}
	}

	void Game::UpdateDecoTools()
	{
		for (int32_t i = 0; i < int32_t(tools.size()); ++i)
		{
			if (!tools[i] || tools[i]->tool->MenuSection != SC_DECO)
			{
				continue;
			}
			UpdateToolTexture(i);
		}
	}

	void Game::OpenProperty()
	{
		static_cast<PropertyTool *>(GetToolFromIdentifier("DEFAULT_UI_PROPERTY"))->OpenWindow();
	}

	std::optional<FindingElement> Game::GetFindParameters() const
	{
		Tool *active = toolSlots[0];
		if (!active)
		{
			return std::nullopt;
		}
		auto &properties = Particle::GetProperties();
		if (active->Identifier.Contains("_PT_"))
		{
			return FindingElement{ properties[FIELD_TYPE], active->ToolID };
		}
		else if (active->Identifier == "DEFAULT_UI_PROPERTY")
		{
			auto configuration = static_cast<PropertyTool *>(active)->GetConfiguration();
			if (configuration)
			{
				return FindingElement{ properties[configuration->changeProperty.propertyIndex], configuration->changeProperty.propertyValue };
			}
		}
		return std::nullopt;
	}

	void Game::UpdateToolAtlas()
	{
		auto texturedTools = int32_t(extraToolTextures.size());
		for (auto &info : tools)
		{
			if (info && info->texture)
			{
				texturedTools += 1;
			}
		}
		int32_t sideLength = 0;
		while (sideLength * sideLength < texturedTools)
		{
			sideLength += 1;
		}
		toolAtlas = PlaneAdapter<std::vector<pixel>>(Size2(sideLength * toolTextureDataSize.X, sideLength * toolTextureDataSize.Y));
		int32_t pos = 0;
		auto addTexture = [&](GameToolInfo::TextureInfo &texture) {
			texture.toolAtlasPos = Pos2(pos % sideLength * toolTextureDataSize.X, pos / sideLength * toolTextureDataSize.Y);
			for (auto p : texture.data.Size().OriginRect())
			{
				toolAtlas[p + texture.toolAtlasPos] = texture.data[p];
			}
			pos += 1;
		};
		for (auto &texture : extraToolTextures)
		{
			addTexture(texture);
		}
		for (auto &info : tools)
		{
			if (!(info && info->texture))
			{
				continue;
			}
			addTexture(*info->texture);
		}
		toolAtlasTexture = MakeToolAtlasTexture(*this);
	}

	std::unique_ptr<Gui::StaticTexture> Game::MakeToolAtlasTexture(View &view) const
	{
		return std::make_unique<Gui::StaticTexture>(view.GetHost(), false, toolAtlas);
	}

	float Game::GetToolStrength() const
	{
		if (toolStrengthMul10)
		{
			return 10.f;
		}
		if (toolStrengthDiv10)
		{
			return 0.1f;
		}
		return 1.0f;
	}
}
