/*
   XHC-WHB04B-6 Wireless MPG pendant LinuxCNC HAL module for LinuxCNC.
   Based on XHC-HB04.

   Copyright (C) 2017 Raoul Rubien (github.com/rubienr).

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the program; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.
 */

// system includes
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <signal.h>

// 3rd party includes
#include <google/protobuf/stubs/common.h>

// local library includes

// local includes
#include "./xhc-whb04b6.h"

using std::endl;

// forward declarations

// globals
//! link object for signal handler
XhcWhb04b6::XhcWhb04b6Component* WhbComponent{nullptr};

// ----------------------------------------------------------------------

static int printUsage(const char* programName, const char* deviceName, bool isError = false)
{
    std::ostream* os = &std::cout;
    if (isError)
    {
        os = &std::cerr;
    }
    *os << programName << " version " << PACKAGE_VERSION << " " << __DATE__ << " " << __TIME__ << endl
        << endl
        << "SYNOPSIS" << endl
        << "    " << programName << " [-h | --help] | [-H] [OPTIONS] " << endl
        << endl
        << "NAME" << endl
        << "    " << programName << " - jog dial HAL component for the " << deviceName << " device" << endl
        << endl
        << "DESCRIPTION" << endl
        << "    " << programName << " is a HAL component that receives events from the " << deviceName << " device "
        << "and exposes them to HAL via HAL pins." << endl
        << endl
        << "OPTIONS" << endl
        << " -h, --help" << endl
        << "    Prints the synopsis and the most commonly used commands." << endl
        << endl
        << " -H " << endl
        << "    Run " << programName << " in HAL-mode instead of interactive mode. When in HAL mode "
        << "commands from device will be exposed to HAL's shred memory. Interactive mode is useful for "
        << "testing device connectivity and debugging." << endl
        << endl
        << " -t" << endl
        << "    Wait with timeout for USB device then proceed, exit otherwise. Without -t the timeout is "
        << "implicitly infinite." << endl
        << endl
        << " -u, -U" << endl
        << "    Show received data from device. With -U received and transmitted data will be printed. "
        << "Output is prefixed with \"usb\"." << endl
        << endl
        << " -p" << endl
        << "    Show HAL pins and HAL related messages. Output is prefixed with \"hal\"." << endl
        << endl
        << " -e" << endl
        << "    Show captured events such as button pressed/released, jog dial, axis rotary button, and "
            "feed rotary button event. Output is prefixed with \"event\"." << endl
        << endl
        << " -a" << endl
        << "    Enable all logging facilities without explicitly specifying each." << endl
        //! this feature must be removed when checksum check is implemented
        << endl
        << " -c" << endl
        << "    Enable checksum output which is necessary for debugging the checksum generator function. Do not rely "
            "on this feature since it will be removed once the generator is implemented." << endl
        << endl
        << " -n " << endl
        << "    Force being silent and not printing any output except of errors. This will also inhibit messages "
            "prefixed with \"init\"." << endl
        << endl
        << "EXAMPLES" << endl
        << programName << " -ue" << endl
        << "    Prints incoming USB data transfer and generated key pressed/released events." << endl
        << endl
        << programName << " -p" << endl
        << "    Prints hal pin names and events distributed to HAL memory." << endl
        << endl
        << programName << " -Ha" << endl
        << "    Start in HAL mode and avoid output, except of errors." << endl
        << endl
        << "AUTHORS" << endl
        << "    This component was started by Raoul Rubien (github.com/rubienr) based on predecessor "
            "device's component xhc-hb04.cc. https://github.com/machinekit/machinekit/graphs/contributors "
            "gives you a more complete list of contributors."
        << endl;

    if (isError)
    {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

// ----------------------------------------------------------------------
//! called on program termination requested
static void quit(int signal)
{
    if (WhbComponent != nullptr)
    {
        WhbComponent->requestTermination(signal);
    }
}

// ----------------------------------------------------------------------

//! registers signal handler
void registerSignalHandler()
{
    signal(SIGINT, quit);
    signal(SIGTERM, quit);
}

// ----------------------------------------------------------------------

bool parseFloat(const char* str, float& out)
{
    std::istringstream iss(str);
    if (!(iss >> out))
    {
        std::cerr << "no valid value specified: " << str << endl;
        return false;
    }
    return true;
}

// ----------------------------------------------------------------------

int main(int argc, char** argv)
{
    WhbComponent = new XhcWhb04b6::XhcWhb04b6Component();

    const char* optargs = "phaeHuctnUs:v:";
    for (int opt = getopt(argc, argv, optargs); opt != -1; opt = getopt(argc, argv, optargs))
    {
        switch (opt)
        {
            case 'H':
                WhbComponent->setSimulationMode(false);
                break;
            case 't':
                WhbComponent->setWaitWithTimeout(3);
                break;
            case 'e':
                WhbComponent->setEnableVerboseKeyEvents(true);
                break;
            case 'u':
                WhbComponent->enableVerboseInit(true);
                WhbComponent->enableVerboseRx(true);
                break;
            case 'U':
                WhbComponent->enableVerboseInit(true);
                WhbComponent->enableVerboseRx(true);
                WhbComponent->enableVerboseTx(true);
                break;
            case 'p':
                WhbComponent->enableVerboseInit(true);
                WhbComponent->enableVerboseHal(true);
                break;
            case 'a':
                WhbComponent->enableVerboseInit(true);
                WhbComponent->setEnableVerboseKeyEvents(true);
                WhbComponent->enableVerboseRx(true);
                WhbComponent->enableVerboseTx(true);
                WhbComponent->enableVerboseHal(true);
                break;
            case 'c':
                WhbComponent->enableCrcDebugging(true);
                break;
            case 'n':
                break;
            case 'h':
                return printUsage(basename(argv[0]), WhbComponent->getName());
                break;
            default:
                return printUsage(basename(argv[0]), WhbComponent->getName(), true);
                break;
        }
    }

    registerSignalHandler();

    WhbComponent->run();

    //! hotfix for https://github.com/machinekit/machinekit/issues/1266
    if (WhbComponent->isSimulationModeEnabled())
    {
        google::protobuf::ShutdownProtobufLibrary();
    }

    delete (WhbComponent);
    return 0;
}
