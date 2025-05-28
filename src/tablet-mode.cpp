#include <wayfire/plugin.hpp>
#include <wayfire/per-output-plugin.hpp>
#include <wayfire/input-device.hpp>
#include <wayfire/plugins/common/shared-core-data.hpp>
#include <wayfire/config/config-manager.hpp>
#include <wayfire/scene-input.hpp>
#include <wayfire/scene.hpp>
#include <wayfire/util.hpp>
#include <wayfire/util/log.hpp>
#include <wayfire/render-manager.hpp>
#include <wayfire/workspace-set.hpp>
#include <libinput.h>
#include <wayfire/option-wrapper.hpp>
#include <wayfire/touch/touch.hpp>
#include <wayfire/output.hpp>
#include <wayfire/signal-definitions.hpp>
#include <wayfire/debug.hpp>
#include <wayfire/util/duration.hpp>
#include <wayfire/nonstd/wlroots-full.hpp>
#include <wayfire/plugins/common/shared-core-data.hpp>
#include <wayfire/plugins/ipc/ipc-method-repository.hpp>
#include <wayfire/plugins/ipc/ipc-helpers.hpp>
#include <wayfire/plugins/common/input-grab.hpp>
#include <wayfire/toplevel-view.hpp>
#include <wayfire/plugins/input-method-v1/input-method-v1.hpp>
#include <linux/input-event-codes.h>

namespace wf
{

class tablet_mode_t
{
    bool tablet_mode = false;
    shared_data::ref_ptr_t<ipc::method_repository_t> repo;

    wf::wl_timer<false> timer_osk;

    wf::signal::connection_t<wf::input_method_v1_activate_signal> on_im_activate = [=] (auto)
    {
        if (tablet_mode)
        {
            timer_osk.disconnect();
            timer_osk.set_timeout(500, [] () { wf::get_core().run("wf-osk -a bottom -b 40"); });
        }
    };

    wf::signal::connection_t<wf::input_method_v1_deactivate_signal> on_im_deactivate = [=] (auto)
    {
        if (tablet_mode)
        {
            timer_osk.disconnect();
            timer_osk.set_timeout(500, [] () { wf::get_core().run("pkill wf-osk"); });
        }
    };

  public:
    tablet_mode_t()
    {
        repo->register_method("touch/set_tablet_mode", set_tablet_mode);
        repo->register_method("touch/lock_rotation", lock_rotation);
        repo->register_method("touch/get_tablet_mode", get_tablet_mode);
        repo->register_method("touch/get_lock_rotation", get_lock_rotation);

        wf::get_core().connect(&on_im_activate);
        wf::get_core().connect(&on_im_deactivate);
    }

    ~tablet_mode_t()
    {
        repo->unregister_method("touch/set_tablet_mode");
        repo->unregister_method("touch/lock_rotation");
        repo->unregister_method("touch/get_tablet_mode");
        repo->unregister_method("touch/get_lock_rotation");
    }

    wf::ipc::method_callback set_tablet_mode = [=] (const wf::json_t& data) -> wf::json_t
    {
        this->tablet_mode = wf::ipc::json_get_bool(data, "tablet");
        for (auto& dev : wf::get_core().get_input_devices())
        {
            auto wlr = dev->get_wlr_handle();
            if (wlr->type == WLR_INPUT_DEVICE_KEYBOARD ||
                wlr->type == WLR_INPUT_DEVICE_POINTER)
            {
                dev->set_enabled(!tablet_mode);
            }
        }

        return wf::ipc::json_ok();
    };

    ipc::method_callback lock_rotation = [] (const wf::json_t& data)
    {
        auto locked = wf::ipc::json_get_bool(data, "locked");

        auto& cfg = wf::get_core().config;
        auto opt = cfg->get_option("autorotate-iio/lock_rotation");

        opt->set_locked(true);
        opt->set_value_str(locked ? "true" : "false");
        return wf::ipc::json_ok();
    };

    ipc::method_callback get_tablet_mode = [=] (auto)
    {
        wf::json_t js;
        js["tablet"] = tablet_mode;
        return js;
    };

