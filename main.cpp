
#include "daemonhandler.hpp"
//---------------------------------------------------------------------------------------------------------------------

int main()
{
    try
    {
        Utils::DaemonHandler dh;

        return dh.Run();
    }
    catch (...) { return 1; }

    return 0;
}
