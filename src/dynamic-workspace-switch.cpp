#include <wayfire/plugin.hpp>
#include <wayfire/output.hpp>
#include <wayfire/output-layout.hpp>
#include <wayfire/workspace-manager.hpp>
#include <wayfire/plugins/common/view-change-viewport-signal.hpp>

class dynamic_workspace_switch_t : public wf::plugin_interface_t
{
  public:
    void init() override
    {
        output->add_activator(
            wf::create_option_string<wf::activatorbinding_t>("<super>KEY_J"), &on_switch_down);
        output->add_activator(
            wf::create_option_string<wf::activatorbinding_t>("<super>KEY_K"), &on_switch_up);

        // Initially, a single workspace
        output->workspace->set_workspace_grid_size({1, 1});
    }

    wf::activator_callback on_switch_down = [=] (auto)
    {
        do_switch(1);
        return true;
    };

    wf::activator_callback on_switch_up = [=] (auto)
    {
        do_switch(-1);
        return true;
    };

    void do_switch(int dy)
    {
        auto y = output->workspace->get_current_workspace().y;
        auto h = output->workspace->get_workspace_grid_size().height;

        int ny = y + dy;
        if (ny < 0)
        {
            return;
        }

        if (ny == h)
        {
            output->workspace->set_workspace_grid_size({1, h + 1});
        }

        output->workspace->request_workspace({0, ny});
        if (dy < 0)
        {
            sanitize();
        }
    }

    void sanitize()
    {
        int h = output->workspace->get_workspace_grid_size().height;
        int cy = output->workspace->get_current_workspace().y;

        int y = h - 1;
        for (; y > cy; y--)
        {
            auto views = output->workspace->get_views_on_workspace({0, y},
                wf::LAYER_MINIMIZED | wf::WM_LAYERS);

            if (!views.empty())
            {
                break;
            }
        }

        if (y != h - 1)
        {
            output->workspace->set_workspace_grid_size({1, y + 1});
        }
    }
};

DECLARE_WAYFIRE_PLUGIN(dynamic_workspace_switch_t)
