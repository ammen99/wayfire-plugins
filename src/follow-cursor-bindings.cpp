#include <wayfire/plugin.hpp>
#include <wayfire/output.hpp>
#include <wayfire/output-layout.hpp>
#include <wayfire/workspace-manager.hpp>
#include <wayfire/plugins/common/view-change-viewport-signal.hpp>

class primary_monitor_switch_t : public wf::plugin_interface_t
{
    wf::option_wrapper_t<
        wf::config::compound_list_t<wf::activatorbinding_t, std::string>>
        bindings{"follow-cursor-bindings/bindings"};

    std::vector<wf::activator_callback> callbacks;

  public:
    void init() override
    {
        bindings.set_callback(rebuild_bindings);
        rebuild_bindings();
    }

    void clear_bindings()
    {
        for (auto& cb : callbacks)
        {
            output->rem_binding(&cb);
        }

        callbacks.clear();
    }

    std::function<void()> rebuild_bindings = [=] ()
    {
        clear_bindings();

        auto cur_bindings = bindings.value();
        callbacks.resize(cur_bindings.size());

        size_t i = 0;
        for (auto [_, act, cmd] : cur_bindings)
        {
            callbacks[i] = [cmd = cmd] (auto data)
            {
                auto& core = wf::get_core();
                auto cursor = core.get_cursor_position();
                auto wo = core.output_layout->get_output_coords_at(cursor, cursor);
                if (wo)
                {
                    return wo->call_plugin(cmd, data);
                }

                return false;
            };

            output->add_activator(wf::create_option(act), &callbacks[i]);
            ++i;
        }
    };

    void fini() override
    {
        clear_bindings();
    }
};

DECLARE_WAYFIRE_PLUGIN(primary_monitor_switch_t)
