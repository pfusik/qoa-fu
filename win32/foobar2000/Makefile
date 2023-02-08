VERSION = 0.1.0
FOOBAR2000_SDK_DIR = ../../../foobar2000_SDK

foo_qoa-$(VERSION).fb2k-component: foo_qoa.dll
	7z a -mx=9 -bd -bso0 -tzip $@ $^

foo_qoa.dll: foo_qoa.cpp ../../transpiled/QOA.cpp $(FOOBAR2000_SDK_DIR)/foobar2000/foobar2000_component_client/component_client.cpp \
	$(addprefix $(FOOBAR2000_SDK_DIR)/foobar2000/SDK/, abort_callback.cpp album_art.cpp audio_chunk.cpp audio_chunk_channel_config.cpp cfg_var_legacy.cpp commonObjects.cpp console.cpp file_info.cpp file_info_impl.cpp filesystem.cpp filesystem_helper.cpp fsItem.cpp guids.cpp input.cpp main_thread_callback.cpp playable_location.cpp replaygain_info.cpp service.cpp) \
	$(addprefix $(FOOBAR2000_SDK_DIR)/pfc/, audio_math.cpp audio_sample.cpp bit_array.cpp other.cpp pathUtils.cpp pfc-fb2k-hooks.cpp sort.cpp splitString2.cpp string-compare.cpp string-conv-lite.cpp string-lite.cpp string_base.cpp string_conv.cpp timers.cpp unicode-normalize.cpp utf8.cpp win-objects.cpp)
	cl -nologo -LD -Fe$@ -std:c++17 -W3 -DNDEBUG -DUNICODE -D_UNICODE -I../../transpiled -I$(FOOBAR2000_SDK_DIR) -I$(FOOBAR2000_SDK_DIR)/foobar2000 -EHsc -O2 -GR- -GS- $^ $(FOOBAR2000_SDK_DIR)/foobar2000/shared/shared-Win32.lib shell32.lib user32.lib -link -release -noexp -noimplib
