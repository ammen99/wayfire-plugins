#include <wayfire/view-transform.hpp>
#include <wayfire/opengl.hpp>
#include <wayfire/plugin.hpp>
#include <wayfire/signal-definitions.hpp>
#include <wayfire/util/log.hpp>

namespace
{
const std::string vertex_source = R"(
#version 100
attribute mediump vec2 position;
varying mediump vec2 fposition;

uniform mat4 matrix;

void main() {
    gl_Position = matrix * vec4(position, 0.0, 1.0);
    fposition = position;
})";

const std::string frag_source = R"(
#version 100
@builtin_ext@

varying mediump vec2 fposition;
@builtin@

// Top left corner
uniform mediump vec2 top_left;

// Top left corner with shadows included
uniform mediump vec2 full_top_left;

// Bottom right corner
uniform mediump vec2 bottom_right;

// Bottom right corner with shadows included
uniform mediump vec2 full_bottom_right;

// Rounding radius
const mediump float radius = 20.0;

void main()
{
    mediump vec2 corner_dist = min(fposition - top_left, bottom_right - fposition);
    if (max(corner_dist.x, corner_dist.y) < radius)
    {
        if (distance(corner_dist, vec2(radius, radius)) > radius)
        {
            discard;
        }
    }

    highp vec2 uv = (fposition - full_top_left) / (full_bottom_right - full_top_left);
    uv.y = 1.0 - uv.y;
    gl_FragColor = get_pixel(uv);
})";
}

class rounded_corners_transformer_t : public wf::view_transformer_t
{
  public:
    uint32_t get_z_order() override
    {
        return wf::TRANSFORMER_2D - 1;
    }

    wf::region_t transform_opaque_region(
        wf::geometry_t box, wf::region_t region) override
    {
        (void)box;
        (void)region;
        return {};
    }

    wf::pointf_t transform_point(
        wf::geometry_t view, wf::pointf_t point) override
    {
        (void)view;
        return point;
    }

    wf::pointf_t untransform_point(
        wf::geometry_t view, wf::pointf_t point) override
    {
        (void)view;
        return point;
    }

    wf::geometry_t get_bounding_box(wf::geometry_t bbox, wlr_box region) override
    {
        (void)bbox;
        return wf::geometry_intersection(this->view->get_wm_geometry(), region);
    }

    std::vector<GLfloat> vertex_data;
    void upload_data(wlr_box src_box)
    {
        auto geometry = view->get_wm_geometry();
        float x = geometry.x, y = geometry.y,
              w = geometry.width, h = geometry.height;

        vertex_data = {
            x, y + h,
            x + w, y + h,
            x + w, y,
            x, y,
        };
        program.attrib_pointer("position", 2, 0, vertex_data.data(), GL_FLOAT);

        program.uniform2f("top_left", x, y);
        program.uniform2f("bottom_right", x + w, y + h);
        program.uniform2f("full_top_left", src_box.x, src_box.y);
        program.uniform2f("full_bottom_right",
            src_box.x + src_box.width, src_box.y + src_box.height);
    }

    void render_with_damage(wf::texture_t src_tex, wlr_box src_box,
        const wf::region_t& damage, const wf::framebuffer_t& target_fb) override
    {
        OpenGL::render_begin(target_fb);

        program.use(src_tex.type);
        program.set_active_texture(src_tex);
        upload_data(src_box);
        program.uniformMatrix4f("matrix", target_fb.get_orthographic_projection());

        GL_CALL(glEnable(GL_BLEND));
        GL_CALL(glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
        for (const auto& box : damage)
        {
            target_fb.logic_scissor(wlr_box_from_pixman_box(box));
            GL_CALL(glDrawArrays(GL_TRIANGLE_FAN, 0, 4));
        }

        GL_CALL(glDisable(GL_BLEND));
        program.deactivate();
        OpenGL::render_end();
    }

    rounded_corners_transformer_t(wayfire_view view)
    {
        this->view = view;

        OpenGL::render_begin();
        program.compile(vertex_source, frag_source);
        OpenGL::render_end();
    }

    virtual ~rounded_corners_transformer_t()
    {}

  private:
    wayfire_view view;
    OpenGL::program_t program;
};

class wayfire_rounded_corners_t : public wf::plugin_interface_t
{
  public:
    void init() override
    {
        output->connect_signal("view-mapped", &on_map);
    }

    wf::signal_connection_t on_map = [=] (wf::signal_data_t *data)
    {
        auto view = wf::get_signaled_view(data);
        // Ignore panels, backgrounds, etc.
        if (view->role == wf::VIEW_ROLE_TOPLEVEL)
        {
            view->add_transformer(
                std::make_unique<rounded_corners_transformer_t>(view));
        }
    };
};

DECLARE_WAYFIRE_PLUGIN(wayfire_rounded_corners_t);
