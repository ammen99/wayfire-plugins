#include <linux/input-event-codes.h>
#include <wayfire/core.hpp>
#include <wayfire/matcher.hpp>
#include <wayfire/plugin.hpp>
#include <wayfire/seat.hpp>
#include <wayfire/signal-definitions.hpp>

class super_to_special_t : public wf::plugin_interface_t
{
    wf::view_matcher_t enabled_for{"super-to-special/enabled_for"};

    wf::signal::connection_t<wf::input_event_signal<wlr_keyboard_key_event>> on_key = [=] (
        wf::input_event_signal<wlr_keyboard_key_event> *ev)
    {
        auto view = wf::get_core().seat->get_active_view();
        if (!view || !enabled_for.matches(view))
        {
            return;
        }

        if ((ev->event->keycode == KEY_LEFTMETA) || (ev->event->keycode == KEY_RIGHTMETA))
        {
            ev->event->keycode = KEY_F13;
        }
    };

  public:
    void init() override
    {
        wf::get_core().connect(&on_key);
    }
};

DECLARE_WAYFIRE_PLUGIN(super_to_special_t);
