#include <wayfire/plugin.hpp>
#include <wayfire/core.hpp>
#include <wayfire/output.hpp>
#include <wayfire/plugins/common/shared-core-data.hpp>

struct skb_layout_t
{
    std::string xkb_layout;
    std::string xkb_variant;
};

struct skb_state_t
{
    skb_layout_t current;
    skb_layout_t other;

    skb_state_t()
    {
        other     = {"us,bg", ",phonetic"};
        current   = {"us,de", ","};
    }

    ~skb_state_t()
    {
        if (already_locked)
        {
            auto& cfg = wf::get_core().config;
            auto layout = cfg.get_option("input/xkb_layout");
            auto variant = cfg.get_option("input/xkb_variant");
            layout->set_locked(0);
            variant->set_locked(0);
        }
    }

    bool already_locked = false;
    void toggle()
    {
        auto& cfg = wf::get_core().config;
        auto layout = cfg.get_option("input/xkb_layout");
        auto variant = cfg.get_option("input/xkb_variant");

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
        wf::get_core().emit_signal("reload-config", nullptr);
    }
};

class switch_kb_layouts : public wf::plugin_interface_t
{
    wf::shared_data::ref_ptr_t<skb_state_t> state;
    wf::option_wrapper_t<wf::activatorbinding_t> activator{"switch-kb-layouts/toggle"};

  public:
    void init()
    {
        output->add_activator(activator, &on_switch);
        state->toggle();
    }

    void fini()
    {
        output->rem_binding(&on_switch);
    }

    wf::wl_timer timer;

  private:
    wf::activator_callback on_switch = [=] (const wf::activator_data_t&)
    {
        timer.set_timeout(1000, [=] ()
        {
            state->toggle();
            return false;
        });
        return true;
    };
};

DECLARE_WAYFIRE_PLUGIN(switch_kb_layouts);
