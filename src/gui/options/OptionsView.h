#pragma once
#include "common/String.h"
#include "gui/interface/Window.h"
#include "gui/interface/ScrollPanel.h"
#include "InitSimulationConfig.h"
#include <optional>

namespace ui
{
	class Checkbox;
	class DropDown;
	class Textbox;
	class Label;
	class Button;
	class Label;
}

class OptionsModel;
class OptionsController;
class OptionsView: public ui::Window
{
	OptionsController *c{};
	ui::Checkbox *heatSimulation{};
	ui::Checkbox *ambientHeatSimulation{};
	ui::Checkbox *newtonianGravity{};
	ui::Checkbox *waterEqualisation{};
	ui::DropDown *airMode{};
	ui::Textbox *horizontalCellCount{};
	ui::Textbox *verticalCellCount{};
	ui::Textbox *cellSize{};
	ui::Textbox *partCount{};
	ui::Textbox *maxTemp{};
	ui::Textbox *minTemp{};
	ui::Textbox *maxPressure{};
	ui::Textbox *minPressure{};
	ui::Textbox *ambientAirTemp{};
	ui::Button *ambientAirTempPreview{};
	ui::DropDown *gravityMode{};
	ui::DropDown *edgeMode{};
	ui::DropDown *temperatureScale{};
	ui::DropDown *scale{};
	ui::Checkbox *resizable{};
	ui::Checkbox *fullscreen{};
	ui::Checkbox *changeResolution{};
	ui::Checkbox *forceIntegerScaling{};
	ui::Checkbox *blurryScaling{};
	ui::Checkbox *fastquit{};
	ui::Checkbox *globalQuit{};
	ui::DropDown *decoSpace{};
	ui::Checkbox *showAvatars{};
	ui::Checkbox *momentumScroll{};
	ui::Checkbox *mouseClickRequired{};
	ui::Checkbox *includePressure{};
	ui::Checkbox *perfectCircle{};
	ui::Checkbox *graveExitsConsole{};
	ui::Checkbox *nativeClipoard{};
	ui::Checkbox *threadedRendering{};
	ui::Checkbox *redirectStd{};
	ui::Checkbox *autoStartupRequest{};
	ui::Label *startupRequestStatus{};
	ui::ScrollPanel *scrollPanel{};
	ui::Label *simConfigStatus{};
	std::optional<SimulationConfig> newConfig;
	float customGravityX, customGravityY;
	void UpdateAmbientAirTempPreview(float airTemp, bool isValid);
	void AmbientAirTempToTextBox(float airTemp);
	void UpdateAirTemp(String temp, bool isDefocus);
	void UpdateStartupRequestStatus();
	void LoadSimConfig(SimulationConfig config);
	void SaveSimConfig();
	void UpdateSimConfig();
public:
	OptionsView();
	void NotifySettingsChanged(OptionsModel * sender);
	void AttachController(OptionsController * c_);
	void OnDraw() override;
	void OnTick() final override;
	void OnTryExit(ExitMethod method) override;
};
