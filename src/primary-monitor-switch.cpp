#include <wayfire/plugin.hpp>

class primary_monitor_switch_t : public wf::plugin_interface_t
{
  public:
    void init()
    {
        /* Create plugin */
    }

    void fini()
    {
        /* Destroy plugin */
    }
};

DECLARE_WAYFIRE_PLUGIN(primary_monitor_switch_t)
