#pragma once

struct EditorComponentGUI
{
	virtual void constructFrame() = 0;
	bool is_open;
	virtual ~EditorComponentGUI() {

	}
};