    ipc::method_callback get_lock_rotation = [=] (auto)
    {
        wf::option_wrapper_t<bool> locked{"autorotate-iio/lock_rotation"};

        wf::json_t js;
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

class tablet_plugin_t : public wf::per_output_plugin_instance_t
{
    wf::shared_data::ref_ptr_t<tablet_mode_t> tablet;
    std::unique_ptr<input_grab_t> input_grab;

    gesture_t reveal_gesture;
    gesture_t tap_to_close_gesture;
    gesture_t tap3_to_extra_btn;
    gesture_t double_tap2_to_side_btn;

    wf::wl_listener_wrapper needs_osk;

    wf::plugin_activation_data_t grab_interface = {
        .capabilities = CAPABILITY_GRAB_INPUT,
    };

  public:
    void init()
    {
        input_grab = std::make_unique<wf::input_grab_t>("tablet-mode", output, nullptr, nullptr, nullptr);

        // Setup initial gesture for amoxtli-panel
        on_output_config_changed.emit(NULL);
        output->connect(&on_output_config_changed);
        // Setup a gesture for closing amoxtli-panel by clicking everywhere else on the screen.
        tap_to_close_gesture = wf::touch::gesture_builder_t()
            .action(touch_action_t(1, true))
            .action(touch_action_t(1, false)
                .set_move_tolerance(5)
                .set_duration(100))
            .on_completed(close_panel)
            .build();
        wf::get_core().add_touch_gesture(&tap_to_close_gesture);

        // Connect signals necessary for the panel
        output->connect(&on_mapped);
        output->connect(&on_unmapped);

        // Setup a gesture to emulate BTN_EXTRA by three-finger tap (needed for mypaint undo shortcut)
        tap3_to_extra_btn = wf::touch::gesture_builder_t()
            .action(touch_action_t(3, true)
                .set_duration(300)
                .set_move_tolerance(25))
            .action(touch_action_t(3, false)
                .set_duration(300)
                .set_move_tolerance(25))
            .on_completed(emulate_btn_extra)
            .build();
        wf::get_core().add_touch_gesture(&tap3_to_extra_btn);

        // Setup a gesture to emulate BTN_SIDE by two finger double tap
        double_tap2_to_side_btn = wf::touch::gesture_builder_t()
            .action(touch_action_t(2, true)
                .set_duration(400)
                .set_move_tolerance(25))
            .action(touch_action_t(2, false)
                .set_duration(400)
                .set_move_tolerance(25))
            .action(touch_action_t(2, true)
                .set_duration(400)
                .set_move_tolerance(25))
            .action(touch_action_t(2, false)
                .set_duration(400)
                .set_move_tolerance(25))
            .on_completed(emulate_btn_side)
            .build();
        wf::get_core().add_touch_gesture(&double_tap2_to_side_btn);

        wf::get_core().connect(&on_tablet_proximity);
    }

    void fini()
    {
        wf::get_core().rem_touch_gesture(&reveal_gesture);
        wf::get_core().rem_touch_gesture(&tap_to_close_gesture);
        wf::get_core().rem_touch_gesture(&tap3_to_extra_btn);
        wf::get_core().rem_touch_gesture(&double_tap2_to_side_btn);
    }

    wf::signal::connection_t<output_configuration_changed_signal> on_output_config_changed =
        [=] (output_configuration_changed_signal*)
    {
        auto og = output->get_layout_geometry();
        touch_target_t target;

        target.x = og.x;
        target.y = og.y + og.height - 20;
        target.width = og.width;
        target.height = 20;


        // Setup a gesture for opening amoxtli-panel.
        // The user needs to swipe from the top edge.
        wf::get_core().rem_touch_gesture(&reveal_gesture);

        this->reveal_gesture = wf::touch::gesture_builder_t()
            .action(touch_action_t(1, true)
                .set_target(target))
            .action(drag_action_t(MOVE_DIRECTION_UP, 50))
            .action(reveal_action_t(drag_started, drag_continues))
            .on_completed(drag_ended)
            .build();
        wf::get_core().add_touch_gesture(&reveal_gesture);
    };

    wf::animation::simple_animation_t panel_dropdown{wf::create_option(300)};
    wayfire_toplevel_view panel;
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

        wf::geometry_t g = panel->get_pending_geometry();
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
            return panel->get_pending_geometry().height;
        }
    }

