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

#include "./xhc-whb04b6.h"

// system includes
#include <assert.h>
#include <iostream>
#include <iomanip>
#include <libusb.h>
#include <bitset>

// 3rd party includes

// local library includes

// local includes

using std::endl;

namespace XhcWhb04b6 {

// ----------------------------------------------------------------------

void XhcWhb04b6Component::initHal()
{
    mHal.init(mMetaButtons, mKeyCodes);
}

// ----------------------------------------------------------------------

void XhcWhb04b6Component::teardownHal()
{
    hal_exit(mHal.getHalComponentId());
}

// ----------------------------------------------------------------------

const char* XhcWhb04b6Component::getName() const
{
    return mName;
}

// ----------------------------------------------------------------------

const char* XhcWhb04b6Component::getHalName() const
{
    return mHal.getHalComponentName();
}

// ----------------------------------------------------------------------

void XhcWhb04b6Component::printCrcDebug(const UsbInPackage& inPackage, const UsbOutPackageData& outPackageBuffer) const
{
    std::ios init(NULL);
    init.copyfmt(*mRxCout);
    *mRxCout << std::setfill('0') << std::hex;

    // calculate checksum on button released, jog dial or rotary button change
    if (inPackage.buttonKeyCode1 == mKeyCodes.Buttons.undefined.code)
    {
        bool isValid = (inPackage.crc == (inPackage.randomByte & outPackageBuffer.seed));

        if (isValid)
        {
            if (mIsCrcDebuggingEnabled)
            {
                *mRxCout << "crc   checksum OK";
            }
        }
        else
        {
            *mRxCout << "warn  checksum error" << endl;
        }
        mRxCout->copyfmt(init);
        return;
    }

    // TODO: implement this experimental code correctly
    // once CRC decode/encode is implemented, it can be applied on received usb data

    std::bitset<8> random(inPackage.randomByte), buttonKeyCode(inPackage.buttonKeyCode1), crc(inPackage.crc);
    std::bitset<8> delta(0);

    if (inPackage.randomByte > inPackage.crc)
    {
        delta = inPackage.randomByte - inPackage.crc;
    }
    else
    {
        delta = inPackage.crc - inPackage.randomByte;
    }
    delta                        = inPackage.randomByte - inPackage.crc;

    if (mIsCrcDebuggingEnabled)
    {
        *mRxCout << endl;
        *mRxCout << "0x key " << std::setw(8) << static_cast<unsigned short>(inPackage.buttonKeyCode1)
                 << " random " << std::setw(8) << static_cast<unsigned short>(inPackage.randomByte)
                 << " crc " << std::setw(8) << static_cast<unsigned short>(inPackage.crc)
                 << " delta " << std::setw(8) << static_cast<unsigned short>(delta.to_ulong()) << endl;

        *mRxCout << "0b key " << buttonKeyCode
                 << " random " << random
                 << " crc " << crc
                 << " delta " << delta << endl;
    }

    //! \brief On button pressed checksum calculation.
    //! Experimental: checksum generator not clear yet, but this is a good starting point.
    //! The implementation has several flaws, but works with seed 0xfe and 0xff (which is a bad seed).
    //! \sa UsbOutPackageData::seed
    //! The checksum implementation does not work reliable with other seeds.
    std::bitset<8> seed(outPackageBuffer.seed), nonSeed(~seed);
    std::bitset<8> nonSeedAndRandom(nonSeed & random);
    std::bitset<8> keyXorNonSeedAndRandom(buttonKeyCode ^ nonSeedAndRandom);
    uint16_t       expectedCrc   = static_cast<unsigned short>(inPackage.crc);
    uint16_t       calculatedCrc = static_cast<unsigned short>(0x00ff &
                                                               (random.to_ulong() - keyXorNonSeedAndRandom.to_ulong()));
    std::bitset<8> calculatedCrcBitset(calculatedCrc);
    bool           isValid       = (calculatedCrc == expectedCrc);

    if (mIsCrcDebuggingEnabled)
    {
        *mRxCout << endl
                 << "~seed                  " << nonSeed << endl
                 << "random                 " << random << endl
                 << "                       -------- &" << endl
                 << "~seed & random         " << nonSeedAndRandom << endl
                 << "key                    " << buttonKeyCode << endl
                 << "                       -------- ^" << endl
                 << "key ^ (~seed & random) " << keyXorNonSeedAndRandom
                 << " = calculated delta " << std::setw(2)
                 << static_cast<unsigned short>(keyXorNonSeedAndRandom.to_ulong())
                 << " vs " << std::setw(2) << static_cast<unsigned short>(delta.to_ulong())
                 << ((keyXorNonSeedAndRandom == delta) ? " OK" : " FAIL") << endl
                 << "calculated crc         " << calculatedCrcBitset << " " << std::setw(2) << calculatedCrc << " vs "
                 << std::setw(2)
                 << expectedCrc << ((isValid) ? "                    OK" : "                    FAIL")
                 << " (random - (key ^ (~seed & random))";
    }

    if (!isValid)
    {
        *mRxCout << "warn  checksum error";
    }

    //assert(calculatedCrc == expectedCrc);
    //assert(keyXorNonSeedAndRandom == delta);

    mRxCout->copyfmt(init);
}

// ----------------------------------------------------------------------

//! interprets data packages as delivered by the XHC WHB04B-6 device
void XhcWhb04b6Component::onInputDataReceived(const UsbInPackage& inPackage)
{
    *mRxCout << "in    ";
    printHexdump(inPackage);
    if (inPackage.rotaryButtonFeedKeyCode != KeyCodes::Feed.undefined.code)
    {
        std::ios init(NULL);
        init.copyfmt(*mRxCout);
        *mRxCout << " delta " << std::setfill(' ') << std::setw(2)
                 << static_cast<unsigned short>(inPackage.rotaryButtonFeedKeyCode);
        mRxCout->copyfmt(init);
    }
    else
    {
        *mRxCout << " delta NA";
    }
    *mRxCout << " => ";
    printInputData(inPackage);
    printCrcDebug(inPackage, mUsb.getOutputPackageData());
    *mRxCout << endl;

    uint8_t keyCode      = inPackage.buttonKeyCode1;
    uint8_t modifierCode = inPackage.buttonKeyCode2;

    // in case key code == undefined
    if (keyCode == KeyCodes::Buttons.undefined.code)
    {
        // swap codes
        keyCode      = modifierCode;
        modifierCode = KeyCodes::Buttons.undefined.code;
    }

    // in case key code == "fn" and modifier == defined
    if ((keyCode == KeyCodes::Buttons.function.code) &&
        (modifierCode != KeyCodes::Buttons.undefined.code))
    {
        // swap codes
        keyCode      = modifierCode;
        modifierCode = KeyCodes::Buttons.function.code;
    }

    // in case of key code == defined and key code != "fn" and modifier == defined and modifier != "fn"
    if ((keyCode != KeyCodes::Buttons.undefined.code) &&
        (modifierCode != KeyCodes::Buttons.undefined.code) &&
        (modifierCode != KeyCodes::Buttons.function.code))
    {
        // last key press overrules last but one key press
        keyCode      = modifierCode;
        modifierCode = KeyCodes::Buttons.undefined.code;
    }

    if (keyCode == KeyCodes::Buttons.undefined.code)
    {
        assert(modifierCode == KeyCodes::Buttons.undefined.code);
    }

    if (keyCode == KeyCodes::Buttons.function.code)
    {
        assert(modifierCode == KeyCodes::Buttons.undefined.code);
    }

    mPendant.processEvent(keyCode, modifierCode,
                          inPackage.rotaryButtonAxisKeyCode,
                          inPackage.rotaryButtonFeedKeyCode,
                          inPackage.stepCount);
}

// ----------------------------------------------------------------------

void XhcWhb04b6Component::initWhb()
{
    mIsRunning = true;
    mUsb.setIsRunning(true);
}

// ----------------------------------------------------------------------

void XhcWhb04b6Component::requestTermination(int signal)
{
    if (signal >= 0)
    {
        *mInitCout << "termination requested upon signal number " << signal << " ..." << endl;
    }
    else
    {
        *mInitCout << "termination requested ... " << endl;
    }
    mUsb.requestTermination();
    mIsRunning = false;
}

// ----------------------------------------------------------------------

bool XhcWhb04b6Component::isRunning() const
{
    return mIsRunning;
}

// ----------------------------------------------------------------------

XhcWhb04b6Component::XhcWhb04b6Component() :
    mName("XHC-WHB04B-6"),
    mHal(),
    mKeyCodes(),
    mMetaButtons{MetaButtonCodes(mKeyCodes.Buttons.reset, mKeyCodes.Buttons.undefined),
                 MetaButtonCodes(mKeyCodes.Buttons.reset, mKeyCodes.Buttons.function),
                 MetaButtonCodes(mKeyCodes.Buttons.stop, mKeyCodes.Buttons.undefined),
                 MetaButtonCodes(mKeyCodes.Buttons.stop, mKeyCodes.Buttons.function),
                 MetaButtonCodes(mKeyCodes.Buttons.start, mKeyCodes.Buttons.undefined),
                 MetaButtonCodes(mKeyCodes.Buttons.start, mKeyCodes.Buttons.function),
                 MetaButtonCodes(mKeyCodes.Buttons.feed_plus, mKeyCodes.Buttons.undefined),
                 MetaButtonCodes(mKeyCodes.Buttons.feed_plus, mKeyCodes.Buttons.function),
                 MetaButtonCodes(mKeyCodes.Buttons.feed_minus, mKeyCodes.Buttons.undefined),
                 MetaButtonCodes(mKeyCodes.Buttons.feed_minus, mKeyCodes.Buttons.function),
                 MetaButtonCodes(mKeyCodes.Buttons.spindle_plus, mKeyCodes.Buttons.undefined),
                 MetaButtonCodes(mKeyCodes.Buttons.spindle_plus, mKeyCodes.Buttons.function),
                 MetaButtonCodes(mKeyCodes.Buttons.spindle_minus, mKeyCodes.Buttons.undefined),
                 MetaButtonCodes(mKeyCodes.Buttons.spindle_minus, mKeyCodes.Buttons.function),
                 MetaButtonCodes(mKeyCodes.Buttons.machine_home, mKeyCodes.Buttons.undefined),
                 MetaButtonCodes(mKeyCodes.Buttons.machine_home, mKeyCodes.Buttons.function),
                 MetaButtonCodes(mKeyCodes.Buttons.safe_z, mKeyCodes.Buttons.undefined),
                 MetaButtonCodes(mKeyCodes.Buttons.safe_z, mKeyCodes.Buttons.function),
                 MetaButtonCodes(mKeyCodes.Buttons.workpiece_home, mKeyCodes.Buttons.undefined),
                 MetaButtonCodes(mKeyCodes.Buttons.workpiece_home, mKeyCodes.Buttons.function),
                 MetaButtonCodes(mKeyCodes.Buttons.spindle_on_off, mKeyCodes.Buttons.undefined),
                 MetaButtonCodes(mKeyCodes.Buttons.spindle_on_off, mKeyCodes.Buttons.function),
                 MetaButtonCodes(mKeyCodes.Buttons.function, mKeyCodes.Buttons.undefined),
                 MetaButtonCodes(mKeyCodes.Buttons.probe_z, mKeyCodes.Buttons.undefined),
                 MetaButtonCodes(mKeyCodes.Buttons.probe_z, mKeyCodes.Buttons.function),
                 MetaButtonCodes(mKeyCodes.Buttons.macro10, mKeyCodes.Buttons.undefined),
                 MetaButtonCodes(mKeyCodes.Buttons.macro10, mKeyCodes.Buttons.function),
                 MetaButtonCodes(mKeyCodes.Buttons.manual_pulse_generator, mKeyCodes.Buttons.undefined),
                 MetaButtonCodes(mKeyCodes.Buttons.manual_pulse_generator, mKeyCodes.Buttons.function),
                 MetaButtonCodes(mKeyCodes.Buttons.step_continuous, mKeyCodes.Buttons.undefined),
                 MetaButtonCodes(mKeyCodes.Buttons.step_continuous, mKeyCodes.Buttons.function),
        //! it is expected to terminate this array with the "undefined" software button
                 MetaButtonCodes(mKeyCodes.Buttons.undefined, mKeyCodes.Buttons.undefined)
    },
    mUsb(mName, *this, mHal),
    //mTxCout(&mDevNull),
    mRxCout(&mDevNull),
    //mKeyEventCout(&mDevNull),
    //mHalInitCout(&mDevNull),
    mInitCout(&mDevNull),
    packageReceivedEventReceiver(*this),
    mPendant(mHal, mUsb.getOutputPackageData())
{
    setSimulationMode(true);
    enableVerbosePendant(false);
    enableVerboseRx(false);
    enableVerboseTx(false);
    enableVerboseInit(false);
    setEnableVerboseKeyEvents(false);
    enableVerboseHal(false);
}

// ----------------------------------------------------------------------

XhcWhb04b6Component::~XhcWhb04b6Component()
{
}

// ----------------------------------------------------------------------

void XhcWhb04b6Component::updateDisplay()
{
    if (mIsRunning)
    {
        mPendant.updateDisplayData();
    }
    else
    {
        mPendant.clearDisplayData();
    }
    mUsb.sendDisplayData();
}

// ----------------------------------------------------------------------

void XhcWhb04b6Component::printPushButtonText(uint8_t keyCode, uint8_t modifierCode, std::ostream& out)
{
    std::ios init(NULL);
    init.copyfmt(out);
    int indent = 15;
    out << std::setfill(' ');

    // no key code
    if (keyCode == mKeyCodes.Buttons.undefined.code)
    {
        out << std::setw(indent) << "";
        return;
    }

    const KeyCode& buttonKeyCode = mKeyCodes.Buttons.getKeyCode(keyCode);

    // print key text
    if (modifierCode == mKeyCodes.Buttons.function.code)
    {
        out << std::setw(indent) << buttonKeyCode.altText;
    }
    else
    {
        out << std::setw(indent) << buttonKeyCode.text;
    }
    out.copyfmt(init);
}

// ----------------------------------------------------------------------

void XhcWhb04b6Component::printRotaryButtonText(const KeyCode* keyCodesBase, uint8_t keyCode, std::ostream& out)
{
    std::ios init(NULL);
    init.copyfmt(out);

    // find key code
    const KeyCode* buttonKeyCode = keyCodesBase;
    while (buttonKeyCode->code != 0)
    {
        if (buttonKeyCode->code == keyCode)
        {
            break;
        }
        buttonKeyCode++;
    }
    out << std::setw(5) << buttonKeyCode->text << "(" << std::setw(4) << buttonKeyCode->altText << ")";
    out.copyfmt(init);
}


// ----------------------------------------------------------------------

DisplayIndicator::DisplayIndicator() :
    asByte(0)
{
}

// ----------------------------------------------------------------------

void XhcWhb04b6Component::printInputData(const UsbInPackage& inPackage, std::ostream& out)
{
    std::ios init(NULL);
    init.copyfmt(out);

    out << "| " << std::setfill('0') << std::hex << std::setw(2) << static_cast<unsigned short>(inPackage.header)
        << " | " << std::setw(2)
        << static_cast<unsigned short>(inPackage.randomByte) << " | ";
    out.copyfmt(init);
    printPushButtonText(inPackage.buttonKeyCode1, inPackage.buttonKeyCode2, out);
    out << " | ";
    printPushButtonText(inPackage.buttonKeyCode2, inPackage.buttonKeyCode1, out);
    out << " | ";
    printRotaryButtonText((KeyCode*)&mKeyCodes.Feed, inPackage.rotaryButtonFeedKeyCode, out);
    out << " | ";
    printRotaryButtonText((KeyCode*)&mKeyCodes.Axis, inPackage.rotaryButtonAxisKeyCode, out);
    out << " | " << std::setfill(' ') << std::setw(3) << static_cast<short>(inPackage.stepCount) << " | " << std::hex
        << std::setfill('0')
        << std::setw(2) << static_cast<unsigned short>(inPackage.crc);

    out.copyfmt(init);
}


// ----------------------------------------------------------------------

void XhcWhb04b6Component::printHexdump(const UsbInPackage& inPackage, std::ostream& out)
{
    std::ios init(NULL);
    init.copyfmt(out);

    out << std::setfill('0') << std::hex << "0x" << std::setw(2) << static_cast<unsigned short>(inPackage.header) << " "
        << std::setw(2)
        << static_cast<unsigned short>(inPackage.randomByte) << " " << std::setw(2)
        << static_cast<unsigned short>(inPackage.buttonKeyCode1) << " " << std::setw(2)
        << static_cast<unsigned short>(inPackage.buttonKeyCode2) << " " << std::setw(2)
        << static_cast<unsigned short>(inPackage.rotaryButtonFeedKeyCode) << " " << std::setw(2)
        << static_cast<unsigned short>(inPackage.rotaryButtonAxisKeyCode) << " " << std::setw(2)
        << static_cast<unsigned short>(inPackage.stepCount & 0xff) << " " << std::setw(2)
        << static_cast<unsigned short>(inPackage.crc);
    out.copyfmt(init);
}

// ----------------------------------------------------------------------

int XhcWhb04b6Component::run()
{
    if (mHal.isSimulationModeEnabled())
    {
        *mInitCout << "init  starting in simulation mode" << endl;
    }

    bool isHalReady = false;
    initWhb();
    initHal();

    if (!mUsb.isWaitForPendantBeforeHalEnabled() && !mHal.isSimulationModeEnabled())
    {
        hal_ready(mHal.getHalComponentId());
        isHalReady = true;
    }

    while (isRunning())
    {
        mHal.setIsPendantConnected(false);

        initWhb();
        if (!mUsb.init())
        {
            return EXIT_FAILURE;
        }

        mHal.setIsPendantConnected(true);

        if (!isHalReady && !mHal.isSimulationModeEnabled())
        {
            hal_ready(mHal.getHalComponentId());
            isHalReady = true;
        }

        if (mUsb.isDeviceOpen())
        {
            *mInitCout << "init  enabling reception ...";
            if (!enableReceiveAsyncTransfer())
            {
                std::cerr << endl << "failed to enable reception" << endl;
                return EXIT_FAILURE;
            }
            *mInitCout << " ok" << endl;
        }
        process();
        teardownUsb();
    }
    teardownHal();

    return EXIT_SUCCESS;
}

// ----------------------------------------------------------------------

void XhcWhb04b6Component::linuxcncSimulate()
{
}

// ----------------------------------------------------------------------

bool XhcWhb04b6Component::enableReceiveAsyncTransfer()
{
    return mUsb.setupAsyncTransfer();
}

// ----------------------------------------------------------------------

void XhcWhb04b6Component::setSimulationMode(bool enableSimulationMode)
{
    mIsSimulationMode = enableSimulationMode;
    mHal.setSimulationMode(mIsSimulationMode);
    mUsb.setSimulationMode(mIsSimulationMode);
}

// ----------------------------------------------------------------------


void XhcWhb04b6Component::setUsbContext(libusb_context* context)
{
    mUsb.setContext(context);
}

// ----------------------------------------------------------------------

libusb_device_handle* XhcWhb04b6Component::getUsbDeviceHandle()
{
    return mUsb.getDeviceHandle();
}

// ----------------------------------------------------------------------

libusb_context* XhcWhb04b6Component::getUsbContext()
{
    return mUsb.getContext();
}

// ----------------------------------------------------------------------

void XhcWhb04b6Component::process()
{
    if (mUsb.isDeviceOpen())
    {
        while (isRunning() && !mUsb.getDoReconnect())
        {
            struct timeval timeout;
            timeout.tv_sec  = 0;
            timeout.tv_usec = 200 * 1000;

            int r = libusb_handle_events_timeout_completed(getUsbContext(), &timeout, nullptr);
            assert((r == LIBUSB_SUCCESS) || (r == LIBUSB_ERROR_NO_DEVICE) || (r == LIBUSB_ERROR_BUSY) ||
                   (r == LIBUSB_ERROR_TIMEOUT) || (r == LIBUSB_ERROR_INTERRUPTED));
            if (mHal.isSimulationModeEnabled())
            {
                linuxcncSimulate();
            }
            updateDisplay();
        }
        updateDisplay();

        mHal.setIsPendantConnected(false);
        *mInitCout << "connection lost, cleaning up" << endl;
        struct timeval tv;
        tv.tv_sec  = 1;
        tv.tv_usec = 0;
        int r = libusb_handle_events_timeout_completed(getUsbContext(), &tv, nullptr);
        assert(0 == r);
        r = libusb_release_interface(getUsbDeviceHandle(), 0);
        assert((0 == r) || (r == LIBUSB_ERROR_NO_DEVICE));
        libusb_close(getUsbDeviceHandle());
        mUsb.setDeviceHandle(nullptr);
    }
}

// ----------------------------------------------------------------------

void XhcWhb04b6Component::teardownUsb()
{
    libusb_exit(getUsbContext());
    mUsb.setContext(nullptr);
}

// ----------------------------------------------------------------------

void XhcWhb04b6Component::enableVerbosePendant(bool enable)
{
    mPendant.enableVerbose(enable);
}

// ----------------------------------------------------------------------

void XhcWhb04b6Component::enableVerboseRx(bool enable)
{
    mUsb.enableVerboseRx(enable);
    if (enable)
    {
        mRxCout = &std::cout;
    }
    else
    {
        mRxCout = &mDevNull;
    }
}

// ----------------------------------------------------------------------

void XhcWhb04b6Component::enableVerboseTx(bool enable)
{
    mUsb.enableVerboseTx(enable);
    /*if (enable)
    {
        mTxCout = &std::cout;
    }
    else
    {
        mTxCout = &mDevNull;
    }*/
}

// ----------------------------------------------------------------------

void XhcWhb04b6Component::enableVerboseHal(bool enable)
{
    mHal.enableVerbose(enable);
    /*if (enable)
    {
        mHalInitCout = &std::cout;
    }
    else
    {
        mHalInitCout = &mDevNull;
    }*/
}

// ----------------------------------------------------------------------

void XhcWhb04b6Component::enableVerboseInit(bool enable)
{
    mUsb.enableVerboseInit(enable);
    if (enable)
    {
        mInitCout = &std::cout;
    }
    else
    {
        mInitCout = &mDevNull;
    }
}

// ----------------------------------------------------------------------

void XhcWhb04b6Component::printPushButtonText(uint8_t keyCode, uint8_t modifierCode)
{
    printPushButtonText(keyCode, modifierCode, *mRxCout);
}

// ----------------------------------------------------------------------

void XhcWhb04b6Component::printRotaryButtonText(const KeyCode* keyCodesBase, uint8_t keyCode)
{
    printRotaryButtonText(keyCodesBase, keyCode, *mRxCout);
}

// ----------------------------------------------------------------------

void XhcWhb04b6Component::printInputData(const UsbInPackage& inPackage)
{
    printInputData(inPackage, *mRxCout);
}

// ----------------------------------------------------------------------

void XhcWhb04b6Component::printHexdump(const UsbInPackage& inPackage)
{
    printHexdump(inPackage, *mRxCout);
}

// ----------------------------------------------------------------------

void XhcWhb04b6Component::setWaitWithTimeout(uint8_t waitSecs)
{
    mUsb.setWaitWithTimeout(waitSecs);
}

// ----------------------------------------------------------------------

bool XhcWhb04b6Component::isSimulationModeEnabled() const
{
    return mIsSimulationMode;
}

// ----------------------------------------------------------------------

void XhcWhb04b6Component::setEnableVerboseKeyEvents(bool enable)
{
    mUsb.enableVerboseRx(enable);
    /*if (enable)
    {
        mKeyEventCout = &std::cout;
    }
    else
    {
        mKeyEventCout = &mDevNull;
    }*/
}

// ----------------------------------------------------------------------

void XhcWhb04b6Component::enableCrcDebugging(bool enable)
{
    mIsCrcDebuggingEnabled = enable;
}
}
