#include <wayfire/plugin.hpp>
#include <wayfire/input-device.hpp>
#include <wayfire/plugins/common/shared-core-data.hpp>
#include <wayfire/util/log.hpp>
#include <wayfire/render-manager.hpp>
#include <wayfire/workspace-manager.hpp>
#include <libinput.h>
#include <wayfire/option-wrapper.hpp>
#include <wayfire/touch/touch.hpp>
#include <wayfire/output.hpp>
#include <wayfire/signal-definitions.hpp>
#include <wayfire/debug.hpp>
#include <wayfire/util/duration.hpp>
#include <wayfire/nonstd/wlroots-full.hpp>

#include "ipc.hpp"

namespace wf
{

class tablet_mode_t
{
    std::unique_ptr<wf::ipc::server_t> server;
    bool tablet_mode = false;

  public:
    tablet_mode_t()
    {
        server = std::make_unique<wf::ipc::server_t>(
            "/tmp/wayfire-touch.socket");

        server->register_method("touch/set_tablet_mode", set_tablet_mode);
        server->register_method("touch/lock_rotation", lock_rotation);
        server->register_method("touch/get_tablet_mode", get_tablet_mode);
        server->register_method("touch/get_lock_rotation", get_lock_rotation);
    }

    using method_t = wf::ipc::server_t::method_cb;
    method_t set_tablet_mode = [=] (nlohmann::json data)
    {
        if (!data.count("tablet") || !data["tablet"].is_boolean()) {
            LOGE("Invalid tablet mode data, missing tablet boolean option.");
        }

        this->tablet_mode = data["tablet"];
        for (auto& dev : wf::get_core().get_input_devices())
        {
            auto wlr = dev->get_wlr_handle();
            if (wlr->type == WLR_INPUT_DEVICE_KEYBOARD ||
                wlr->type == WLR_INPUT_DEVICE_POINTER)
            {
                dev->set_enabled(!data["tablet"]);
            }
        }

        return nlohmann::json{};
    };

    method_t lock_rotation = [] (nlohmann::json data)
    {
        if (!data.count("locked") || !data["locked"].is_boolean()) {
            LOGE("Invalid lock rotation data, missing locked boolean option.");
        }

        auto& cfg = wf::get_core().config;
        auto opt = cfg.get_option("autorotate-iio/lock_rotation");

        opt->set_locked(true);
        opt->set_value_str(data["locked"] ? "true" : "false");
        return nlohmann::json{};
    };

    method_t get_tablet_mode = [=] (nlohmann::json)
    {
        nlohmann::json js;
        js["tablet"] = tablet_mode;
        return js;
    };

    method_t get_lock_rotation = [=] (nlohmann::json)
    {
        wf::option_wrapper_t<bool> locked{"autorotate-iio/lock_rotation"};

        nlohmann::json js;
        js["locked"] = (bool)locked;
        return js;
    };
};

using namespace wf::touch;

class reveal_action_t : public gesture_action_t
{
    std::function<void()> on_start, on_move;
  public:
    reveal_action_t(std::function<void()> start,
        std::function<void()> move)
    {
        this->on_start = start;
        this->on_move = move;
    }

    action_status_t update_state(const gesture_state_t&,
        const gesture_event_t& event) override
    {
        if (event.type != touch::EVENT_TYPE_MOTION)
        {
            return ACTION_STATUS_COMPLETED;
        }

        on_move();
        return ACTION_STATUS_RUNNING;
    }

    void reset(uint32_t time) override
    {
        gesture_action_t::reset(time);
        on_start();
    }
};

class tablet_plugin_t : public wf::plugin_interface_t
{
    wf::shared_data::ref_ptr_t<tablet_mode_t> tablet;

    touch_action_t *touch_down;
    std::unique_ptr<gesture_t> reveal_gesture;
    std::unique_ptr<gesture_t> tap_to_close_gesture;

    wf::wl_listener_wrapper needs_osk;

