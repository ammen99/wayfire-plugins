#include <wayfire/core.hpp>
#include <wayfire/geometry.hpp>
#include <wayfire/plugin.hpp>
#include <wayfire/plugins/ipc/ipc-method-repository.hpp>
#include <wayfire/plugins/ipc/ipc-helpers.hpp>
#include <wayfire/plugins/common/shared-core-data.hpp>
#include <wayfire/workspace-set.hpp>
#include <wayfire/util/log.hpp>
#include <wayfire/debug.hpp>
#include <wayfire/seat.hpp>

class ammen99_ipc_commands : public wf::plugin_interface_t
{
    wf::shared_data::ref_ptr_t<wf::ipc::method_repository_t> repository;
  public:
    void init() override
    {
        repository->register_method("ammen99/ipc/set_grid_size", method_set_grid_size);
    }

    wf::ipc::method_callback method_set_grid_size = [=] (const wf::json_t& data)
    {
        int width = wf::ipc::json_get_uint64(data, "width");
        int height = wf::ipc::json_get_uint64(data, "height");

        auto wo = wf::get_core().seat->get_active_output();
        if (wo)
        {
            wo->wset()->set_workspace_grid_size({width, height});
        }

        return wf::ipc::json_ok();
    };
};

DECLARE_WAYFIRE_PLUGIN(ammen99_ipc_commands);
