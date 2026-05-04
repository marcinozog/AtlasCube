char cmd[][40] = {
    "AT+GVER",        // Get firmware version
    "AT+GRBN",        // Get remote Bluetooth device name
    "AT+GLBA",        // Get local Bluetooth MAC address
    "AT+GRBA",        // Get remote Bluetooth MAC address
    "AT+GLBN",        // Get local Bluetooth name

    "AT+SLBN=xxx",    // Set local Bluetooth name
    "AT+SPIN=xxxx",   // Set pairing PIN
    "AT+GPIN",        // Get pairing PIN
    "AT+SPIN=0000",   // Disable pairing PIN

    "AT+STATE",       // Get current Bluetooth state
    "AT+CODE",        // Get current audio codec

    "AT+PWDS",        // Soft power down
    "AT+PWOS",        // Soft power on
    "AT+BOOT",        // Reboot device

    "AT+FACT",        // Factory reset
    "AT+RES",         // Restore settings / reconnect

    "AT+GVOL",        // Get volume
    "AT+SVOL=0",      // Set volume (0)
    "AT+SVOL=11",     // Set volume (example mid level)
    "AT+SVOL=15",     // Set volume (max)

    "AT+SYNCVOLSTATE",// Get volume sync state
    "AT+SYNCVOLOFF",  // Disable volume sync
    "AT+SYNCVOLON",   // Enable volume sync

    "AT+JAUTBAUD",    // Auto baud rate detect

    "AT+MTONE",       // Get system tone state
    "AT+MTONEON",     // Enable system tones
    "AT+MTONEOFF",    // Disable system tones

    "AT+MCALL",       // Get call feature state
    "AT+CALLON",      // Enable call feature
    "AT+CALLOFF",     // Disable call feature

    "AT+PR",          // Enter pairing mode
    "AT+AC",          // Accept incoming call / connect
    "AT+DC",          // Disconnect
    "AT+CA",          // Answer call
    "AT+CH",          // Hang up / reject call
    "AT+CE",          // End call
    "AT+CR",          // Redial last number

    "AT+PP",          // Play/Pause
    "AT+PN",          // Next track
    "AT+PV",          // Previous track
    "AT+VD",          // Volume down
    "AT+VU",          // Volume up
    "AT+PA",          // Play
    "AT+PJ",          // Pause

    "AT+DELTWS",      // Delete TWS pairing
    "AT+TWSEND",      // Enable TWS
    "AT+TWSINQUIRE",  // Search for TWS devices
    "AT+TWSCONNDISC", // Connect/disconnect TWS

    "AT+SAOUT",       // Get audio output mode
    "AT+SAOUT=DAC",   // Set output to DAC (analog)
    "AT+SAOUT=I2S",   // Set output to I2S
    "AT+SAOUT=SPDIF", // Set output to SPDIF
    "AT+SAOUT=CONF",  // Configure audio output

    "AT+AUDMOD",      // Audio mode (BT / line-in)
    "AT+AUIN1",       // Route I2S to line-in
    "AT+AUIN2"        // Route Bluetooth to line-in
};