/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 Scott Moreau
 * Copyright (c) 2023 Ilia Bozhinov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <math.h>
#include <deque>
#include <numeric>
#include <wayfire/config/types.hpp>
#include <wayfire/geometry.hpp>
#include <wayfire/plugin.hpp>
#include <wayfire/output.hpp>
#include <wayfire/util.hpp>
#include <wayfire/render-manager.hpp>
#include <wayfire/per-output-plugin.hpp>
#include <wayfire/plugins/common/cairo-util.hpp>

class wayfire_bench_screen : public wf::per_output_plugin_instance_t
{
    wf::cairo_text_t text;

    uint64_t last_refresh_time = 0;
    std::deque<uint64_t> last_frame_times;
    wf::option_wrapper_t<double> average_frames{"ammen99-bench/average_frames"};
    wf::option_wrapper_t<double> refresh_interval{"ammen99-bench/refresh_interval"};

    wf::wl_timer<true> timer;

  public:
    void init() override
    {
        output->render->add_effect(&pre_hook, wf::OUTPUT_EFFECT_DAMAGE);
        output->render->add_effect(&overlay_hook, wf::OUTPUT_EFFECT_OVERLAY);

        // Ensure we render at least 1 fps
        timer.set_timeout(1000, [=] ()
        {
            output->render->damage(get_geometry());
            return true;
        });
    }

    wf::geometry_t get_geometry()
    {
        int x = output->get_screen_size().width / 2.0 - text.get_size().width / 2.0;
        int y = 30;
        return wf::construct_box({x, y}, text.get_size());
    }

    void compute_timing()
    {
        last_frame_times.push_back(wf::get_current_time());

        uint64_t current_time = wf::get_current_time();
        uint64_t oldest_allowed = current_time - 1000ll * average_frames;
        while (!last_frame_times.empty() && last_frame_times.front() < oldest_allowed)
        {
            last_frame_times.pop_front();
        }

        if (current_time - last_refresh_time >= refresh_interval * 1000)
        {
            render_bench(last_frame_times.size() / average_frames);
            last_refresh_time = current_time;
        }
    }

    void render_bench(double current_fps)
    {
        char fps_buf[128];
        sprintf(fps_buf, "fps: %.1f", current_fps);

        wf::cairo_text_t::params params;
        params.font_size = 32;
        params.bg_color = wf::color_t{0, 0, 0, 0.5};
        params.text_color = wf::color_t{0.8, 0.8, 0.8, 0.8};
        text.render_text(fps_buf, params);
    }

    wf::effect_hook_t pre_hook = [=] ()
    {
        if (!output->render->get_scheduled_damage().empty())
        {
            output->render->damage(get_geometry());
            compute_timing();
        }
    };

    wf::effect_hook_t overlay_hook = [=] ()
    {
        auto fb = output->render->get_target_framebuffer();
        OpenGL::render_begin(fb);
        OpenGL::render_transformed_texture(wf::texture_t{text.tex.tex},
            get_geometry(), fb.get_orthographic_projection(), glm::vec4(1.0),
            OpenGL::TEXTURE_TRANSFORM_INVERT_Y);
        OpenGL::render_end();
    };

    void fini() override
    {
        timer.disconnect();
        output->render->rem_effect(&pre_hook);
        output->render->rem_effect(&overlay_hook);
        output->render->damage_whole();
    }
};

DECLARE_WAYFIRE_PLUGIN(wf::per_output_plugin_t<wayfire_bench_screen>);
