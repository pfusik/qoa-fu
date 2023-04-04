VERSION = 0.2.0
FOOBAR2000_SDK_DIR = ../../../foobar2000_SDK

foo_qoa-$(VERSION).fb2k-component: foo_qoa.dll x64/foo_qoa.dll
	7z a -mx=9 -bd -bso0 -tzip $@ $^

foo_qoa.dll x64/foo_qoa.dll: foo_qoa.cpp ../../transpiled/QOA.cpp $(FOOBAR2000_SDK_DIR)/foobar2000/foobar2000_component_client/component_client.cpp \
	$(patsubst %, $(FOOBAR2000_SDK_DIR)/foobar2000/SDK/%.cpp, abort_callback album_art audio_chunk audio_chunk_channel_config cfg_var_legacy commonObjects console \
	file_info file_info_impl filesystem filesystem_helper fsItem guids input main_thread_callback playable_location replaygain_info service) \
	$(patsubst %, $(FOOBAR2000_SDK_DIR)/pfc/%.cpp, audio_math audio_sample bit_array other pathUtils pfc-fb2k-hooks sort splitString2 string-compare string-conv-lite string-lite string_base string_conv timers unicode-normalize utf8 win-objects)
	$(@D)/msvc.bat cl -nologo -LD -Fe$@ -Fo$(@D)/ -std:c++17 -W3 -DNDEBUG -DUNICODE -D_UNICODE -I../../transpiled -I$(FOOBAR2000_SDK_DIR) -I$(FOOBAR2000_SDK_DIR)/foobar2000 -EHsc -O2 -GR- -GS- $^ \
		$(FOOBAR2000_SDK_DIR)/foobar2000/shared/shared-$(@D:.=Win32).lib shell32.lib user32.lib -link -release -noexp -noimplib
