#include <memory>
#include <nlohmann/json_fwd.hpp>
#include <sstream>
#include <wayfire/config/option-types.hpp>
#include <wayfire/config/types.hpp>
#include <wayfire/core.hpp>
#include <wayfire/geometry.hpp>
#include <wayfire/opengl.hpp>
#include <wayfire/plugin.hpp>
#include <wayfire/plugins/ipc/ipc-method-repository.hpp>
#include <wayfire/plugins/ipc/ipc-helpers.hpp>
#include <wayfire/plugins/common/shared-core-data.hpp>
#include <wayfire/region.hpp>
#include <wayfire/scene-input.hpp>
#include <wayfire/scene-render.hpp>
#include <wayfire/util/log.hpp>
#include <wayfire/debug.hpp>
#include <wayfire/plugins/common/cairo-util.hpp>
#include <wayfire/scene-operations.hpp>
#include <iostream>
#include <deque>
#include <regex>

class logger_streambuf_t : public std::streambuf
{
  public:
    using callback_t = std::function<void(std::string)>;
    logger_streambuf_t(callback_t callback)
    {
        this->line_cb = callback;
    }

  protected:
    int_type overflow(int_type c) override
    {
        if (c != traits_type::eof())
        {
            buffer += traits_type::to_char_type(c);
        }

        if (c == '\n' || c == traits_type::eof())
        {
            if (c == '\n')
            {
                buffer.pop_back();
            }

            line_cb(buffer);
            buffer.clear();
        }

        return traits_type::not_eof(c);
    }

  private:
    callback_t line_cb;
    std::string buffer;
};

class output_log_overlay_t : public wf::scene::node_t
{
    class output_log_render_instance_t : public wf::scene::simple_render_instance_t<output_log_overlay_t>
    {
      public:
        using simple_render_instance_t::simple_render_instance_t;

        void render(const wf::render_target_t& target, const wf::region_t& region)
        {
            OpenGL::render_begin(target);

            auto g = wf::construct_box(wf::origin(self->get_bounding_box()), self->text.get_size());
            for (auto box : region)
            {
                target.logic_scissor(wlr_box_from_pixman_box(box));
                OpenGL::render_rectangle(g, wf::color_t{0.01, 0.01, 0.01, 0.1}, target.get_orthographic_projection());
                OpenGL::render_texture(self->text.tex.tex, target, g, glm::vec4(1.0f),
                    OpenGL::TEXTURE_TRANSFORM_INVERT_Y);
            }

            OpenGL::render_end();
        }
    };

    std::deque<std::string> lines;
    wf::cairo_text_t text;

  public:
    output_log_overlay_t() : node_t(false)
    {}

    void gen_render_instances(std::vector<wf::scene::render_instance_uptr>& instances,
        wf::scene::damage_callback push_damage, wf::output_t *output) override
    {
        instances.push_back(std::make_unique<output_log_render_instance_t>(this, push_damage, output));
    }

    wf::geometry_t get_bounding_box() override
    {
        auto outputs = wf::get_core().output_layout->get_outputs();
        if (outputs.empty())
        {
            return {0, 0, 1280, 720};
        } else
        {
            auto og = outputs.front()->get_layout_geometry();
            return {og.x, og.y, 1280, 720};
        }
    }

    void add_line(std::string line)
    {
        lines.push_back(line);

        const size_t max_lines = get_bounding_box().height / wf::cairo_text_t::measure_height(12);
        while (lines.size() > max_lines)
        {
            lines.pop_front();
        }

        std::string full_txt;
        for (auto& line : lines)
        {
            full_txt += line;
            full_txt += '\n';
        }

        wf::cairo_text_t::params params;
        params.text_color = wf::color_t{0.5, 0.5, 0.5, 1};
        params.font_size = 12;
        params.rounded_rect = false;
        params.bg_rect = false;
        params.max_size = wf::dimensions(get_bounding_box());

        text.render_text(full_txt, params);
        wf::scene::damage_node(this->shared_from_this(), get_bounding_box());
    }
};

class wayfire_ipc_debugger : public wf::plugin_interface_t
{
    std::regex filter{""};
    logger_streambuf_t::callback_t log_callback = [&] (std::string line)
    {
        if (std::regex_match(line, filter))
        {
            std::cout << line << std::endl;
            overlay->add_line(line);
        }
    };

    logger_streambuf_t logstream{log_callback};
    std::ostream logger{&logstream};

    std::shared_ptr<output_log_overlay_t> overlay;
    wf::shared_data::ref_ptr_t<wf::ipc::method_repository_t> repository;

  public:
    void init() override
    {
        nlohmann::json data;
        repository->register_method("ammen99/debug/filter", method_set_filter);
        repository->register_method("ammen99/debug/stop_log", method_stop_log);
        repository->register_method("ammen99/debug/scenedump", method_dump_scenegraph);
    }

    wf::ipc::method_callback method_set_filter = [=] (nlohmann::json data)
    {
        WFJSON_EXPECT_FIELD(data, "filter", string);
        this->filter = std::string(data["filter"]);

        // Add overlay
        if (!overlay)
        {
            overlay = std::make_shared<output_log_overlay_t>();
            wf::scene::add_front(wf::get_core().scene(), overlay);
        }

        // Enable all logs
        wf::log::enabled_categories.reset();
        wf::log::enabled_categories.flip();

        // Redirect to custom logging
        wf::log::initialize_logging(logger, wf::log::LOG_LEVEL_DEBUG, wf::log::LOG_COLOR_MODE_OFF);

        return wf::ipc::json_ok();
    };

    wf::ipc::method_callback method_stop_log = [=] (nlohmann::json)
    {
        // Stop logging
        wf::log::initialize_logging(std::cout, wf::log::LOG_LEVEL_INFO, wf::log::LOG_COLOR_MODE_OFF);
        wf::log::enabled_categories.reset();

        // Disable overlay
        if (overlay)
        {
            wf::scene::remove_child(overlay);
            overlay.reset();
        }

        return wf::ipc::json_ok();
    };

    nlohmann::json dump_scenegraph(wf::scene::node_ptr root)
    {
        nlohmann::json result = nlohmann::ordered_json::object();

        result["name"] = root->stringify();
        result["local-bbox"] = wf::ipc::geometry_to_json(root->get_bounding_box());

        std::stringstream ss;
        ss << root.get();
        result["id"] = ss.str();

        result["children"] = nlohmann::json::array();

        for (auto& ch : root->get_children())
        {
            result["children"].push_back(dump_scenegraph(ch));
        }

        return result;
    }

    wf::ipc::method_callback method_dump_scenegraph = [=] (nlohmann::json)
    {
        return dump_scenegraph(wf::get_core().scene());
    };
};

DECLARE_WAYFIRE_PLUGIN(wayfire_ipc_debugger);
