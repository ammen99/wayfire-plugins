#include <wayfire/plugin.hpp>
#include <wayfire/core.hpp>
#include <wayfire/signal-definitions.hpp>
#include <linux/input-event-codes.h>

class workaround_logitech_broken_button : public wf::plugin_interface_t
{
  public:
    void init() override
    {
        wf::get_core().connect(&on_button);
    }

    wf::signal::connection_t<wf::input_event_signal<wlr_pointer_button_event>> on_button = [=] (
        wf::input_event_signal<wlr_pointer_button_event> *ev)
    {
        if (ev->event->button == BTN_EXTRA)
        {
            ev->mode = wf::input_event_processing_mode_t::IGNORE;
        }
    };
};

DECLARE_WAYFIRE_PLUGIN(workaround_logitech_broken_button);
