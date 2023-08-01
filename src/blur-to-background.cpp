#include <wayfire/option-wrapper.hpp>
#include <wayfire/img.hpp>
#include <cstdlib>
#include <memory>
#include <wayfire/config/types.hpp>
#include <wayfire/plugin.hpp>
#include <wayfire/region.hpp>
#include <wayfire/signal-provider.hpp>
#include <wayfire/view.hpp>
#include <wayfire/matcher.hpp>
#include <wayfire/output.hpp>
#include <wayfire/view-transform.hpp>
#include <wayfire/signal-definitions.hpp>
#include <wayfire/bindings-repository.hpp>
#include <wayfire/output-layout.hpp>

#include "wayfire/plugins/blur/blur.hpp"
#include "wayfire/core.hpp"
#include "wayfire/debug.hpp"
#include "wayfire/geometry.hpp"
#include "wayfire/opengl.hpp"
#include "wayfire/scene-operations.hpp"
#include "wayfire/scene-render.hpp"
#include "wayfire/scene.hpp"

namespace wf
{
namespace ammen99
{
struct blurred_background_t : public wf::custom_data_t
{
  private:
    wf::output_t *output;
    std::vector<scene::render_instance_uptr> instances;
    wf::region_t cached_region;
    wf::signal::connection_t<scene::root_node_update_signal> on_root_updated = [=] (scene::root_node_update_signal *ev)
    {
        if (ev->flags & (scene::update_flag::CHILDREN_LIST | scene::update_flag::ENABLED))
        {
            prepare_render();
        }
    };

    wf::signal::connection_t<output_configuration_changed_signal> on_output_changed = [=] (auto)
    {
        prepare_render_target();
        prepare_render();
        cached_region |= output->get_relative_geometry();
    };

    void prepare_render_target()
    {
        background = output->render->get_target_framebuffer();
        int w = background.viewport_width;
        int h = background.viewport_height;
        background.reset();
        OpenGL::render_begin();
        background.allocate(w, h);
        background.bind();
        OpenGL::clear({1, 0, 1, 1});
        OpenGL::render_end();
    }

    void prepare_render()
    {
        instances.clear();
        auto node = output->node_for_layer(wf::scene::layer::BACKGROUND);
        node->gen_render_instances(instances, [=] (const wf::region_t& damage)
        {
            cached_region |= damage;
        }, output);

        wf::region_t visible = node->get_bounding_box();
        scene::compute_visibility_from_list(instances, output, visible, {0, 0});
    }

  public:
    wf::render_target_t background;
    std::unique_ptr<wf_blur_base> blur_algo;

    ~blurred_background_t()
    {
        OpenGL::render_begin();
        background.release();
        OpenGL::render_end();
    }

    blurred_background_t(wf::output_t *output)
    {
        this->output = output;

        blur_algo = create_kawase_blur();
        prepare_render_target();
        prepare_render();
        wf::get_core().scene()->connect(&on_root_updated);
        output->connect(&on_output_changed);
    }

    void run_renderer()
    {
        if (cached_region.empty())
        {
            return;
        }

        cached_region.clear();

        background.geometry = output->get_layout_geometry();
        scene::render_pass_params_t params;
        params.background_color = {1, 1, 0, 1};
        params.damage = background.geometry;
        params.instances = &instances;
        params.target = background;
        params.reference_output = output;
        scene::run_render_pass(params, scene::RPASS_CLEAR_BACKGROUND);
        blur_algo->prepare_blur(background, background.geometry);
    }

    static blurred_background_t& get(wf::output_t *output)
    {
        if (!output->has_data<blurred_background_t>())
        {
            output->store_data(std::make_unique<blurred_background_t>(output));
        }

        return *output->get_data<blurred_background_t>();
    }
};

class background_blur_node_t : public scene::floating_inner_node_t
{
  public:
    std::weak_ptr<wf::view_interface_t> _view;

    background_blur_node_t(wayfire_view view) : floating_inner_node_t(false)
    {
        _view = view->weak_from_this();
    }

    std::string stringify() const override
    {
        return "background-blur " + stringify_flags();
    }

