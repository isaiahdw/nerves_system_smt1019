################################################################################
#
# smt-audio-dsp — custom LADSPA plugin: high-pass + soft limiter for the 2 W
# built-in speaker (protects the woofer against over-excursion on bass and adds
# makeup-gain loudness). Loaded by the ALSA `type ladspa` PCM plugin (from
# alsa-plugins) via /etc/asound.conf, so every audio stream is filtered
# transparently with no per-player or application changes.
#
# The source lives in ./src and is built in-tree (SITE_METHOD = local), so the
# .so is compiled from source with the system toolchain — no checked-in binary.
#
################################################################################

SMT_AUDIO_DSP_VERSION = 1.1
SMT_AUDIO_DSP_SITE = $(SMT_AUDIO_DSP_PKGDIR)/src
SMT_AUDIO_DSP_SITE_METHOD = local
SMT_AUDIO_DSP_LICENSE = MIT

define SMT_AUDIO_DSP_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) -O2 -fPIC -shared -I$(@D) \
		-o $(@D)/smt_audio_dsp.so $(@D)/hpf_limiter.c -lm
endef

define SMT_AUDIO_DSP_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/smt_audio_dsp.so \
		$(TARGET_DIR)/usr/lib/ladspa/smt_audio_dsp.so
endef

$(eval $(generic-package))
