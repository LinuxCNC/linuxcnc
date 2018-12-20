/*
   Copyright (C) 2017 Raoul Rubien (github.com/rubienr)

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

#pragma once

// system includes
#include <stdint.h>

// 3rd party includes

// local includes
#include "./hal.h"
#include "./usb.h"
#include "./pendant.h"

// forward declarations

namespace XhcWhb04b6 {

// forward declarations

// ----------------------------------------------------------------------

//! The XHC WHB04B-6 user space component for Linuxcnc.
class XhcWhb04b6Component :
    public OnUsbInputPackageListener
{
public:
    XhcWhb04b6Component();
    virtual ~XhcWhb04b6Component();
    void process();
    void teardownUsb();
    void setUsbContext(libusb_context* context);
    libusb_device_handle* getUsbDeviceHandle();
    libusb_context* getUsbContext();
    //! \return the name as specified to \ref XhcWhb04b6Component
    const char* getName() const;
    //! \return the name as specified to \ref Hal
    const char* getHalName() const;
    //! callback method received by \ref Usb when a \ref libusb_transfer is received
    void onInputDataReceived(const UsbInPackage& inPackage) override;
    void initWhb();
    void initHal();
    void teardownHal();
    bool enableReceiveAsyncTransfer();
    void updateDisplay();
    void linuxcncSimulate();
    void requestTermination(int signal = -42);
    bool isRunning() const;
    int run();
    bool isSimulationModeEnabled() const;
    void setSimulationMode(bool enableSimulationMode);
    void setEnableVerboseKeyEvents(bool enable);
    void enableVerboseRx(bool enable);
    void enableVerboseTx(bool enable);
    void enableVerboseHal(bool enable);
    void enableVerboseInit(bool enable);
    void enableCrcDebugging(bool enable);
    void setWaitWithTimeout(uint8_t waitSecs = 3);
    void printCrcDebug(const UsbInPackage& inPackage, const UsbOutPackageData& outPackageBuffer) const;
    void offerHalMemory();

private:
    const char* mName;
    Hal                   mHal;
    const KeyCodes        mKeyCodes;
    const MetaButtonCodes mMetaButtons[32];
    Usb                   mUsb;
    bool                  mIsRunning{false};
    bool                  mIsSimulationMode{false};
    std::ostream          mDevNull{nullptr};
    std::ostream             * mTxCout;
    std::ostream             * mRxCout;
    std::ostream             * mKeyEventCout;
    std::ostream             * mHalInitCout;
    std::ostream             * mInitCout;
    OnUsbInputPackageListener& packageReceivedEventReceiver;
    bool    mIsCrcDebuggingEnabled{false};
    Pendant mPendant;

    //! prints human readable output of the push buttons state
    void printPushButtonText(uint8_t keyCode, uint8_t modifierCode, std::ostream& out);
    //! prints human readable output of the push buttons state to \ref verboseRxOut stream
    void printPushButtonText(uint8_t keyCode, uint8_t modifierCode);
    //! prints human readable output of the push buttons state to \p out
    void printRotaryButtonText(const KeyCode* keyCodesBase, uint8_t keyCode, std::ostream& out);
    //! prints human readable output of the rotary button text to \ref verboseRxOut stream
    void printRotaryButtonText(const KeyCode* keyCodesBase, uint8_t keyCode);
    //! prints human readable output of the rotary button text to \p out
    void printInputData(const UsbInPackage& inPackage, std::ostream& out);
    //! prints human readable output of the input package to \ref verboseRxOut stream
    void printInputData(const UsbInPackage& inPackage);
    //! prints human readable output of the input package to \p out
    void printHexdump(const UsbInPackage& inPackage, std::ostream& out);
    //! prints a hexdump of the input package to \ref verboseRxOut stream
    void printHexdump(const UsbInPackage& inPackage);
};
}
