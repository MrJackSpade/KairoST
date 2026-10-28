package com.mrjackspade.kairost

import com.mrjackspade.kairo.frontend.UiText

/**
 * Atari TOS images by model. A TOS image names its version in its header (the word after the
 * first branch), and each model ran its own versions: the ST 1.00 to 1.04, the STE 1.06, 1.62 and
 * 2.0x, the TT 3.0x and the Falcon 4.0x. EmuTOS images, which name a TOS version too, are marked
 * "ETOS" in their header and are not the player's TOS.
 */
object StTos {
    enum class Model(val file: String, val label: String, val description: Int, val bytes: Long,
                     val versions: IntRange) {
        ST("tos-st.img", "ST", R.string.tos_st_description, 192L * 1024, 0x0100..0x0104),
        STE("tos-ste.img", "STE", R.string.tos_ste_description, 256L * 1024, 0x0106..0x0206),
        TT("tos-tt.img", "TT", R.string.tos_tt_description, 512L * 1024, 0x0300..0x03ff),
        FALCON("tos-falcon.img", "Falcon", R.string.tos_falcon_description, 512L * 1024, 0x0400..0x04ff)
    }

    /** The model a TOS image's header is for; null for anything else, EmuTOS included. */
    fun model(header: ByteArray): Model? {
        if (header.size < 0x30 || header[0] != 0x60.toByte()) return null
        if (String(header, 0x2c, 4, Charsets.ISO_8859_1) == "ETOS") return null
        val version = (header[2].toInt() and 0xff) shl 8 or (header[3].toInt() and 0xff)
        return Model.entries.firstOrNull { version in it.versions }
    }

    /**
     * How well a TOS image suits [model] when a folder holds several, null when it is not for it:
     * English first, since the on-screen keyboard has English legends and a German TOS, say, maps
     * the keys its own way; then the latest version. The header names the country at 0x1d (shifted
     * left one bit): 0 is the US, 3 the UK, and 0x7f a multi-language TOS (4.0x), whose language
     * comes from the NVRAM KairoST sets.
     */
    fun rank(header: ByteArray, model: Model): Int? {
        if (model(header) != model) return null
        val version = (header[2].toInt() and 0xff) shl 8 or (header[3].toInt() and 0xff)
        val country = (header[0x1d].toInt() and 0xff) shr 1
        return (if (country == 0 || country == 3 || country == 0x7f) 0x10000 else 0) + version
    }

    /** How much of an image [model], [version] and [describe] read. */
    const val HEADER_BYTES = 256

    /** The first [HEADER_BYTES] of [input], fewer when it is shorter. */
    fun readHeader(input: java.io.InputStream): ByteArray {
        val bytes = ByteArray(HEADER_BYTES)
        var count = 0
        while (count < bytes.size) {
            val read = input.read(bytes, count, bytes.size - count)
            if (read < 0) break
            count += read
        }
        return bytes.copyOf(count)
    }

    /** The version a TOS header names, such as 0x0104; 0 when it is not a TOS header. */
    fun version(header: ByteArray): Int =
        if (header.size < 4 || header[0] != 0x60.toByte()) 0
        else (header[2].toInt() and 0xff) shl 8 or (header[3].toInt() and 0xff)

    /** The countries TOS names in its header (0x1d, shifted left one bit). */
    private val countries = listOf(R.string.country_us, R.string.country_germany, R.string.country_france,
        R.string.country_uk, R.string.country_spain, R.string.country_italy, R.string.country_sweden,
        R.string.country_switzerland_french, R.string.country_switzerland_german, R.string.country_turkey,
        R.string.country_finland, R.string.country_norway, R.string.country_denmark, R.string.country_saudi_arabia,
        R.string.country_netherlands, R.string.country_czech_republic, R.string.country_hungary)

    /** A name for an image, from its header: "TOS 1.04 (UK)", "TOS 4.04", "EmuTOS". */
    fun describe(header: ByteArray): UiText {
        if (header.size >= 0x30 && String(header, 0x2c, 4, Charsets.ISO_8859_1) == "ETOS") return UiText.fixed("EmuTOS")
        val version = version(header)
        if (version == 0 || header.size < 0x1e) return UiText.res(R.string.tos_unknown_image)
        val name = "TOS %x.%02x".format(version shr 8, version and 0xff)
        // 0x7f: a multi-language TOS, which takes its language from NVRAM.
        val country = (header[0x1d].toInt() and 0xff) shr 1
        return countries.getOrNull(country)?.let { UiText.res(R.string.tos_with_country, name, UiText.res(it)) }
            ?: UiText.fixed(name)
    }

    /** The model of TOS a machine (StCatalog.machine) starts. */
    fun forMachine(machine: String): Model = when (machine) {
        "st", "megast" -> Model.ST
        "ste", "megaste" -> Model.STE
        "tt" -> Model.TT
        "falcon" -> Model.FALCON
        else -> error("No TOS model for machine $machine")
    }
}
