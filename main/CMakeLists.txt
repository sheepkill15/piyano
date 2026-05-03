idf_component_register(
    SRCS 
        "main.cpp"
        "Midi.cpp"
        "UsbMidi.cpp"
        "engine/SynthEngine.cpp"
        "engine/Sound.cpp"
        "engine/AudioContext.cpp"
        "instruments/ModularInstrument.cpp"
        "instruments/DrumKitInstrument.cpp"
        "synth/dsp/WaveTables.cpp"
        "workstation/Workstation.cpp"
    INCLUDE_DIRS 
        "."
        "engine"
        "instruments"
        "workstation"
    REQUIRES 
        driver
        usb
)

# Hot path: prioritize throughput over size on Xtensa.
target_compile_options(${COMPONENT_LIB} PRIVATE
    -O3
    -ffast-math
    -fno-math-errno
)
