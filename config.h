#define PLUG_NAME "SFRR-Sampler"
#define PLUG_MFR "Stereokroma"
#define PLUG_VERSION_HEX 0x00010300
#define PLUG_VERSION_STR "1.3.0"
#define PLUG_UNIQUE_ID 'RbSp'
#define PLUG_MFR_ID 'Stkr'
#define PLUG_URL_STR "https://stereokroma.com"
#define PLUG_EMAIL_STR "jatomchuk@gmail.com"
#define PLUG_COPYRIGHT_STR "Copyright 2025 Stereokroma"
#define PLUG_CLASS_NAME IPlugInstrument

#define BUNDLE_NAME "IPlugInstrument"
#define BUNDLE_MFR "Stereokroma"
#define BUNDLE_DOMAIN "com"

#define PLUG_CHANNEL_IO "0-2"
#define SHARED_RESOURCES_SUBPATH "IPlugInstrument"

#define PLUG_LATENCY 0
#define PLUG_TYPE 1
#define PLUG_DOES_MIDI_IN 1
#define PLUG_DOES_MIDI_OUT 1
#define PLUG_DOES_MPE 0
#define PLUG_DOES_STATE_CHUNKS 1
#define PLUG_HAS_UI 1
#define PLUG_WIDTH 980
#define PLUG_HEIGHT 540
#define PLUG_FPS 60
#define PLUG_SHARED_RESOURCES 0
#define PLUG_HOST_RESIZE 0

#define AUV2_ENTRY IPlugInstrument_Entry
#define AUV2_ENTRY_STR "IPlugInstrument_Entry"
#define AUV2_FACTORY IPlugInstrument_Factory
#define AUV2_VIEW_CLASS IPlugInstrument_View
#define AUV2_VIEW_CLASS_STR "IPlugInstrument_View"

#define AAX_TYPE_IDS 'IPI1', 'IPI2'
#define AAX_PLUG_MFR_STR "Stereokroma"
#define AAX_PLUG_NAME_STR "IPlugInstrument\nIPIS"
#define AAX_DOES_AUDIOSUITE 0
#define AAX_PLUG_CATEGORY_STR "Synth"

#define VST3_SUBCATEGORY "Instrument|Synth"
#define CLAP_MANUAL_URL "https://stereokroma.com"
#define CLAP_SUPPORT_URL "https://stereokroma.com"
#define CLAP_DESCRIPTION "Simple Fucking Round Robin Sampler"
#define CLAP_FEATURES "instrument"

#define APP_NUM_CHANNELS 2
#define APP_N_VECTOR_WAIT 0
#define APP_MULT 1
#define APP_COPY_AUV3 0
#define APP_SIGNAL_VECTOR_SIZE 64

#define ROBOTO_FN "Roboto-Regular.ttf"
#define STEREOKROMA_LOGO_FN "Stereokroma.svg"
