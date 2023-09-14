#include <wayfire/core.hpp>
#include <wayfire/geometry.hpp>
#include <wayfire/opengl.hpp>
#include <wayfire/option-wrapper.hpp>
#include <wayfire/plugin.hpp>
#include <wayfire/region.hpp>
#include <wayfire/bindings-repository.hpp>
#include <wayfire/scene-operations.hpp>
#include <wayfire/signal-definitions.hpp>
#include <wayfire/signal-provider.hpp>

class cursor_overlay_t : public wf::scene::node_t
{
    class cursor_render_instance_t : public wf::scene::simple_render_instance_t<cursor_overlay_t>
    {
      public:
        using simple_render_instance_t::simple_render_instance_t;

        void render(const wf::render_target_t& target, const wf::region_t& region)
        {
            OpenGL::render_begin(target);

            for (auto box : region)
            {
                target.logic_scissor(wlr_box_from_pixman_box(box));
                OpenGL::render_rectangle(self->geometry,
                    wf::color_t{0.5, 0, 0, 0.5}, target.get_orthographic_projection());
            }

            OpenGL::render_end();
        }
    };

  public:
    cursor_overlay_t() : node_t(false)
    {}

    void gen_render_instances(std::vector<wf::scene::render_instance_uptr>& instances,
        wf::scene::damage_callback push_damage, wf::output_t *output) override
    {
        instances.push_back(std::make_unique<cursor_render_instance_t>(this, push_damage, output));
    }

    wf::geometry_t geometry = {0, 0, 10, 10};
    wf::geometry_t get_bounding_box() override
    {
        return geometry;
    }
};

class wayfire_show_cursor : public wf::plugin_interface_t
{
    wf::option_wrapper_t<bool> start_enabled{"show-cursor/start_enabled"};
    wf::option_wrapper_t<wf::activatorbinding_t> toggle{"show-cursor/toggle"};

    bool currently_enabled = true;
    std::shared_ptr<cursor_overlay_t> node;

    wf::activator_callback on_toggle = [=] (auto)
    {
        currently_enabled = !currently_enabled;
        if (currently_enabled)
        {
            enable();
        } else
        {
            disable();
        }

        return true;
    };

    void enable()
    {
        if (!node)
        {
            node = std::make_shared<cursor_overlay_t>();
        }

        wf::scene::readd_front(wf::get_core().scene(), node);
        update_position();
        wf::get_core().connect(&on_motion);
        wf::get_core().connect(&on_motion_abs);
    }

    void disable()
    {
        wf::scene::remove_child(node);
        on_motion.disconnect();
        on_motion_abs.disconnect();
    }

    void update_position()
    {
        auto gc = wf::get_core().get_cursor_position();
        if (node)
        {
            wf::scene::damage_node(node, node->get_bounding_box());
            node->geometry.x = gc.x - node->geometry.width / 2.0;
            node->geometry.y = gc.y - node->geometry.height / 2.0;
            wf::scene::damage_node(node, node->get_bounding_box());
        }
    }

    wf::signal::connection_t<wf::post_input_event_signal<wlr_pointer_motion_event>> on_motion = [&] (auto)
    {
        update_position();
    };

    wf::signal::connection_t<wf::post_input_event_signal<wlr_pointer_motion_absolute_event>> on_motion_abs =
        [&] (auto)
    {
        update_position();
    };

  public:
    void init() override
    {
        wf::get_core().bindings->add_activator(toggle, &on_toggle);
        currently_enabled = start_enabled;

        if (start_enabled)
        {
            enable();
        }
    }
};

DECLARE_WAYFIRE_PLUGIN(wayfire_show_cursor);
