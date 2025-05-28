#include <wayfire/plugin.hpp>
#include <wayfire/core.hpp>
#include <wayfire/output.hpp>
#include <wayfire/bindings-repository.hpp>
#include <wayfire/signal-definitions.hpp>
#include <wayfire/config/config-manager.hpp>
#include <wayfire/util.hpp>

struct skb_layout_t
{
    std::string xkb_layout;
    std::string xkb_variant;
};

class switch_kb_layouts : public wf::plugin_interface_t
{
    wf::option_wrapper_t<wf::activatorbinding_t> activator{"switch-kb-layouts/toggle"};

    skb_layout_t current;
    skb_layout_t other;

    bool already_locked = false;
    void toggle()
    {
        auto& cfg = wf::get_core().config;
        auto layout = cfg->get_option("input/xkb_layout");
        auto variant = cfg->get_option("input/xkb_variant");

        if (!already_locked)
        {
            already_locked = true;
            layout->set_locked(1);
            variant->set_locked(1);
        }

        layout->set_value_str(other.xkb_layout);
        variant->set_value_str(other.xkb_variant);
        std::swap(current, other);

        // Workaround for Wayfire core: trigger xkb_* option reload
        wf::reload_config_signal data;
        wf::get_core().emit(&data);
    }

  public:
    void init()
    {
        other     = {"us,bg", ",phonetic"};
        current   = {"us,de", ","};

        wf::get_core().bindings->add_activator(activator, &on_switch);
        toggle();
    }

    void fini()
    {
        if (already_locked)
        {
            auto& cfg = wf::get_core().config;
            auto layout = cfg->get_option("input/xkb_layout");
            auto variant = cfg->get_option("input/xkb_variant");
            layout->set_locked(0);
            variant->set_locked(0);
        }

        wf::get_core().bindings->rem_binding(&on_switch);
    }

    wf::wl_timer<false> timer;

  private:
    wf::activator_callback on_switch = [=] (const wf::activator_data_t&)
    {
        timer.set_timeout(1000, [=] ()
        {
            toggle();
        });
        return true;
    };
};

DECLARE_WAYFIRE_PLUGIN(switch_kb_layouts);