    void gen_render_instances(std::vector<scene::render_instance_uptr>& instances,
        scene::damage_callback push_damage, wf::output_t *shown_on) override;
};

class blur_render_instance_t : public scene::transformer_render_instance_t<background_blur_node_t>
{
  public:
    using transformer_render_instance_t::transformer_render_instance_t;

    bool should_report_opaque()
    {
        auto view = self->_view.lock();
        const auto& ch = self->get_children();
        return view && ch.size() == 1 && view->get_surface_root_node() == ch.front();
    }

    void schedule_instructions(std::vector<scene::render_instruction_t>& instructions,
        const wf::render_target_t& target, wf::region_t& damage) override
    {
        transformer_render_instance_t::schedule_instructions(instructions, target, damage);
        if (should_report_opaque())
        {
            damage ^= self->get_bounding_box();
        }
    }

    void compute_visibility(wf::output_t *output, wf::region_t& visible) override
    {
        transformer_render_instance_t::compute_visibility(output, visible);
        if (should_report_opaque())
        {
            visible ^= self->get_bounding_box();
        }
    }

    void passthrough_render(const wf::render_target_t& target, const wf::region_t& damage,
        const wf::texture_t& tex)
    {
        auto bbox = self->get_bounding_box();
        OpenGL::render_begin(target);
        for (auto& rect : damage)
        {
            target.logic_scissor(wlr_box_from_pixman_box(rect));
            OpenGL::render_texture(tex, target, bbox);
        }

        OpenGL::render_end();
    }

    void render(const wf::render_target_t& target, const wf::region_t& damage) override
    {
        auto tex = get_texture(target.scale);
        auto bounding_box = self->get_bounding_box();
        auto our_damage = damage & bounding_box;
        auto view = self->_view.lock();
        if (our_damage.empty() || !view || !view->get_output())
        {
            return passthrough_render(target, damage, tex);
        }

        auto& blur = blurred_background_t::get(view->get_output());

        if (wf::dimensions(target.geometry) != wf::dimensions(blur.background.geometry))
        {
            LOGE("Failed to render blur: different framebuffer sizes! Prepared is ", blur.background.geometry,
                " but we render to ", target.geometry);
            return passthrough_render(target, damage, tex);
        }

        blur.run_renderer();
        blur.background.geometry = target.geometry;
        blur.blur_algo->render(tex, bounding_box, our_damage, blur.background, target);
    }
};

void background_blur_node_t::gen_render_instances(std::vector<scene::render_instance_uptr>& instances,
    scene::damage_callback push_damage, wf::output_t *shown_on)
{
    auto uptr = std::make_unique<blur_render_instance_t>(this, push_damage, shown_on);
    if (uptr->has_instances())
    {
        instances.push_back(std::move(uptr));
    }
}
}
}

class background_blur_t : public wf::plugin_interface_t
{
  public:
    wf::signal::connection_t<wf::view_mapped_signal> on_view_mapped = [=] (wf::view_mapped_signal *ev)
    {
        if (enabled_for.matches(ev->view))
        {
            add_transformer(ev->view);
        }
    };

    wf::view_matcher_t enabled_for{"blur-to-background/enabled_for"};
    void add_transformer(wayfire_view view)
    {
        auto tmanager = view->get_transformed_node();
        if (tmanager->get_transformer<wf::ammen99::background_blur_node_t>())
        {
            return;
        }

        auto node = std::make_shared<wf::ammen99::background_blur_node_t>(view);
        tmanager->add_transformer(node, wf::TRANSFORMER_BLUR);
    }

    void pop_transformer(wayfire_view view)
    {
        auto tmanager = view->get_transformed_node();
        tmanager->rem_transformer<wf::ammen99::background_blur_node_t>();
    }

  public:
    void init() override
    {
        wf::get_core().connect(&on_view_mapped);
        for (auto& view : wf::get_core().get_all_views())
        {
            if (enabled_for.matches(view))
            {
                add_transformer(view);
            }
        }
    }

    void fini() override
    {
        for (auto& view : wf::get_core().get_all_views())
        {
            pop_transformer(view);
        }

        for (auto& wo : wf::get_core().output_layout->get_outputs())
        {
            wo->erase_data<wf::ammen99::blurred_background_t>();
        }
    }
};

DECLARE_WAYFIRE_PLUGIN(background_blur_t);
