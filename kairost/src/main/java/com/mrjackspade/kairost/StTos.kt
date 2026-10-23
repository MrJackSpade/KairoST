package com.mrjackspade.kairost

/**
 * Atari TOS images by model. A TOS image names its version in its header (the word after the
 * first branch), and each model ran its own versions: the ST 1.00 to 1.04, the STE 1.06, 1.62 and
 * 2.0x, the TT 3.0x and the Falcon 4.0x. EmuTOS images, which name a TOS version too, are marked
 * "ETOS" in their header and are not the player's TOS.
 */
object StTos {
    enum class Model(val file: String, val label: String, val description: String, val bytes: Long,
                     val versions: IntRange) {
        ST("tos-st.img", "ST", "TOS 1.00 to 1.04, for games on the ST and Mega ST", 192L * 1024, 0x0100..0x0104),
        STE("tos-ste.img", "STE", "TOS 1.06, 1.62 or 2.0x, for games on the STE and Mega STE", 256L * 1024, 0x0106..0x0206),
        TT("tos-tt.img", "TT", "TOS 3.0x, for games on the TT", 512L * 1024, 0x0300..0x03ff),
        FALCON("tos-falcon.img", "Falcon", "TOS 4.0x, for games on the Falcon", 512L * 1024, 0x0400..0x04ff)
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
    private val countries = listOf("US", "Germany", "France", "UK", "Spain", "Italy", "Sweden",
        "Switzerland, French", "Switzerland, German", "Turkey", "Finland", "Norway", "Denmark",
        "Saudi Arabia", "Netherlands", "Czech Republic", "Hungary")

    /** A name for an image, from its header: "TOS 1.04 (UK)", "TOS 4.04", "EmuTOS". */
    fun describe(header: ByteArray): String {
        if (header.size >= 0x30 && String(header, 0x2c, 4, Charsets.ISO_8859_1) == "ETOS") return "EmuTOS"
        val version = version(header)
        if (version == 0 || header.size < 0x1e) return "Unknown image"
        val name = "TOS %x.%02x".format(version shr 8, version and 0xff)
        // 0x7f: a multi-language TOS, which takes its language from NVRAM.
        val country = (header[0x1d].toInt() and 0xff) shr 1
        return countries.getOrNull(country)?.let { "$name ($it)" } ?: name
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
