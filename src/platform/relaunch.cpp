#include "platform/relaunch.hpp"

#include <switch.h>

bool scheduleReturn(const LaunchArgs& args) {
    if (!envHasNextLoad()) return false;
    const std::string cmd = buildReturnCommand(args);
    return R_SUCCEEDED(envSetNextLoad(sanitizeArg(args.returnPath).c_str(), cmd.c_str()));
}