  public:
    void init()
    {
        grab_interface->capabilities = CAPABILITY_GRAB_INPUT;
        // Setup a gesture for opening amoxtli-panel.
        // The user needs to swipe from the top edge.

        // First, user touches down
        auto touch_down = std::make_unique<touch_action_t>(1, true);
        this->touch_down = touch_down.get();
        // Set initial target
        on_output_config_changed.emit(NULL);
        output->connect_signal("output-configuration-changed",
            &on_output_config_changed);

        // Second, user swipes down
        auto swipe_down = std::make_unique<drag_action_t>(
            MOVE_DIRECTION_UP, 50);

        // Third, we reveal amoxtli.
        // A special gesture which moves the panel.
        auto reveal_up = std::make_unique<reveal_action_t>(
            drag_started, drag_continues);

        std::vector<std::unique_ptr<gesture_action_t>> actions;
        actions.emplace_back(std::move(touch_down));
        actions.emplace_back(std::move(swipe_down));
        actions.emplace_back(std::move(reveal_up));

        reveal_gesture = std::make_unique<gesture_t>(std::move(actions), drag_ended);
        wf::get_core().add_touch_gesture(reveal_gesture);

        // Setup a gesture for closing amoxtli-panel by clicking everywhere else on the screen.
        auto tap_down = std::make_unique<touch_action_t>(1, true);
        auto tap_up = std::make_unique<touch_action_t>(1, false);
        tap_up->set_move_tolerance(5);
        tap_up->set_duration(100);

        actions.clear();
        actions.emplace_back(std::move(tap_down));
        actions.emplace_back(std::move(tap_up));
        tap_to_close_gesture = std::make_unique<gesture_t>(std::move(actions), close_panel);
        wf::get_core().add_touch_gesture(tap_to_close_gesture);

        // Connect signals necessary for the panel
        output->connect_signal("view-mapped", &on_mapped);
        output->connect_signal("view-unmapped", &on_unmaped);
    }

    void fini()
    {
        wf::get_core().rem_touch_gesture(reveal_gesture);
        wf::get_core().rem_touch_gesture(tap_to_close_gesture);
    }

    wf::signal_connection_t on_output_config_changed = [&] (wf::signal_data_t *)
    {
        auto og = output->get_layout_geometry();
        touch_target_t target;

        target.x = og.x;
        target.y = og.y + og.height - 20;
        target.width = og.width;
        target.height = 20;

        touch_down->set_target(target);
    };

    wf::animation::simple_animation_t panel_dropdown{wf::create_option(300)};
    wayfire_view panel;
    bool close_panel_on_animation_done = false;

    wf::effect_hook_t animate_panel = [=] ()
    {
        if (!panel || !panel_dropdown.running())
        {
            output->render->rem_effect(&animate_panel);
            if (close_panel_on_animation_done && panel)
            {
                panel->close();
            }
        }

        wf::geometry_t g = panel->get_wm_geometry();
        auto og = output->get_relative_geometry();
        g.x = og.width / 2 - g.width / 2;
        g.y = (int)panel_dropdown;
        panel->set_geometry(g);
        output->render->schedule_redraw();
    };

    int autopick_y()
    {
        if (!panel) return 0;

        auto state = wf::get_core().get_touch_state();
        if (state.fingers.count(0))
        {
            return state.fingers[0].current.y - output->get_layout_geometry().y;
        } else
        {
            return panel->get_wm_geometry().height;
        }
    }

    void start_animation(int y)
    {
        if (!panel)
        {
            return;
        }

        int min_y = output->get_screen_size().height - panel->get_wm_geometry().height - 50;
        panel_dropdown.animate(std::max(y, min_y));

        output->render->rem_effect(&animate_panel);
        output->render->add_effect(&animate_panel, OUTPUT_EFFECT_PRE);
        output->render->schedule_redraw();
    }

    wf::signal_connection_t on_mapped = [&] (wf::signal_data_t *data)
    {
        auto view = get_signaled_view(data);
        if (view->get_app_id() == "amoxtli.panel")
        {
            close_panel_on_animation_done = false;
            panel = view;

            output->workspace->add_view(panel, LAYER_DESKTOP_WIDGET);

            // Initially, the panel should be hidden
            auto y = output->get_screen_size().height;
            panel_dropdown.set(y, y);
            start_animation(autopick_y());
        }
    };

    wf::signal_connection_t on_unmaped = [&] (wf::signal_data_t *data)
    {
        if (get_signaled_view(data) == panel)
        {
            panel = nullptr;
        }
    };

    std::function<void()> drag_started = [=] ()
    {
        grab_interface->grab();
        if (!panel)
        {
            wf::get_core().run("pkill wf-panel");
            wf::get_core().run("amoxtli-panel 1080");
        }
    };

    std::function<void()> drag_continues = [=] ()
    {
        start_animation(autopick_y());
    };

    std::function<void()> drag_ended = [=] ()
    {
        grab_interface->ungrab();
        if (panel)
        {
            start_animation(panel->get_wm_geometry().height);
        }
    };

    std::function<void()> close_panel = [=] ()
    {
        if (panel && panel != wf::get_core().get_touch_focus_view())
        {
            close_panel_on_animation_done = true;
            start_animation(output->get_screen_size().height);
        }
    };
};
}

DECLARE_WAYFIRE_PLUGIN(wf::tablet_plugin_t);
