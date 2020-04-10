#include <wayfire/plugin.hpp>
#include <wayfire/output.hpp>
#include <wayfire/output-layout.hpp>
#include <wayfire/workspace-manager.hpp>
#include <wayfire/plugins/common/view-change-viewport-signal.hpp>

class primary_monitor_switch_t : public wf::plugin_interface_t
{
    wf::option_wrapper_t<std::string> external_monitor{
        "primary-monitor-switch/external-monitor"};

    wf::signal_connection_t on_pre_remove = {[=] (wf::signal_data_t*) {
        if (this->output->to_string() != (std::string)external_monitor)
            return;

        /* Find next output */
        auto outputs = wf::get_core().output_layout->get_outputs();
        wf::output_t *next_output = nullptr;
        for (auto other_output : outputs)
        {
            if (other_output != this->output)
                next_output = other_output;
        }

        if (next_output == nullptr)
        {
            /* Maybe we are running on a single output, or the compositor is
             * shutting down and there are no more outputs */
            return;
        }

        this->move_views_to_output(this->output, next_output);
    }};

    wf::wl_idle_call delayed_action;

  public:
    void init() override
    {
        delayed_action.run_once([=] () {
            if ((std::string)external_monitor == this->output->to_string())
            {
                auto outputs = wf::get_core().output_layout->get_outputs();
                for (wf::output_t *other_output : outputs)
                {
                    if (other_output != this->output)
                        move_views_to_output(other_output, this->output);
                }
            }
        });

        this->output->connect_signal("pre-remove", &on_pre_remove);
    }

    void move_views_to_output(wf::output_t *from, wf::output_t *to)
    {
        assert(from && to);
        wf::dimensions_t grid_size = from->workspace->get_workspace_grid_size();
        static constexpr uint32_t interesting_layers =
            wf::LAYER_WORKSPACE | wf::LAYER_MINIMIZED | wf::LAYER_FULLSCREEN;

        for (int x = 0; x < grid_size.width; x++)
        {
            for (int y = 0; y < grid_size.height; y++)
            {
                auto views = from->workspace->get_views_on_workspace(
                    {x, y}, interesting_layers, true);
                std::reverse(views.begin(), views.end());

                for (wayfire_view v : views)
                    move_view(v, to, {x, y});
            }
        }

        to->workspace->request_workspace(
            from->workspace->get_current_workspace());
    }

    void move_view(wayfire_view view, wf::output_t *to, wf::point_t target_ws)
    {
        wf::get_core().move_view_to_output(view, to);
        wf::point_t current_ws = to->workspace->get_current_workspace();

        to->workspace->move_to_workspace(view, target_ws);
        if (current_ws != target_ws)
        {
            view_change_viewport_signal data;
            data.view = view;
            data.from = current_ws;
            data.to = target_ws;
            to->emit_signal("view-change-viewport", &data);
        }

        if (view->fullscreen)
            view->fullscreen_request(to, true);

        if (view->tiled_edges)
            view->tile_request(view->tiled_edges);
    }

    void fini() override
    {
        /* Destroy plugin */
        /* This time, there is nothing to destroy, since signal_connection_t
         * will disconnect automatically */
    }
};

DECLARE_WAYFIRE_PLUGIN(primary_monitor_switch_t)
