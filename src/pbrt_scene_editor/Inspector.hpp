#pragma once

#include "editorComponent.hpp"

struct Inspectable
{
    //layout
    virtual void show(){

    }
};

struct Inspector : EditorComponentGUI
{
    Inspector();
	virtual void constructFrame() override;
    void init();
    ~Inspector();
private:
	Inspectable* _currentInspect;
};