    void start_animation(int y)
    {
        if (!panel)
        {
            return;
        }

        int min_y = output->get_screen_size().height - panel->get_pending_geometry().height - 50;
        panel_dropdown.animate(std::max(y, min_y));

        output->render->rem_effect(&animate_panel);
        output->render->add_effect(&animate_panel, OUTPUT_EFFECT_PRE);
        output->render->schedule_redraw();
    }

    wf::signal::connection_t<view_mapped_signal> on_mapped = [=] (view_mapped_signal *ev)
    {
        if (ev->view->get_app_id() == "amoxtli.panel")
        {
            close_panel_on_animation_done = false;
            panel = toplevel_cast(ev->view);

            panel->get_wset()->remove_view(panel);
            wf::scene::readd_front(output->node_for_layer(wf::scene::layer::OVERLAY), panel->get_root_node());

            // Initially, the panel should be hidden
            auto y = output->get_screen_size().height;
            panel_dropdown.set(y, y);
            start_animation(autopick_y());
        }
    };

    wf::signal::connection_t<view_unmapped_signal> on_unmapped = [=] (view_unmapped_signal *ev)
    {
        if (ev->view == panel)
        {
            panel = nullptr;
        }
    };

    std::function<void()> drag_started = [=] ()
    {
        input_grab->grab_input(wf::scene::layer::OVERLAY);
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
        input_grab->ungrab_input();
        if (panel)
        {
            start_animation(panel->get_pending_geometry().height);
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

    void emulate_press(wf::scene::node_ptr focus, uint32_t btn)
    {
        if (!focus) return;
        focus->pointer_interaction().handle_pointer_enter({10, 10});
        auto seat = wf::get_core().get_current_seat();

        wlr_seat_pointer_notify_button(seat, get_current_time(), btn, WL_POINTER_BUTTON_STATE_PRESSED);
        wlr_seat_pointer_notify_frame(seat);
        wlr_seat_pointer_notify_button(seat, get_current_time(), btn, WL_POINTER_BUTTON_STATE_RELEASED);
        wlr_seat_pointer_notify_frame(seat);

        if (focus != wf::get_core().get_cursor_focus())
        {
            focus->pointer_interaction().handle_pointer_leave();
        }
    }

    wf::scene::node_ptr find_focus()
    {
        auto fingers = wf::get_core().get_touch_state();
        if (fingers.fingers.empty())
        {
            return nullptr;
        }

        int id = fingers.fingers.begin()->first;
        return wf::get_core().get_touch_focus(id);
    }

    std::function<void()> emulate_btn_extra = [=] ()
    {
        emulate_press(find_focus(), BTN_EXTRA);
    };

    std::function<void()> emulate_btn_side = [=] ()
    {
        emulate_press(find_focus(), BTN_SIDE);
    };

    void set_touch_state(bool state)
    {
        for (auto dev : wf::get_core().get_input_devices())
        {
            if (dev->get_wlr_handle()->type == WLR_INPUT_DEVICE_TOUCH)
            {
                dev->set_enabled(state);
            }
        }
    }

    wf::wl_timer<false> timer_reenable_touch;
    wf::option_wrapper_t<int> reenable_timeout{"tablet-mode/touch_reenable_timeout"};

    wf::signal::connection_t<wf::input_event_signal<wlr_tablet_tool_proximity_event>> on_tablet_proximity =
        [=] (wf::input_event_signal<wlr_tablet_tool_proximity_event> *ev)
    {
        if (ev->event->state == WLR_TABLET_TOOL_PROXIMITY_OUT)
        {
            timer_reenable_touch.set_timeout(reenable_timeout, [=] ()
            {
                set_touch_state(true);
            });
        } else
        {
            timer_reenable_touch.disconnect();
            set_touch_state(false);
        }
    };
};
}

DECLARE_WAYFIRE_PLUGIN(wf::per_output_plugin_t<wf::tablet_plugin_t>);
