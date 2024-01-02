#pragma once

#include "editorComponent.hpp"

struct Inspectable
{

};

struct Inspector : EditorComponentGUI
{
	virtual void constructFrame() override;
private:
	Inspectable* _currentInspect;
};