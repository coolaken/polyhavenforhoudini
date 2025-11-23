#pragma once
#include <UT/UT_DSOVersion.h>
#include <CMD/CMD_Manager.h>
#include <CMD/CMD_Args.h>
#include <iostream>
#include "startwindow.h"



/// cmd_polyhaven()
///
/// Callback function for the new 'cmd_polyhaven' command
static void
cmd_polyhaven(CMD_Args& args)
{
    StartWindow* ins = StartWindow::getInstance();
    ins->show();

}

/// This function gets called once during Houdini initialization to register
/// the 'cmd_polyhaven' hscript command.
void
CMDextendLibrary(CMD_Manager* cman)
{
    // install the cmd_polyhaven command into the command manager
    cman->installCommand("cmd_polyhaven", "", cmd_polyhaven);
}