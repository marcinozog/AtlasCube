// AT Command table for FSC-BT1026E / QCC5125 Bluetooth module
// Source: Feasycom UART Assistant shortcut list

char cmd[][40] = {

    // --- Device info ---
    "AT+GVER",          // Get firmware version
    "AT+GLBA",          // Get local Bluetooth MAC address
    "AT+GRBA",          // Get remote (phone) Bluetooth MAC address
    "AT+GLBN",          // Get local Bluetooth name
    "AT+GRBN",          // Get remote (phone) Bluetooth name
    "AT+SLBN=xxx",      // Set local Bluetooth name (example 1)
    "AT+SLBN=QCC5124EL",// Set local Bluetooth name (example 2)
    "AT+GPIN",          // Get pairing PIN
    "AT+SPIN=xxxxx",    // Set pairing PIN (example 1)
    "AT+SPIN=12345",    // Set pairing PIN (example 2)
    "AT+SPIN=0000",     // Disable pairing PIN
    "AT+UARTBAUD",      // Get UART baud rate

    // --- System control ---
    "AT+BOOT",          // Reboot device
    "AT+RES",           // Factory reset
    "AT+PWDS",          // Power off
    "AT+PWOS",          // Power on

    // --- Bluetooth connection ---
    "AT+IQ",            // Get Bluetooth state
    "AT+PR",            // Enter pairing mode
    "AT+AC",            // Reconnect
    "AT+DC",            // Disconnect
    "AT+FACT",          // Clear pairing list
    "AT+EDUT",          // DUT mode

    // --- Call control ---
    "AT+CA",            // Answer call
    "AT+CJ",            // Reject call
    "AT+CE",            // Hang up call
    "AT+CR",            // Redial last number
    "AT+MCALL",         // Query call function availability
    "AT+CALLOFF",       // Disable call function
    "AT+CALLON",        // Enable call function
    "AT+CALLCONF",      // Call ADK config

    // --- Playback control ---
    "AT+PP",            // Play / Pause
    "AT+PA",            // Play
    "AT+PU",            // Pause
    "AT+PN",            // Next track
    "AT+PV",            // Previous track
    "AT+VP",            // Volume up
    "AT+VD",            // Volume down
    "AT+CODE",          // Get current audio codec

    // --- Song metadata ---
    "AT+GMETA",         // Get song info (title/artist/album)
    "AT+GMAUTOM",       // Query song info auto-send mode
    "AT+SMAUTOFF",      // Disable auto song info
    "AT+SMAUTOI",       // Enable auto song info
    "AT+SMTIMEOFF",     // Disable song time display
    "AT+SMTIMEON",      // Enable song time display

    // --- Volume ---
    "AT+GVOL",          // Get current volume
    "AT+SVOL=0",        // Set volume to 0
    "AT+SVOL=10",       // Set volume to 10
    "AT+SVOL=15",       // Set volume to 15
    "AT+GVSTEP",        // Get volume step level
    "AT+SVSTEP=CONF",   // Set volume steps to ADK default (16)
    "AT+SVSTEP=32",     // Set volume steps to 32
    "AT+SVSTEP=64",     // Set volume steps to 64
    "AT+SVSTEP=128",    // Set volume steps to 128
    "AT+SYNCVOLSTATE",  // Get volume sync state
    "AT+SYNCVOLOFF",    // Disable volume sync
    "AT+SYNCVOLON",     // Enable volume sync

    // --- Audio output ---
    "AT+AOUT",          // Query current audio output mode
    "AT+SAOUT=DAC",     // Set output: analog (DAC)
    "AT+SAOUT=I2S",     // Set output: I2S
    "AT+SAOUT=SPDIF",   // Set output: S/PDIF
    "AT+SAOUT=CONF",    // Set output: ADK default
    "AT+SWAOUT",        // Dynamic switch audio output
    "AT+MAUD",          // Get AUDM status
    "AT+AUDM0",         // BT disconnect / pause AUX/USB
    "AT+AUDM1",         // I2S-MCLK continuous output
    "AT+AUDM2",         // BT disconnect / play AUX/USB

    // --- Sample rate ---
    "AT+GARATE",        // Get current sample rate
    "AT+SARATE=CONF",   // Set sample rate: ADK default
    "AT+SARATE=48K",    // Set sample rate: 48 kHz
    "AT+SARATE=96K",    // Set sample rate: 96 kHz
    "AT+SARATE=>48K",   // Set minimum sample rate: 48 kHz

    // --- Audio channel ---
    "AT+ACHAN",         // Query audio output channel
    "AT+SACHAN=MONO",   // Set output channel: mono
    "AT+SACHAN=STEREO", // Set output channel: stereo
    "AT+SACHAN=CONF",   // Set output channel: ADK default

    // --- Prompt tone ---
    "AT+MTONE",         // Query prompt tone state
    "AT+TONEON",        // Enable prompt tone
    "AT+TONEOFF",       // Disable prompt tone

    // --- EQ ---
    "AT+NEXTEQ",        // Switch to next EQ bank
    "AT+SEQBANKDEF=A",  // Restore all EQ banks to default
    "AT+SEQBANKDEF=1",  // Restore EQ bank 1 to default
    "AT+SEQBANKDEF=6",  // Restore EQ bank 6 to default

    // --- TWS ---
    "AT+TWSINQUIRE",    // TWS search (inquiry)
    "AT+TWSCONNDISC",   // TWS search / cancel search
    "AT+TWSPAIR",       // TWS search / be discoverable
    "AT+TWSEND",        // TWS disconnect
    "AT+DELTWS",        // Delete TWS pairing list
};