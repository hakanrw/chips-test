/*
    UI implementation for mp1000.c, this must live in a .cc file.
*/
#include "chips/chips_common.h"
#include "chips/mc6800.h"
#include "chips/mc6821.h"
#include "chips/mc6847.h"
#include "chips/kbd.h"
#include "chips/mem.h"
#include "chips/clk.h"
#include "systems/mp1000.h"
#define UI_DASM_USE_MC6800
#define UI_DBG_USE_MC6800
#define CHIPS_UTIL_IMPL
#include "util/mc6800dasm.h"
#define CHIPS_UI_IMPL
#include "imgui.h"
#include "imgui_internal.h"
#include "ui/ui_util.h"
#include "ui/ui_settings.h"
#include "ui/ui_chip.h"
#include "ui/ui_memedit.h"
#include "ui/ui_memmap.h"
#include "ui/ui_dasm.h"
#include "ui/ui_dbg.h"
#include "ui/ui_mc6800.h"
#include "ui/ui_mc6821.h"
#include "ui/ui_mc6847.h"
#include "ui/ui_audio.h"
#include "ui/ui_display.h"
#include "ui/ui_kbd.h"
#include "ui/ui_snapshot.h"
#include "ui/ui_mp1000.h"
