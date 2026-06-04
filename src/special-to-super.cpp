#include <linux/input-event-codes.h>
#include <wayfire/core.hpp>
#include <wayfire/plugin.hpp>
#include <wayfire/signal-definitions.hpp>

class special_to_super_t : public wf::plugin_interface_t
{
    wf::signal::connection_t<wf::input_event_signal<wlr_keyboard_key_event>> on_key = [=] (
        wf::input_event_signal<wlr_keyboard_key_event> *ev)
    {
        if (ev->event->keycode == KEY_F13)
        {
            ev->event->keycode = KEY_LEFTMETA;
        }
    };

  public:
    void init() override
    {
        wf::get_core().connect(&on_key);
    }
};

DECLARE_WAYFIRE_PLUGIN(special_to_super_t);
